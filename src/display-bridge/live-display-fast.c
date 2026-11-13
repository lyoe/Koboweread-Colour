// SPDX-License-Identifier: GPL-3.0-or-later
/* Fast variant of live-display.c (same protocol, refresh policy, LUT and
 * restore path):
 *  - reads the virtual framebuffer through mmap instead of an 8.5 MB pread;
 *    the state is still re-checked afterwards and torn frames are dropped;
 *  - computes luma with NEON using the identical integer formula;
 *  - submits only the changed rectangle for partial (non-flashing) updates,
 *    unless fast/partial-disabled exists. Full refreshes stay full-screen;
 *  - waits up to 5 s for both KOReader processes to be idle before pausing.
 */
#define _GNU_SOURCE
#include "fbink.h"
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
static int consume_refresh_request(void){
 const char *path="/tmp/kobo-android-runtime/lab/interactive-refresh-request";
 int fd=open(path,O_RDONLY|O_CLOEXEC|O_NOFOLLOW);if(fd<0)return errno==ENOENT?0:-1;
 struct stat st;char data[8]={0};
 int valid=fstat(fd,&st)==0 && S_ISREG(st.st_mode) && st.st_uid==0 && (st.st_mode&0777)==0400 && st.st_size==8 && read(fd,data,8)==8 && !memcmp(data,"refresh\n",8);
 close(fd);if(!valid || unlink(path)<0)return -1;return 1;
}
static int open_regular(const char *path,off_t bytes){
 int fd=open(path,O_RDONLY|O_CLOEXEC|O_NOFOLLOW);struct stat st;
 if(fd<0)return -1;
 if(fstat(fd,&st)<0 || !S_ISREG(st.st_mode) || st.st_uid!=0 || st.st_size!=bytes){close(fd);return -1;}
 return fd;
}
static int read_state(int fd,uint32_t state[6]){
 return pread(fd,state,24,0)==24 && state[0]==0x4b464230 && state[1]==WIDTH && state[2]==HEIGHT && state[3]==WIDTH*4 && (state[4]==0 || state[4]==HEIGHT)?0:-1;
}
#define PARTIAL_DISABLED "/mnt/onboard/.adds/kobo-android-lab/fast/partial-disabled"
/* gray[p]=lut[(77R+150G+29B+128)>>8], FNV-1a hash over gray (same as before). */
static uint32_t convert(const unsigned char *rgba,unsigned char *gray,const unsigned char lut[256]){
 size_t p=0;
#if defined(__ARM_NEON)
 const uint8x8_t kr=vdup_n_u8(77),kg=vdup_n_u8(150),kb=vdup_n_u8(29);
 const uint16x8_t round=vdupq_n_u16(128);
 for(;p+8<=PIXELS;p+=8){
  uint8x8x4_t px=vld4_u8(rgba+p*4);
  uint16x8_t y=vmlal_u8(vmlal_u8(vmlal_u8(round,px.val[0],kr),px.val[1],kg),px.val[2],kb);
  vst1_u8(gray+p,vshrn_n_u16(y,8));
 }
#endif
 for(;p<PIXELS;p++)gray[p]=(unsigned char)((77u*rgba[p*4]+150u*rgba[p*4+1]+29u*rgba[p*4+2]+128u)>>8);
 uint32_t hash=2166136261u;
 for(p=0;p<PIXELS;p++){gray[p]=lut[gray[p]];hash=(hash^gray[p])*16777619u;}
 return hash;
}
/* Bounding box of pixels that differ from the previous frame; 0 if none.
 * Horizontal edges are widened to multiples of 8 for the EPDC. */
static int changed_rect(const unsigned char *a,const unsigned char *b,int *x,int *y,int *w,int *h){
 int top=-1,bottom=-1,left=WIDTH,right=-1;
 for(int r=0;r<HEIGHT;r++){
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
int main(int argc,char **argv){
 if((argc!=2 && argc!=3) || geteuid()!=0)return 2;
 const char *profile=argc==3?argv[2]:"reagl-text";
 if(strcmp(profile,"classic") && strcmp(profile,"reagl") && strcmp(profile,"reagl-text"))return 2;
 int text_emphasis=!strcmp(profile,"reagl-text"),request_reagl=strcmp(profile,"classic")!=0;
 unsigned char lut[256];display_lut(lut,text_emphasis);
 char *end;long seconds=strtol(argv[1],&end,10);if(*end || seconds<5 || seconds>1860)return 2;
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
 unsigned char *gray=malloc(PIXELS),*previous=malloc(PIXELS),*region=malloc(PIXELS);
 int partial_enabled=access(PARTIAL_DISABLED,F_OK)!=0;
#if SCALE == 1
 unsigned char *scaled=gray;
#else
 unsigned char *scaled=malloc(PHYSICAL_PIXELS);
#endif
 if(!gray || !previous || !region || !scaled)return 6;
 FBInkConfig cfg={0};cfg.is_quiet=true;cfg.ignore_alpha=true;cfg.halign=CENTER;cfg.valign=CENTER;
 int fb=fbink_open();if(fb<0)return 7;
 if(fbink_init(fb,&cfg)<0)return 8;
 FBInkState physical={0};fbink_get_state(&cfg,&physical);
 if(physical.screen_width!=WIDTH*SCALE || physical.screen_height!=HEIGHT*SCALE){fprintf(stderr,"Unsupported orientation %ux%u\n",physical.screen_width,physical.screen_height);return 9;}
 int use_reagl=request_reagl && (physical.device_id==DEVICE_KOBO_FORMA || physical.device_id==DEVICE_KOBO_FORMA_32GB || physical.device_id==DEVICE_KOBO_LIBRA_2);
 printf("LIVE_DISPLAY_FAST mmap=1 neon=%d partial=%d\n",
#if defined(__ARM_NEON)
  1,
#else
  0,
#endif
  partial_enabled);
 printf("LIVE_DISPLAY_QUALITY profile=%s text_emphasis=%d requested_reagl=%d enabled_reagl=%d device_id=%u\n",profile,text_emphasis,request_reagl,use_reagl,(unsigned)physical.device_id);
 signal(SIGINT,stop);signal(SIGTERM,stop);signal(SIGHUP,stop);
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
 printf("LIVE_DISPLAY_HANDOFF independent=%d\n",independent);
 printf("LIVE_DISPLAY_STARTED timeout=%ld source=%dx%d physical=%dx%d\n",seconds,WIDTH,HEIGHT,WIDTH*SCALE,HEIGHT*SCALE);
 while(!interrupted && now()<deadline){
  struct timespec delay={0,100000000};nanosleep(&delay,NULL);
  if(independent&&!reading_valid())goto cleanup;
  int request=consume_refresh_request();if(request<0)goto cleanup;if(request)refresh_pending=1;
  uint32_t before[6],after[6];if(read_state(state_fd,before)<0)goto cleanup;
  if((frames && before[5]==last_sequence && !refresh_pending) || now()-updated<0.25)continue;
  rgba=frames_map+(size_t)before[4]*WIDTH*4;
  uint32_t hash=convert(rgba,gray,lut);
  if(read_state(state_fd,after)<0)goto cleanup;
  if(memcmp(before,after,24))continue;
  last_sequence=before[5];
  int rx=0,ry=0,rw=WIDTH,rh=HEIGHT,changed=frames?changed_rect(gray,previous,&rx,&ry,&rw,&rh):1;
  if(frames && !changed && !refresh_pending)continue;
#if SCALE != 1
  for(int y=0;y<HEIGHT;y++)for(int x=0;x<WIDTH;x++){
   unsigned char v=gray[y*WIDTH+x];size_t at=(size_t)(y*SCALE)*(WIDTH*SCALE)+x*SCALE;
   for(int dy=0;dy<SCALE;dy++)for(int dx=0;dx<SCALE;dx++)scaled[at+dy*WIDTH*SCALE+dx]=v;
  }
#endif
  double paint_begin=now();
  cfg.is_flashing=needs_full_refresh(frames,partials,now()-last_full,refresh_pending);
  cfg.wfm_mode=cfg.is_flashing?WFM_GC16:use_reagl?WFM_REAGL:WFM_AUTO;
  int submitted;
  int whole=cfg.is_flashing || !partial_enabled || !changed || (rw==WIDTH && rh==HEIGHT) || SCALE!=1;
  if(whole){rx=0;ry=0;rw=WIDTH;rh=HEIGHT;submitted=fbink_print_raw_data(fb,scaled,WIDTH*SCALE,HEIGHT*SCALE,PHYSICAL_PIXELS,0,0,&cfg);}
  else{
   for(int r=0;r<rh;r++)memcpy(region+(size_t)r*rw,gray+(size_t)(ry+r)*WIDTH+rx,(size_t)rw);
   FBInkConfig part=cfg;part.halign=NONE;part.valign=NONE;
   submitted=fbink_print_raw_data(fb,region,rw,rh,(size_t)rw*rh,(short)rx,(short)ry,&part);
  }
  if(submitted<0 && use_reagl && !cfg.is_flashing){
   printf("LIVE_DISPLAY_REAGL_SUBMIT_FAILED_FALLBACK_GC16 error=%d\n",submitted);
   use_reagl=0;cfg.wfm_mode=WFM_GC16;cfg.is_flashing=true;
   rx=0;ry=0;rw=WIDTH;rh=HEIGHT;
   submitted=fbink_print_raw_data(fb,scaled,WIDTH*SCALE,HEIGHT*SCALE,PHYSICAL_PIXELS,0,0,&cfg);
  }
  if(submitted<0)goto cleanup;
  if(fbink_wait_for_complete(fb,LAST_MARKER)<0)goto cleanup;
  if(cfg.is_flashing){full_refreshes++;partials=0;last_full=now();if(refresh_pending)manual_refreshes++;printf("LIVE_DISPLAY_FULL_REFRESH reason=%s\n",refresh_pending?"human-buttons":frames?"periodic":"initial");}else partials++;
  refresh_pending=0;
  memcpy(previous,gray,PIXELS);updated=paint_begin;frames++;
  printf("LIVE_DISPLAY_FRAME frame=%u sequence=%u gray_fnv=%08x monotonic=%.3f paint_ms=%.1f region=%d,%d,%dx%d waveform=%s\n",frames,last_sequence,hash,now(),(now()-paint_begin)*1000,rx,ry,rw,rh,cfg.wfm_mode==WFM_REAGL?"REAGL":cfg.wfm_mode==WFM_GC16?"GC16":"AUTO");
 }
 rc=frames?0:11;
cleanup:
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
 munmap((void *)frames_map,(size_t)PIXELS*8);close(frame);close(state_fd);free(gray);free(previous);free(region);
#if SCALE != 1
 free(scaled);
#endif
 printf("LIVE_DISPLAY_REFRESH_SUMMARY full=%u manual=%u partial_since_full=%u\n",full_refreshes,manual_refreshes,partials);
 printf("LIVE_DISPLAY_FINISHED frames=%u result=%d\n",frames,rc);return rc;
}
