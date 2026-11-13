// SPDX-License-Identifier: GPL-3.0-or-later
#define _GNU_SOURCE
#include "fbink.h"
#include "../runtime-prototype/close-inherited-fds.h"
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#define WIDTH 720
#define HEIGHT 960
#define PIXELS (WIDTH*HEIGHT)
#define READER_EXE "/mnt/onboard/.adds/koreader/luajit"
static volatile sig_atomic_t interrupted;
static void stop(int signal_number){(void)signal_number;interrupted=1;}
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec/1e9;}
#include "reader-identity.h"
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
 struct reader readers[2]={{0}};if(find_readers(readers,'S')<0)return 5;
 unsigned char *rgba=malloc(PIXELS*4),*gray=malloc(PIXELS),*previous=malloc(PIXELS),*scaled=malloc(PIXELS*4);
 if(!rgba || !gray || !previous || !scaled)return 6;
 FBInkConfig cfg={0};cfg.is_quiet=true;cfg.ignore_alpha=true;cfg.halign=CENTER;cfg.valign=CENTER;
 int fb=fbink_open();if(fb<0)return 7;
 if(fbink_init(fb,&cfg)<0)return 8;
 FBInkState physical={0};fbink_get_state(&cfg,&physical);
 if(physical.screen_width!=1440 || physical.screen_height!=1920){fprintf(stderr,"Unsupported orientation %ux%u\n",physical.screen_width,physical.screen_height);return 9;}
 int use_reagl=request_reagl && (physical.device_id==DEVICE_KOBO_FORMA || physical.device_id==DEVICE_KOBO_FORMA_32GB);
 printf("LIVE_DISPLAY_QUALITY profile=%s text_emphasis=%d requested_reagl=%d enabled_reagl=%d device_id=%u\n",profile,text_emphasis,request_reagl,use_reagl,(unsigned)physical.device_id);
 signal(SIGINT,stop);signal(SIGTERM,stop);signal(SIGHUP,stop);
 FBInkDump saved={0},check={0};int rc=10,saved_ok=0;unsigned frames=0,partials=0,full_refreshes=0,manual_refreshes=0;uint32_t last_sequence=0;double updated=0,last_full=0,deadline=now()+seconds;int refresh_pending=0;
 for(int i=0;i<2;i++){
  if(interrupted || !same_reader(&readers[i]) || kill(readers[i].pid,SIGSTOP)<0)goto cleanup;
  readers[i].stopped=1;printf("LIVE_DISPLAY_PAUSED_KOREADER pid=%d\n",readers[i].pid);
 }
 for(int i=0;i<2;i++){
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
 printf("LIVE_DISPLAY_STARTED timeout=%ld source=720x960 physical=1440x1920\n",seconds);
 while(!interrupted && now()<deadline){
  struct timespec delay={0,100000000};nanosleep(&delay,NULL);
  int request=consume_refresh_request();if(request<0)goto cleanup;if(request)refresh_pending=1;
  uint32_t before[6],after[6];if(read_state(state_fd,before)<0)goto cleanup;
  if((frames && before[5]==last_sequence && !refresh_pending) || now()-updated<0.25)continue;
  if(pread(frame,rgba,PIXELS*4,(off_t)before[4]*WIDTH*4)!=PIXELS*4 || read_state(state_fd,after)<0)goto cleanup;
  if(memcmp(before,after,24))continue;
  last_sequence=before[5];uint32_t hash=2166136261u;
  for(size_t p=0;p<PIXELS;p++){
   gray[p]=lut[(77u*rgba[p*4]+150u*rgba[p*4+1]+29u*rgba[p*4+2]+128u)>>8];
   hash=(hash^gray[p])*16777619u;
  }
  if(frames && !memcmp(gray,previous,PIXELS) && !refresh_pending)continue;
  for(int y=0;y<HEIGHT;y++)for(int x=0;x<WIDTH;x++){
   unsigned char v=gray[y*WIDTH+x];size_t at=(size_t)(y*2)*(WIDTH*2)+x*2;
   scaled[at]=scaled[at+1]=scaled[at+WIDTH*2]=scaled[at+WIDTH*2+1]=v;
  }
  double paint_begin=now();
  cfg.is_flashing=needs_full_refresh(frames,partials,now()-last_full,refresh_pending);
  cfg.wfm_mode=cfg.is_flashing?WFM_GC16:use_reagl?WFM_REAGL:WFM_AUTO;
  int submitted=fbink_print_raw_data(fb,scaled,WIDTH*2,HEIGHT*2,PIXELS*4,0,0,&cfg);
  if(submitted<0 && use_reagl && !cfg.is_flashing){
   printf("LIVE_DISPLAY_REAGL_SUBMIT_FAILED_FALLBACK_GC16 error=%d\n",submitted);
   use_reagl=0;cfg.wfm_mode=WFM_GC16;cfg.is_flashing=true;
   submitted=fbink_print_raw_data(fb,scaled,WIDTH*2,HEIGHT*2,PIXELS*4,0,0,&cfg);
  }
  if(submitted<0)goto cleanup;
  if(fbink_wait_for_complete(fb,LAST_MARKER)<0)goto cleanup;
  if(cfg.is_flashing){full_refreshes++;partials=0;last_full=now();if(refresh_pending)manual_refreshes++;printf("LIVE_DISPLAY_FULL_REFRESH reason=%s\n",refresh_pending?"human-buttons":frames?"periodic":"initial");}else partials++;
  refresh_pending=0;
  memcpy(previous,gray,PIXELS);updated=paint_begin;frames++;
  printf("LIVE_DISPLAY_FRAME frame=%u sequence=%u gray_fnv=%08x monotonic=%.3f paint_ms=%.1f waveform=%s\n",frames,last_sequence,hash,now(),(now()-paint_begin)*1000,cfg.wfm_mode==WFM_REAGL?"REAGL":cfg.wfm_mode==WFM_GC16?"GC16":"AUTO");
 }
 rc=frames?0:11;
cleanup:
 if(saved_ok){
  cfg.is_flashing=true;
  int restore=fbink_restore(fb,&cfg,&saved);
  if(restore<0 || fbink_dump(fb,&check)<0 || check.size!=saved.size || memcmp(check.data,saved.data,saved.size))rc=12;
  else puts("LIVE_DISPLAY_FRAMEBUFFER_RESTORE_BYTES_MATCH");
 }
 fbink_free_dump_data(&check);fbink_free_dump_data(&saved);fbink_close(fb);
 for(int i=0;i<2;i++)if(readers[i].stopped){
  if(!same_reader(&readers[i]) || kill(readers[i].pid,SIGCONT)<0)rc=13;
  else printf("LIVE_DISPLAY_RESUMED_KOREADER pid=%d\n",readers[i].pid);
 }
 close(frame);close(state_fd);free(rgba);free(gray);free(previous);free(scaled);
 printf("LIVE_DISPLAY_REFRESH_SUMMARY full=%u manual=%u partial_since_full=%u\n",full_refreshes,manual_refreshes,partials);
 printf("LIVE_DISPLAY_FINISHED frames=%u result=%d\n",frames,rc);return rc;
}
