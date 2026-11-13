// SPDX-License-Identifier: GPL-3.0-or-later
#define _GNU_SOURCE
#include "../runtime-prototype/close-inherited-fds.h"
#include <linux/input.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#define READER_EXE "/mnt/onboard/.adds/koreader/luajit"
#include "reader-identity.h"
#include "../launcher-prototype/handoff-prototype/reading-lease.h"
#include "../libra2-prototype/device-profile.h"
#include "exit-hold.h"
#include "forma-buttons.h"
#include "../libra2-prototype/device-profile.h"
static volatile sig_atomic_t stopped;
static unsigned skipped_events;
static unsigned rejected_events;
static int request_control(const char *path,const char *reason){
 char temporary[256];if(snprintf(temporary,sizeof(temporary),"%s.%ld",path,(long)getpid())>=(int)sizeof(temporary))return -1;
 int fd=open(temporary,O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0400);if(fd<0)return -1;
 size_t size=strlen(reason);int ok=write(fd,reason,size)==(ssize_t)size;if(close(fd)<0)ok=0;
 if(ok && link(temporary,path)<0){struct stat st;ok=errno==EEXIST && lstat(path,&st)==0 && S_ISREG(st.st_mode) && st.st_uid==0 && (st.st_mode&0777)==0400;}
 unlink(temporary);return ok?0:-1;
}
static void stop(int sig){(void)sig;stopped=1;}
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec/1e9;}
static int send_event(int fd,int action,int x,int y,unsigned *sequence){
 uint32_t words[]={htonl(0x4b545031),htonl(action),htonl(x),htonl(y),htonl(++*sequence)},reply;
 size_t offset=0;
 while(offset<sizeof(words)){
  ssize_t n=send(fd,(char*)words+offset,sizeof(words)-offset,MSG_NOSIGNAL);
  if(n<0 && errno==EINTR)continue;if(n<=0){fprintf(stderr,"HOST_TOUCH_SEND_FAILED action=%d serial=%u errno=%d closed=%d\n",action,*sequence,n<0?errno:0,n==0);return -1;}offset+=(size_t)n;
 }
 offset=0;while(offset<sizeof(reply)){
  ssize_t n=recv(fd,(char*)&reply+offset,sizeof(reply)-offset,0);
  if(n<0 && errno==EINTR)continue;if(n<=0){fprintf(stderr,"HOST_TOUCH_ACK_FAILED action=%d serial=%u errno=%d closed=%d\n",action,*sequence,n<0?errno:0,n==0);return -1;}offset+=(size_t)n;
 }
 if(ntohl(reply)==2){skipped_events++;return 2;}
 if(ntohl(reply)==3){rejected_events++;return 3;}
 if(ntohl(reply)!=1){fprintf(stderr,"HOST_TOUCH_ACK_REJECTED action=%d serial=%u\n",action,*sequence);return -1;}return 0;
}
int main(int argc,char **argv){
 if((argc!=2 && argc!=3) || geteuid()!=0)return 2;
 int long_session=argc==3&&!strcmp(argv[2],"--interactive-long");
 int interactive=long_session || (argc==3&&!strcmp(argv[2],"--interactive"));
 int one_gesture=argc==3&&!strcmp(argv[2],"--one-gesture");if(argc==3&&!interactive&&!one_gesture)return 2;
 char *end;long seconds=strtol(argv[1],&end,10);if(*end || seconds<5 || seconds>(long_session?1800:180))return 2;
 if(close_inherited_fds()<0)return 3;setvbuf(stdout,NULL,_IOLBF,0);
 int independent=access(HANDOFF_LEASE,F_OK)==0;
 struct reader readers[2]={{0}};
 if((independent&&!reading_valid())||(!independent&&find_readers(readers,'T')<0)){
  fprintf(stderr,"HOST_TOUCH_OWNER_REJECTED independent=%d\n",independent);return 4;
 }
 printf("HOST_TOUCH_HANDOFF independent=%d\n",independent);
 int fd=open("/dev/input/event1",O_RDONLY|O_NONBLOCK|O_CLOEXEC|O_NOFOLLOW);if(fd<0)return 5;
 char name[128]={0};struct input_absinfo ax,ay;
 if(ioctl(fd,EVIOCGNAME(sizeof(name)),name)<0 || strcmp(name,KOBO_TOUCH_NAME) ||
    ioctl(fd,EVIOCGABS(ABS_MT_POSITION_X),&ax)<0 || ioctl(fd,EVIOCGABS(ABS_MT_POSITION_Y),&ay)<0 ||
    ax.minimum!=0 || ax.maximum!=KOBO_TOUCH_MAX_X || ay.minimum!=0 || ay.maximum!=KOBO_TOUCH_MAX_Y)return 6;
 const char *path="/tmp/kobo-android-runtime/dev/socket/kobo_touch_probe";struct stat st;
 if(lstat(path,&st)<0 || !S_ISSOCK(st.st_mode) || st.st_uid!=0 || (st.st_mode&0777)!=0600)return 7;
 int sock=socket(AF_UNIX,SOCK_STREAM|SOCK_CLOEXEC,0);if(sock<0)return 8;
 struct sockaddr_un address={.sun_family=AF_UNIX};strcpy(address.sun_path,path);
 if(connect(sock,(struct sockaddr*)&address,sizeof(address))<0)return 9;
 struct ucred peer;socklen_t peer_size=sizeof(peer);
 if(getsockopt(sock,SOL_SOCKET,SO_PEERCRED,&peer,&peer_size)<0 || peer.uid!=0)return 10;
 struct timeval timeout={interactive?15:2,0};setsockopt(sock,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));setsockopt(sock,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof(timeout));
 signal(SIGINT,stop);signal(SIGTERM,stop);signal(SIGHUP,stop);signal(SIGPIPE,SIG_IGN);
 if(ioctl(fd,EVIOCGRAB,1)<0)return 11;
 unsigned long keys[(KEY_MAX+8*sizeof(long))/(8*sizeof(long))];memset(keys,0,sizeof(keys));
 if(ioctl(fd,EVIOCGKEY(sizeof(keys)),keys)<0){ioctl(fd,EVIOCGRAB,0);return 12;}
 int button=(keys[BTN_TOUCH/(8*sizeof(long))]>>(BTN_TOUCH%(8*sizeof(long))))&1;
 unsigned raw_events=0,blocked_frames=0,multi_frames=0;int initial_contact=button;
 int blocked=button,down=0,current=0,xraw=ax.value,yraw=ay.value,lastx=0,lasty=0,rc=0;unsigned mask=0,sequence=0,gestures=0;
#ifdef KOBO_TARGET_LIBRA2
 int slot=0;unsigned active_slots=0;
#endif
 int coords[16][2];for(int i=0;i<16;i++){coords[i][0]=xraw;coords[i][1]=yraw;}
#ifdef KOBO_TARGET_LIBRA2
 // Kernel may omit an unchanged slot selector or coordinate. Seed all slots,
 // and suppress contacts that were already held before taking ownership.
 struct input_absinfo selected;int tracking[11]={ABS_MT_TRACKING_ID};
 int slot_x[11]={ABS_MT_POSITION_X},slot_y[11]={ABS_MT_POSITION_Y};
 if(ioctl(fd,EVIOCGABS(ABS_MT_SLOT),&selected)<0 || selected.value<0 || selected.value>=10 ||
    ioctl(fd,EVIOCGMTSLOTS(sizeof(tracking)),tracking)<0 || ioctl(fd,EVIOCGMTSLOTS(sizeof(slot_x)),slot_x)<0 || ioctl(fd,EVIOCGMTSLOTS(sizeof(slot_y)),slot_y)<0){ioctl(fd,EVIOCGRAB,0);return 12;}
 slot=selected.value;
 for(int i=0;i<10;i++){coords[i][0]=slot_x[i+1];coords[i][1]=slot_y[i+1];if(tracking[i+1]>=0)active_slots|=1u<<i;}
 if(active_slots){blocked=1;initial_contact=1;}
#endif
 int key_fd=-1;struct forma_buttons physical_keys={0};unsigned page_keys=0,key_refreshes=0,key_exits=0,exit_holds=0;
 if(interactive){
  key_fd=open("/dev/input/event0",O_RDONLY|O_NONBLOCK|O_CLOEXEC|O_NOFOLLOW);
  char key_name[128]={0};unsigned long key_bits[(KEY_MAX+8*sizeof(long))/(8*sizeof(long))]={0};
  if(key_fd<0 || ioctl(key_fd,EVIOCGNAME(sizeof(key_name)),key_name)<0 || strcmp(key_name,"gpio-keys") || ioctl(key_fd,EVIOCGBIT(EV_KEY,sizeof(key_bits)),key_bits)<0){rc=24;goto cleanup;}
  const int required[]={193,194,116};for(unsigned k=0;k<3;k++)if(!((key_bits[required[k]/(8*sizeof(long))]>>(required[k]%(8*sizeof(long))))&1)){rc=24;goto cleanup;}
  if(ioctl(key_fd,EVIOCGRAB,1)<0 || ioctl(key_fd,EVIOCGKEY(sizeof(key_bits)),key_bits)<0){rc=24;goto cleanup;}
  unsigned held=0;for(unsigned k=0;k<3;k++)if((key_bits[required[k]/(8*sizeof(long))]>>(required[k]%(8*sizeof(long))))&1)held|=1u<<k;
  forma_buttons_init(&physical_keys,held);puts("HOST_FORMA_BUTTONS_READY upper=next lower=previous single_hold_1s=refresh chord_hold_2s=exit power_release=exit");
 }
 double deadline=now()+seconds;struct exit_hold hold={0};
#ifdef KOBO_TARGET_LIBRA2
 puts("HOST_TOUCH_FORWARD_READY protocol=elan-type-b single-finger=1 axis-transform=swap-profile-scale-clamp");
#else
 puts("HOST_TOUCH_FORWARD_READY protocol=forma-snow single-finger=1 axis-transform=mirror-y-swap-divide-two");
#endif
 while(!stopped && now()<deadline){
  if(independent&&!reading_valid()){fprintf(stderr,"HOST_TOUCH_HANDOFF_LEASE_LOST\n");rc=13;goto cleanup;}
  for(int i=0;!independent&&i<2;i++){struct reader r={0};if(identity(readers[i].pid,&r,0)<0 || r.started!=readers[i].started || r.state!='T'){rc=13;goto cleanup;}}
  struct pollfd pollers[2]={{.fd=fd,.events=POLLIN},{.fd=key_fd,.events=POLLIN}};int ready=poll(pollers,key_fd>=0?2:1,100);
  if(ready<0){if(errno==EINTR)continue;rc=14;break;}if(!ready){exit_hold_tick(&hold,now());continue;}
  if(key_fd>=0 && (pollers[1].revents&(POLLERR|POLLHUP|POLLNVAL))){rc=25;goto cleanup;}
  if(key_fd>=0 && (pollers[1].revents&POLLIN)){
   struct input_event key_events[32];ssize_t count=read(key_fd,key_events,sizeof(key_events));
   if(count<=0 || count%sizeof(*key_events)){rc=25;goto cleanup;}
   for(size_t k=0;k<(size_t)count/sizeof(*key_events);k++){
    struct input_event *key=&key_events[k];
    if(key->type==EV_SYN && key->code==SYN_DROPPED){rc=25;goto cleanup;}
    if(key->type!=EV_KEY)continue;
    int action=forma_button_event(&physical_keys,key->code,key->value,now());
    if(action==FORMA_EXIT){if(request_control("/tmp/kobo-android-runtime/lab/interactive-session-finish","reason=physical-keys\n")<0){rc=23;goto cleanup;}key_exits++;puts("HOST_FORMA_KEY_EXIT_RELEASED");goto cleanup;}
    if(action==FORMA_REFRESH){if(request_control("/tmp/kobo-android-runtime/lab/interactive-refresh-request","refresh\n")<0){rc=26;goto cleanup;}key_refreshes++;puts("HOST_FORMA_KEY_REFRESH_REQUESTED");}
    if(action==FORMA_PREVIOUS || action==FORMA_NEXT){
     if(button || down || hold.state || (pollers[0].revents&POLLIN)){puts("HOST_FORMA_KEY_SKIPPED_TOUCH_ACTIVE");continue;}
     int result=send_event(sock,action==FORMA_PREVIOUS?4:5,0,0,&sequence);
     if(result<0){rc=27;goto cleanup;}page_keys++;printf("HOST_FORMA_PAGE_KEY direction=%s ack=%d\n",action==FORMA_PREVIOUS?"previous":"next",result);
    }
   }
  }
  if(!(pollers[0].revents&POLLIN))continue;
  struct input_event events[64];ssize_t n=read(fd,events,sizeof(events));
  if(n<0){if(errno==EINTR || errno==EAGAIN)continue;rc=15;break;}
  if(n<=0 || n%sizeof(struct input_event)){rc=16;break;}
  for(size_t i=0;i<(size_t)n/sizeof(*events);i++){
   struct input_event *ev=&events[i];raw_events++;
#ifdef KOBO_TARGET_LIBRA2
   if(!interactive && raw_events<=256 && ((ev->type==EV_ABS && (ev->code==ABS_MT_SLOT || ev->code==ABS_MT_TRACKING_ID || ev->code==ABS_MT_PRESSURE)) || (ev->type==EV_KEY && ev->code==BTN_TOUCH)))
    printf("ELAN_CONTROL_EVENT type=%u code=%u value=%d slot=%d active=%x down=%d blocked=%d\n",ev->type,ev->code,ev->value,slot,active_slots,down,blocked);
#endif
   if(ev->type==EV_KEY && ev->code==BTN_TOUCH)button=!!ev->value;
   else if(ev->type==EV_ABS){
#ifdef KOBO_TARGET_LIBRA2
    // Elan uses protocol B: tracking IDs are identities, never array indexes.
    if(ev->code==ABS_MT_SLOT){if(ev->value<0 || ev->value>=10){rc=28;goto cleanup;}slot=ev->value;}
    else if(ev->code==ABS_MT_TRACKING_ID){
     if(ev->value==-1)active_slots&=~(1u<<slot);
     else if(ev->value>=0 && ev->value<=65535){
      // A reported tracking ID does not itself mean an additional finger.
      // Slot occupancy is authoritative; reject overlapping occupied slots.
      active_slots|=1u<<slot;
     }
     else {rc=28;goto cleanup;}
     if(active_slots&(active_slots-1))blocked=1;
    }else if(ev->code==ABS_MT_POSITION_X){coords[slot][0]=ev->value;}
    else if(ev->code==ABS_MT_POSITION_Y){coords[slot][1]=ev->value;}
#else
    if(ev->code==ABS_MT_SLOT){blocked=1;}
    else if(ev->code==ABS_MT_TRACKING_ID){
     if(ev->value<0 || ev->value>15){blocked=1;continue;}
     current=ev->value;mask|=1u<<current;
     if(mask&(mask-1))blocked=1;
    }else if(ev->code==ABS_MT_POSITION_X){coords[current][0]=ev->value;}
    else if(ev->code==ABS_MT_POSITION_Y){coords[current][1]=ev->value;}
#endif
   }else if(ev->type==EV_SYN && ev->code==SYN_DROPPED){
    // Stop on lost kernel events: do not invent a gesture state.
    rc=17;goto cleanup;
   }else if(ev->type==EV_SYN && ev->code==SYN_REPORT){
#ifdef KOBO_TARGET_LIBRA2
    mask=active_slots;button=active_slots!=0;
    if(active_slots){for(current=0;current<10 && !(active_slots&(1u<<current));current++);}
#endif
    if(blocked)exit_hold_cancel(&hold);
    if(!button){
     if(interactive){int held=exit_hold_release(&hold,now());
      if(held==HOLD_EXIT){if(request_control("/tmp/kobo-android-runtime/lab/interactive-session-finish","reason=corner-hold\n")<0){rc=23;goto cleanup;}exit_holds++;puts("HOST_EXIT_CORNER_HOLD_RELEASED");goto cleanup;}
      if(held==HOLD_FLUSH){if(send_event(sock,0,hold.x,hold.y,&sequence)<0){rc=20;goto cleanup;}down=1;lastx=hold.x;lasty=hold.y;}
     }
     if(down){if(send_event(sock,1,lastx,lasty,&sequence)<0){rc=18;goto cleanup;}gestures++;puts("HOST_TOUCH_GESTURE_UP");down=0;if(one_gesture)goto cleanup;}
     down=0;blocked=0;mask=0;current=0;continue;
    }
    if(blocked){exit_hold_cancel(&hold);blocked_frames++;if(mask&(mask-1))multi_frames++;if(down){if(send_event(sock,3,lastx,lasty,&sequence)<0){rc=19;goto cleanup;}down=0;}continue;}
    xraw=coords[current][0];yraw=coords[current][1];
    int x,y;if(kobo_touch_map(xraw,yraw,&x,&y)<0){blocked=1;exit_hold_cancel(&hold);continue;}
    if(interactive && (!down || hold.state)){
     int held=exit_hold_contact(&hold,x,y,now());
     if(held==HOLD_BUFFER)continue;
     if(held==HOLD_FLUSH){if(send_event(sock,0,hold.x,hold.y,&sequence)<0){rc=20;goto cleanup;}down=1;lastx=hold.x;lasty=hold.y;}
    }
    if(!down || x!=lastx || y!=lasty){
     if(send_event(sock,down?2:0,x,y,&sequence)<0){rc=20;goto cleanup;}
     if(!down)puts("HOST_TOUCH_GESTURE_DOWN");
     down=1;lastx=x;lasty=y;
    }
   }
  }
 }
cleanup:
 if(down && send_event(sock,3,lastx,lasty,&sequence)<0)rc=21;
 if(ioctl(fd,EVIOCGRAB,0)<0)rc=22;
 if(key_fd>=0){ioctl(key_fd,EVIOCGRAB,0);close(key_fd);}
 printf("HOST_FORMA_BUTTONS_CLOSED page_keys=%u refresh_requests=%u exit_requests=%u\n",page_keys,key_refreshes,key_exits);
 close(fd);close(sock);
 printf("HOST_TOUCH_FORWARD_CLOSED gestures=%u events=%u result=%d raw_events=%u blocked_frames=%u multi_frames=%u initial_contact=%d skipped_events=%u rejected_events=%u exit_holds=%u\n",gestures,sequence,rc,raw_events,blocked_frames,multi_frames,initial_contact,skipped_events,rejected_events,exit_holds);return rc;
}
