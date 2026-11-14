// SPDX-License-Identifier: GPL-3.0-or-later
/* Fast variant of live-display.c (same protocol, refresh policy, LUT and
 * restore path):
 *  - reads the virtual framebuffer through mmap instead of an 8.5 MB pread;
 *    the state is still re-checked afterwards and torn frames are dropped;
 *  - computes luma with NEON using the identical integer formula;
 *  - submits only the changed rectangle for partial (non-flashing) updates,
 *    unless fast/partial-disabled exists. Full refreshes stay full-screen;
 *  - waits up to 5 s for both KOReader processes to be idle before pausing;
 *  - skips a new frame whose RGBA signature equals the last converted frame's
 *    before the LUT and diff (WeRead redraws identical pages for ~20 s after
 *    opening a book and once more after each page turn), unless
 *    resident-v197/bridgesame-disabled exists. Same outcome as before: such a
 *    frame always ended with no changed rectangle.
 */
#define _GNU_SOURCE
#include "fbink.h"
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_WRITE_NO_STDIO
#include "stb/stb_image_write.h"
#include "../runtime-prototype/close-inherited-fds.h"
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#if defined(__ARM_NEON)
#include <arm_neon.h>
#endif
#include <time.h>

#include "../libra2-prototype/device-profile.h"
#define WIDTH KOBO_FB_WIDTH
#define HEIGHT KOBO_FB_HEIGHT
#define PIXELS (WIDTH*HEIGHT)
#define SCALE KOBO_FB_SCALE
#define PHYSICAL_PIXELS (PIXELS*SCALE*SCALE)
#define READER_EXE "/mnt/onboard/.adds/koreader/luajit"
static volatile sig_atomic_t interrupted;
static void stop(int signal_number){(void)signal_number;interrupted=1;}
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec/1e9;}
#include "reader-identity.h"
#include "../launcher-prototype/handoff-prototype/reading-lease.h"
#include "refresh-policy.h"
#include "display-quality.h"
struct snapshot_output { FILE *file; int failed; };
static void snapshot_write(void *context,void *data,int size){
 struct snapshot_output *out=context;
 if(fwrite(data,1,(size_t)size,out->file)!=(size_t)size)out->failed=1;
}
static int capture_requested_frame(const unsigned char *pixels,int have_frame){
 const char *request="/tmp/kobo-android-runtime/lab/interactive-screenshot-request";
 if(!have_frame || unlink(request)<0)return 0;
 const char *dir=getenv("KOBO_SNAPSHOT_DIR");
 if(!dir || !*dir)return -1;
 struct stat st;
 if((mkdir(dir,0755)<0 && errno!=EEXIST) || lstat(dir,&st)<0 || !S_ISDIR(st.st_mode)){
  puts("LIVE_DISPLAY_SCREENSHOT_DIRECTORY_FAILED");return -1;
 }
 char path[256];
 if(snprintf(path,sizeof(path),"%s/weread-screenshot-XXXXXX.png",dir)>=(int)sizeof(path))return -1;
 int fd=mkstemps(path,4);
 if(fd<0){perror("LIVE_DISPLAY_SCREENSHOT_CREATE");return -1;}
 FILE *file=fdopen(fd,"wb");
 if(!file){close(fd);unlink(path);return -1;}
 struct snapshot_output out={.file=file};
 int ok=stbi_write_png_to_func(snapshot_write,&out,WIDTH,HEIGHT,1,pixels,WIDTH);
 if(fclose(file)!=0)out.failed=1;
 if(ok && !out.failed){printf("LIVE_DISPLAY_SCREENSHOT_SAVED %s\n",path);return 1;}
 unlink(path);puts("LIVE_DISPLAY_SCREENSHOT_WRITE_FAILED");return -1;
}
static int consume_refresh_request(void){
 const char *path="/tmp/kobo-android-runtime/lab/interactive-refresh-request";
 int fd=open(path,O_RDONLY|O_CLOEXEC|O_NOFOLLOW);if(fd<0)return errno==ENOENT?0:-1;
 struct stat st;char data[8]={0};
 int valid=fstat(fd,&st)==0 && S_ISREG(st.st_mode) && st.st_uid==0 && (st.st_mode&0777)==0400 && st.st_size==8 && read(fd,data,8)==8 && !memcmp(data,"refresh\n",8);
 close(fd);if(!valid || unlink(path)<0)return -1;return 1;
}
/* One short-lived request per accepted reader page key. */
static int take_page_wipe(void){
 const char *path="/tmp/kobo-android-runtime/lab/resident-page-wipe";
 int fd=open(path,O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK);
 if(fd<0)return 0;
 struct stat st;char data[64]={0},direction;double sent;
 int valid=fstat(fd,&st)==0 && S_ISREG(st.st_mode) && st.st_uid==0 &&
           (st.st_mode&0777)==0400 && st.st_size>0 && st.st_size<(off_t)sizeof(data) &&
           read(fd,data,(size_t)st.st_size)==st.st_size;
 close(fd);
 if(!valid || unlink(path)<0 || sscanf(data,"%c %lf",&direction,&sent)!=2)return 0;
 double age=now()-sent;
 if(!(age>=0 && age<=1.5))return 0;
 return direction=='p'?-1:direction=='n'?1:0;
}

/* A live IME window can redraw its cursor without any typing. */
static int keyboard_visible(void){
 const char *path="/tmp/kobo-android-runtime/data/user_de/0/com.android.inputmethod.pinyin/files/kobo-keyboard-visible";
 int fd=open(path,O_RDONLY|O_CLOEXEC|O_NOFOLLOW|O_NONBLOCK);
 if(fd<0)return 0;
 char pid_text[16]={0};
 ssize_t size=read(fd,pid_text,sizeof(pid_text)-1);
 close(fd);
 if(size<=0)return 0;
 char *end;
 long pid=strtol(pid_text,&end,10);
 if(*end || pid<=1 || pid>2147483647L)return 0;
 char proc[64];
 snprintf(proc,sizeof(proc),"/proc/%ld/cmdline",pid);
 fd=open(proc,O_RDONLY|O_CLOEXEC);
 if(fd<0)return 0;
 static const char ime[]="com.android.inputmethod.pinyin";
 char name[sizeof(ime)];
 size=read(fd,name,sizeof(name));
 close(fd);
 return size==sizeof(name) && !memcmp(name,ime,sizeof(name));
}

static int open_regular(const char *path,off_t bytes){
 int fd=open(path,O_RDONLY|O_CLOEXEC|O_NOFOLLOW);struct stat st;
 if(fd<0)return -1;
 if(fstat(fd,&st)<0 || !S_ISREG(st.st_mode) || st.st_uid!=0 || st.st_size!=bytes){close(fd);return -1;}
 return fd;
}
/* 0 = published frame, 1 = the HWC is writing (single-buffer mode sets the
 * top bit of the sequence until the frame is complete), -1 = invalid. */
static int read_state(int fd,uint32_t state[6]){
 if(!(pread(fd,state,24,0)==24 && state[0]==0x4b464230 && state[1]==WIDTH && state[2]==HEIGHT && state[3]==WIDTH*4 && (state[4]==0 || state[4]==HEIGHT)))return -1;
 return (state[5]&0x80000000u)?1:0;
}
#define PARTIAL_DISABLED "/mnt/onboard/.adds/kobo-android-lab/fast-v197/partial-disabled"
/* Frame polling: 20 ms (a page turn waited 50 ms on average at 100 ms for the
 * bridge to notice the new frame). Off: back to 100 ms. */
#define FASTPOLL_DISABLED "/mnt/onboard/.adds/kobo-android-lab/resident-v197/bridgepoll-disabled"
#define SAME_DISABLED "/mnt/onboard/.adds/kobo-android-lab/resident-v197/bridgesame-disabled"
/* Black hold: every later book open publishes one all-black frame (the old
 * window gone, the reader's first buffer not composed yet) 0.2-1.4 s before
 * the page; painting it cost a black flash and, when the page came while the
 * 0.45 s full refresh ran, delayed the page by the rest of that refresh.
 * An all-black frame is held for BLACK_HOLD_S and painted only if nothing
 * else arrives meanwhile. Off: resident-v197/blackhold-disabled. */
#define BLACK_DISABLED "/mnt/onboard/.adds/kobo-android-lab/resident-v197/blackhold-disabled"
#define BLACK_HOLD_S 2.0
static int all_black(const unsigned char *gray){
 if(gray[0]|gray[PIXELS/2]|gray[PIXELS-1])return 0;
 uint32_t acc=0;size_t p=0;
 for(;p+4<=PIXELS;p+=4){uint32_t v;memcpy(&v,gray+p,4);acc|=v;if((p&4095)==0&&acc)return 0;}
 for(;p<PIXELS;p++)acc|=gray[p];
 return acc==0;
}
/* Pass 1: gray[p]=(77R+150G+29B+128)>>8 (before the LUT) and a 256-bit
 * signature of the RGBA bytes, 8 lanes of h=(h^word)*K. With K odd each step
 * is a bijection of h, so a frame that differs in a single word always gets a
 * different signature; a frame that differs elsewhere matches only by a
 * 32-bit coincidence in every lane its changes touch. It replaces an 8.5 MB
 * copy of the last frame (resident-v197 memory work) and costs no extra read:
 * the bytes are already loaded for the luma. */
#define SIG_K 0x9e3779b1u
static void luma_sig(const unsigned char *rgba,unsigned char *gray,uint32_t sig[8]){
 size_t p=0;
 for(int i=0;i<8;i++)sig[i]=0x85ebca6bu+(uint32_t)i*0x27d4eb2fu;
#if defined(__ARM_NEON)
 const uint8x8_t kr=vdup_n_u8(77),kg=vdup_n_u8(150),kb=vdup_n_u8(29);
 const uint16x8_t round=vdupq_n_u16(128);
 const uint32x4_t k=vdupq_n_u32(SIG_K);
 uint32x4_t h0=vld1q_u32(sig),h1=vld1q_u32(sig+4);
 for(;p+8<=PIXELS;p+=8){
  uint8x8x4_t px=vld4_u8(rgba+p*4);
  uint16x8_t y=vmlal_u8(vmlal_u8(vmlal_u8(round,px.val[0],kr),px.val[1],kg),px.val[2],kb);
  vst1_u8(gray+p,vshrn_n_u16(y,8));
  h0=vmulq_u32(veorq_u32(h0,vreinterpretq_u32_u8(vcombine_u8(px.val[0],px.val[1]))),k);
  h1=vmulq_u32(veorq_u32(h1,vreinterpretq_u32_u8(vcombine_u8(px.val[2],px.val[3]))),k);
 }
 vst1q_u32(sig,h0);vst1q_u32(sig+4,h1);
#endif
 for(;p<PIXELS;p++){
  gray[p]=(unsigned char)((77u*rgba[p*4]+150u*rgba[p*4+1]+29u*rgba[p*4+2]+128u)>>8);
  uint32_t w;memcpy(&w,rgba+p*4,4);sig[p&7]=(sig[p&7]^w)*SIG_K;
 }
}
/* Pass 2: gray[p]=lut[gray[p]], four pixels per 32-bit word: 9.6-10.3 ms per
 * frame on the Libra 2 against 13.4 ms byte by byte (NEON vtbl/vtbx over the
 * 256-entry table measured 19 ms: the Cortex-A9's table lookups are slow).
 * The FNV hash of the gray frame computed here before only went to the log;
 * the frame line shows the RGBA signature instead. */
static void lut_gray(unsigned char *gray,const unsigned char lut[256]){
 size_t p=0;
 for(;p+4<=PIXELS;p+=4){
  uint32_t v;memcpy(&v,gray+p,4);
  v=(uint32_t)lut[v&255]|(uint32_t)lut[(v>>8)&255]<<8|(uint32_t)lut[(v>>16)&255]<<16|(uint32_t)lut[v>>24]<<24;
  memcpy(gray+p,&v,4);
 }
 for(;p<PIXELS;p++)gray[p]=lut[gray[p]];
}
/* Bounding box of pixels that differ from the previous frame; 0 if none.
 * Horizontal edges are widened to multiples of 8 for the EPDC. */
static int changed_rect(const unsigned char *a,const unsigned char *b,int *x,int *y,int *w,int *h,int start,int end){
 int top=-1,bottom=-1,left=WIDTH,right=-1;
 for(int r=start;r<end;r++){
  const unsigned char *ra=a+(size_t)r*WIDTH,*rb=b+(size_t)r*WIDTH;
  if(!memcmp(ra,rb,WIDTH))continue;
  if(top<0)top=r;
  bottom=r;
  int l=0;while(l<left && ra[l]==rb[l])l++;
  if(l<left)left=l;
  int rr=WIDTH-1;while(rr>right && ra[rr]==rb[rr])rr--;
  if(rr>right)right=rr;
 }
 if(top<0)return 0;
 left&=~7;right=(right|7)<WIDTH?(right|7):WIDTH-1;
 *x=left;*y=top;*w=right-left+1;*h=bottom-top+1;return 1;
}
/* Keep disjoint keyboard, candidate and editor changes from forming one
 * large update. Submit the candidate band first, then the other regions. */
static int submit_typing_bands(int fb,const unsigned char *gray,unsigned char *previous,FBInkConfig config){
 const int bounds[][2]={{HEIGHT/10,HEIGHT*3/5},{HEIGHT*3/5,HEIGHT},{0,HEIGHT/10}};
 int rectangles[3][4], count=0, area=0;
 for(int i=0;i<3;i++){
  int *r=rectangles[count];
  if(!changed_rect(gray,previous,&r[0],&r[1],&r[2],&r[3],bounds[i][0],bounds[i][1]))continue;
  if(r[3]>HEIGHT/6)return 0;
  area+=r[2]*r[3];
  count++;
 }
 if(count<2 || area>PIXELS/6)return 0;
 config.wfm_mode=WFM_DU;config.halign=NONE;config.valign=NONE;
 for(int i=0;i<count;i++){
  int x=rectangles[i][0],y=rectangles[i][1],w=rectangles[i][2],h=rectangles[i][3];
  for(int row=0;row<h;row++)
   memcpy(previous+(size_t)row*w,gray+(size_t)(y+row)*WIDTH+x,(size_t)w);
  if(fbink_print_raw_data(fb,previous,w,h,(size_t)w*h,(short)x,(short)y,&config)<0)return -1;
 }
 return count;
}

/* KOReader's screen, kept for the restore at exit, is mostly white: hold it
 * run-length coded (runs of up to 255 equal bytes as count,value pairs)
 * while the bridge runs, about 2 MB less resident. Kept raw when coding does
 * not at least halve it. */
static unsigned char *rle_pack(const unsigned char *in,size_t n,size_t *out_n){
 size_t cap=n/2,o=0;unsigned char *out=malloc(cap?cap:1);if(!out)return 0;
 for(size_t i=0;i<n;){
  size_t r=1;while(i+r<n&&r<255&&in[i+r]==in[i])r++;
  if(o+2>cap){free(out);return 0;}
  out[o++]=(unsigned char)r;out[o++]=in[i];i+=r;
 }
 *out_n=o;return out;
}
static unsigned char *rle_unpack(const unsigned char *in,size_t in_n,size_t n){
 unsigned char *out=malloc(n);if(!out)return 0;size_t o=0;
 for(size_t i=0;i+1<in_n;i+=2){size_t r=in[i];if(o+r>n){free(out);return 0;}memset(out+o,in[i+1],r);o+=r;}
 if(o!=n){free(out);return 0;}
 return out;
}
/* Reveal eight disjoint strips of the new page, following Swipe_Animation's
 * wipe approach. The untouched strip still contains the old page on panel. */
static int submit_page_wipe(int fb,const unsigned char *gray,unsigned char *scratch,
                           int x,int y,int w,int h,int direction,FBInkConfig config){
 const int strips=8;
 config.halign=NONE;config.valign=NONE;
 for(int i=0;i<strips;i++){
  int strip=direction>0?strips-1-i:i;
  int left=x+((w*strip/strips)&~7);
  int right=strip+1==strips?x+w:x+((w*(strip+1)/strips)&~7);
  int width=right-left;
  for(int row=0;row<h;row++)
   memcpy(scratch+(size_t)row*width,gray+(size_t)(y+row)*WIDTH+left,(size_t)width);
  if(fbink_print_raw_data(fb,scratch,width,h,(size_t)width*h,(short)left,(short)y,&config)<0)return -1;
  if(i+1<strips){struct timespec pause={0,26000000};nanosleep(&pause,NULL);}
 }
 return 0;
}
int main(int argc,char **argv){
 if((argc!=2 && argc!=3) || geteuid()!=0)return 2;
 const char *profile=argc==3?argv[2]:"reagl-text";
 if(strcmp(profile,"classic") && strcmp(profile,"reagl") && strcmp(profile,"reagl-text"))return 2;
 int text_emphasis=!strcmp(profile,"reagl-text"),request_reagl=strcmp(profile,"classic")!=0;
 unsigned char lut[256];display_lut(lut,text_emphasis);
 char *end;long seconds=strtol(argv[1],&end,10);if(*end || seconds<5 || seconds>43260)return 2; /* resident launcher: 12 h foreground */
 if(close_inherited_fds()<0)return 3;
 setvbuf(stdout,NULL,_IOLBF,0);
 int frame=open_regular("/tmp/kobo-android-runtime/dev/graphics/fb0",PIXELS*8);
 int state_fd=open_regular("/tmp/kobo-android-runtime/lab/virtual-fb.state",24);
 if(frame<0 || state_fd<0)return 4;
 int independent=access(HANDOFF_LEASE,F_OK)==0;
 if(independent&&!reading_valid())return 5;
 struct reader readers[2]={{0}};
 /* KOReader may still be redrawing right after it was resumed; wait up to 5 s
  * for both processes to be idle instead of giving up at once. */
 if(!independent){
  int found=-1;
  for(int attempt=0;attempt<50 && found<0;attempt++){
   memset(readers,0,sizeof readers);found=find_readers(readers,'S');
   if(found<0){struct timespec delay={0,100000000};nanosleep(&delay,NULL);}
  }
  if(found<0)return 5;
 }
 const unsigned char *frames_map=mmap(NULL,(size_t)PIXELS*8,PROT_READ,MAP_SHARED,frame,0);
 if(frames_map==MAP_FAILED)return 6;
 const unsigned char *rgba=0;
 /* gray: this frame; previous: the last painted frame. Once the changed
  * rectangle is known, previous is free until the frame is painted, so it
  * holds the packed partial region; after painting the two are swapped
  * instead of copied (one 2 MB buffer and one 2 MB copy per frame less). */
 unsigned char *gray=malloc(PIXELS),*previous=malloc(PIXELS),*region;
 const char *wipe_setting=getenv("KOBO_PAGE_WIPE");
 int page_wipe=SCALE==1 && !independent && wipe_setting && !strcmp(wipe_setting,"1");
 int partial_enabled=access(PARTIAL_DISABLED,F_OK)!=0;
 int same_enabled=access(SAME_DISABLED,F_OK)!=0,last_valid=0;unsigned same_skipped=0,busy_skipped=0,changed_dropped=0;
 int black_enabled=access(BLACK_DISABLED,F_OK)!=0,black_pending=0;unsigned black_held=0;double black_deadline=0;
 uint32_t sig[8],last_sig[8];
 long poll_ns=access(FASTPOLL_DISABLED,F_OK)!=0?20000000:100000000;
#if SCALE == 1
 unsigned char *scaled=gray;
#else
 unsigned char *scaled=malloc(PHYSICAL_PIXELS);
#endif
 if(!gray || !previous || !scaled)return 6;
 FBInkConfig cfg={0};cfg.is_quiet=true;cfg.ignore_alpha=true;cfg.halign=CENTER;cfg.valign=CENTER;
 int fb=fbink_open();if(fb<0)return 7;
 if(fbink_init(fb,&cfg)<0)return 8;
 FBInkState physical={0};fbink_get_state(&cfg,&physical);
 if(physical.screen_width!=WIDTH*SCALE || physical.screen_height!=HEIGHT*SCALE){fprintf(stderr,"Unsupported orientation %ux%u\n",physical.screen_width,physical.screen_height);return 9;}
 int use_reagl=request_reagl && (physical.device_id==DEVICE_KOBO_FORMA || physical.device_id==DEVICE_KOBO_FORMA_32GB || physical.device_id==DEVICE_KOBO_LIBRA_2);
 printf("LIVE_DISPLAY_FAST mmap=1 neon=%d partial=%d poll_ms=%ld same_skip=%d same_sig=1 black_hold=%d\n",
#if defined(__ARM_NEON)
  1,
#else
  0,
#endif
  partial_enabled,poll_ns/1000000,same_enabled,black_enabled);
 printf("LIVE_DISPLAY_QUALITY profile=%s text_emphasis=%d requested_reagl=%d enabled_reagl=%d device_id=%u\n",profile,text_emphasis,request_reagl,use_reagl,(unsigned)physical.device_id);
 signal(SIGINT,stop);signal(SIGTERM,stop);signal(SIGHUP,stop);
 unsigned char *saved_rle=0;size_t saved_rle_n=0;
 FBInkDump saved={0},check={0};int rc=10,saved_ok=0;unsigned frames=0,partials=0,full_refreshes=0,manual_refreshes=0;uint32_t last_sequence=0;double updated=0,last_full=0,deadline=now()+seconds;int refresh_pending=0;
 for(int i=0;!independent&&i<2;i++){
  if(interrupted || !same_reader(&readers[i]) || kill(readers[i].pid,SIGSTOP)<0)goto cleanup;
  readers[i].stopped=1;printf("LIVE_DISPLAY_PAUSED_KOREADER pid=%d\n",readers[i].pid);
 }
 for(int i=0;!independent&&i<2;i++){
  int confirmed=0;
  for(int attempt=0;attempt<100;attempt++){
   struct reader actual={0};
   if(identity(readers[i].pid,&actual,0)<0 || actual.started!=readers[i].started)goto cleanup;
   if(actual.state=='T'){confirmed=1;break;}
   struct timespec delay={0,10000000};nanosleep(&delay,NULL);
  }
  if(!confirmed)goto cleanup;
 }
 if(fbink_dump(fb,&saved)<0)goto cleanup;saved_ok=1;
 if(saved.data && (saved_rle=rle_pack(saved.data,saved.size,&saved_rle_n))){free(saved.data);saved.data=0;}
 printf("LIVE_DISPLAY_SAVED_SCREEN bytes=%zu packed=%zu\n",saved.size,saved_rle?saved_rle_n:saved.size);
 printf("LIVE_DISPLAY_HANDOFF independent=%d\n",independent);
 printf("LIVE_DISPLAY_STARTED timeout=%ld source=%dx%d physical=%dx%d\n",seconds,WIDTH,HEIGHT,WIDTH*SCALE,HEIGHT*SCALE);
 double snapshot_notice_until=0;
 while(!interrupted && now()<deadline){
  struct timespec delay={0,poll_ns};nanosleep(&delay,NULL);
  if(independent&&!reading_valid())goto cleanup;
  int snapshot=capture_requested_frame(previous,frames!=0);
  if(snapshot){
   FBInkConfig notice={0};notice.is_quiet=true;notice.is_centered=true;
   notice.row=-3;notice.fontmult=3;notice.wfm_mode=WFM_GC16;
   if(fbink_print(fb,snapshot>0?"PNG saved: snapshot/":"Screenshot failed",&notice)>=0){
    fbink_wait_for_complete(fb,LAST_MARKER);
    snapshot_notice_until=now()+1.2;
   }
  }
  if(snapshot_notice_until>now())continue;
  if(snapshot_notice_until){refresh_pending=1;snapshot_notice_until=0;}
  int request=consume_refresh_request();if(request<0)goto cleanup;if(request)refresh_pending=1;
  uint32_t before[6],after[6];int st=read_state(state_fd,before);if(st<0)goto cleanup;
  if(st>0){busy_skipped++;continue;}
  int black_due=black_pending && now()>=black_deadline;
  if(frames && before[5]==last_sequence && !refresh_pending && !black_due)continue;
  int typing=frames && !refresh_pending && keyboard_visible();
  if(now()-updated<(typing?0.05:0.25))continue;
  double seen=now();
  rgba=frames_map+(size_t)before[4]*WIDTH*4;
  luma_sig(rgba,gray,sig);
  if(read_state(state_fd,after)<0)goto cleanup;
  if(memcmp(before,after,24)){changed_dropped++;continue;}
  if(same_enabled && last_valid && !refresh_pending && !memcmp(sig,last_sig,sizeof sig)){
   last_sequence=before[5];same_skipped++;black_pending=0;continue;
  }
  if(black_enabled && frames && !refresh_pending && all_black(gray)){
   if(!black_pending){black_pending=1;black_deadline=seen+BLACK_HOLD_S;black_held++;printf("LIVE_DISPLAY_BLACK_HELD sequence=%u seen=%.3f\n",before[5],seen);}
   if(now()<black_deadline){last_sequence=before[5];continue;}
  }
  black_pending=0;
  lut_gray(gray,lut);uint32_t hash=0;for(int i=0;i<8;i++)hash^=sig[i];
  double converted=now();
  last_sequence=before[5];
  /* Remember this frame's signature (taken from bytes the state check above
   * proved were not republished meanwhile). */
#define REMEMBER_FRAME() do{ memcpy(last_sig,sig,sizeof sig);last_valid=1; }while(0)
  int rx=0,ry=0,rw=WIDTH,rh=HEIGHT,changed=frames?changed_rect(gray,previous,&rx,&ry,&rw,&rh,0,HEIGHT):1;
  double diffed=now();
  if(frames && !changed && !refresh_pending){REMEMBER_FRAME();continue;}
  /* While typing, skip isolated caret-sized changes in the search header.
   * Keep the last painted frame for the next text or candidate update. */
  if(typing && changed && ry+rh<=HEIGHT/10 && rw<=16 && rh<=HEIGHT/32)
   continue;
#if SCALE != 1
  for(int y=0;y<HEIGHT;y++)for(int x=0;x<WIDTH;x++){
   unsigned char v=gray[y*WIDTH+x];size_t at=(size_t)(y*SCALE)*(WIDTH*SCALE)+x*SCALE;
   for(int dy=0;dy<SCALE;dy++)for(int dx=0;dx<SCALE;dx++)scaled[at+dy*WIDTH*SCALE+dx]=v;
  }
#endif
  double paint_begin=now();
  cfg.is_flashing=needs_full_refresh(frames,partials,now()-last_full,refresh_pending);
  if(typing)cfg.is_flashing=false;
  cfg.wfm_mode=cfg.is_flashing?WFM_GC16:use_reagl?WFM_REAGL:WFM_AUTO;
  int direction=page_wipe && frames?take_page_wipe():0;
  int wiped=0;
  int submitted;
  if(direction && !cfg.is_flashing && partial_enabled && rw>=WIDTH/2 && rh>=HEIGHT/3){
   if(submit_page_wipe(fb,gray,previous,0,ry,WIDTH,rh,direction,cfg)==0){
    wiped=1;
    printf("LIVE_DISPLAY_PAGE_WIPE direction=%s sequence=%u\n",direction>0?"next":"previous",before[5]);
   }else{
    // Restore the entire target rectangle after a partial submission failure.
    cfg.is_flashing=true;cfg.wfm_mode=WFM_GC16;
   }
  }
  int whole=cfg.is_flashing || !partial_enabled || !changed || (rw==WIDTH && rh==HEIGHT) || SCALE!=1;
  if(typing && !whole && !wiped && rh<=HEIGHT/7 && rw*rh<=PIXELS/8)
   cfg.wfm_mode=WFM_DU;
  int bands=0;
  if(typing && !whole && !wiped && cfg.wfm_mode!=WFM_DU)
   bands=submit_typing_bands(fb,gray,previous,cfg);
  if(bands>0){
   cfg.wfm_mode=WFM_DU;
   printf("LIVE_DISPLAY_IME_BANDS count=%d sequence=%u\n",bands,before[5]);
   submitted=0;
  }else if(wiped)submitted=0;
  else if(whole){rx=0;ry=0;rw=WIDTH;rh=HEIGHT;submitted=fbink_print_raw_data(fb,scaled,WIDTH*SCALE,HEIGHT*SCALE,PHYSICAL_PIXELS,0,0,&cfg);}
  else{
   region=previous;
   for(int r=0;r<rh;r++)memcpy(region+(size_t)r*rw,gray+(size_t)(ry+r)*WIDTH+rx,(size_t)rw);
   FBInkConfig part=cfg;part.halign=NONE;part.valign=NONE;
   submitted=fbink_print_raw_data(fb,region,rw,rh,(size_t)rw*rh,(short)rx,(short)ry,&part);
   if(submitted<0 && cfg.wfm_mode==WFM_DU){
    cfg.wfm_mode=part.wfm_mode=use_reagl?WFM_REAGL:WFM_AUTO;
    submitted=fbink_print_raw_data(fb,region,rw,rh,(size_t)rw*rh,(short)rx,(short)ry,&part);
   }
  }
  if(submitted<0 && use_reagl && !cfg.is_flashing){
   printf("LIVE_DISPLAY_REAGL_SUBMIT_FAILED_FALLBACK_GC16 error=%d\n",submitted);
   use_reagl=0;cfg.wfm_mode=WFM_GC16;cfg.is_flashing=true;
   rx=0;ry=0;rw=WIDTH;rh=HEIGHT;
   submitted=fbink_print_raw_data(fb,scaled,WIDTH*SCALE,HEIGHT*SCALE,PHYSICAL_PIXELS,0,0,&cfg);
  }
  if(submitted<0)goto cleanup;
  REMEMBER_FRAME();
  if(fbink_wait_for_complete(fb,LAST_MARKER)<0)goto cleanup;
  if(cfg.is_flashing){full_refreshes++;partials=0;last_full=now();if(refresh_pending)manual_refreshes++;printf("LIVE_DISPLAY_FULL_REFRESH reason=%s\n",refresh_pending?"human-buttons":frames?"periodic":"initial");}else partials++;
  refresh_pending=0;
  {unsigned char *t=previous;previous=gray;gray=t;}
#if SCALE == 1
  scaled=gray;
#endif
  updated=paint_begin;frames++;
  printf("LIVE_DISPLAY_FRAME frame=%u sequence=%u rgba_sig=%08x monotonic=%.3f paint_ms=%.1f seen=%.3f convert_ms=%.1f diff_ms=%.1f region=%d,%d,%dx%d waveform=%s\n",frames,last_sequence,hash,now(),(now()-paint_begin)*1000,seen,(converted-seen)*1000,(diffed-converted)*1000,rx,ry,rw,rh,cfg.wfm_mode==WFM_REAGL?"REAGL":cfg.wfm_mode==WFM_GC16?"GC16":cfg.wfm_mode==WFM_DU?"DU":"AUTO");
 }
 rc=frames?0:11;
cleanup:
 if(saved_rle){saved.data=rle_unpack(saved_rle,saved_rle_n,saved.size);free(saved_rle);saved_rle=0;if(!saved.data)saved_ok=0,rc=12;}
 if(saved_ok){
  cfg.is_flashing=true;cfg.wfm_mode=WFM_GC16;
  int restore=fbink_restore(fb,&cfg,&saved);
  if(restore<0 || fbink_dump(fb,&check)<0 || check.size!=saved.size || memcmp(check.data,saved.data,saved.size))rc=12;
  else puts("LIVE_DISPLAY_FRAMEBUFFER_RESTORE_BYTES_MATCH");
 }
 fbink_free_dump_data(&check);fbink_free_dump_data(&saved);fbink_close(fb);
 for(int i=0;i<2;i++)if(readers[i].stopped){
  if(!same_reader(&readers[i]) || kill(readers[i].pid,SIGCONT)<0)rc=13;
  else printf("LIVE_DISPLAY_RESUMED_KOREADER pid=%d\n",readers[i].pid);
 }
 munmap((void *)frames_map,(size_t)PIXELS*8);close(frame);close(state_fd);free(gray);free(previous);
#if SCALE != 1
 free(scaled);
#endif
 printf("LIVE_DISPLAY_REFRESH_SUMMARY full=%u manual=%u partial_since_full=%u same_skipped=%u black_held=%u busy_skipped=%u changed_dropped=%u\n",full_refreshes,manual_refreshes,partials,same_skipped,black_held,busy_skipped,changed_dropped);
 printf("LIVE_DISPLAY_FINISHED frames=%u result=%d\n",frames,rc);return rc;
}
