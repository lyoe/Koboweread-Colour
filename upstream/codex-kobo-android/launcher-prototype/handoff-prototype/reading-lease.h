// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef KOBO_READING_LEASE_H
#define KOBO_READING_LEASE_H
#include "guard.h"
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#define HANDOFF_ROOT "/tmp/kobo-weread-handoff"
#define HANDOFF_LEASE HANDOFF_ROOT "/reading-lease"
#define HANDOFF_HELPER "/mnt/onboard/.adds/kobo-android-lab/handoff-test"
#define RUNTIME_PREFIX "/tmp/kobo-android-runtime/"
struct reading_lease {pid_t owner,wrapper,child;unsigned long long owner_start,wrapper_start,child_start;char boot[64];};
static inline int reading_load(struct reading_lease *l){
 struct stat d,s;if(lstat(HANDOFF_ROOT,&d)<0||!S_ISDIR(d.st_mode)||d.st_uid||((d.st_mode&0777)!=0700))return -1;
 int fd=open(HANDOFF_LEASE,O_RDONLY|O_CLOEXEC|O_NOFOLLOW);if(fd<0)return -1;
 char buf[512]={0},extra;ssize_t n=read(fd,buf,sizeof(buf)-1);
 int ok=fstat(fd,&s)==0&&S_ISREG(s.st_mode)&&s.st_uid==0&&(s.st_mode&0777)==0600&&n>0&&n<511;close(fd);
 if(!ok||sscanf(buf,"%d %llu %d %llu %d %llu %63s %c",&l->owner,&l->owner_start,&l->wrapper,&l->wrapper_start,&l->child,&l->child_start,l->boot,&extra)!=7||l->owner<=1||l->wrapper<=1||l->child<=1)return -1;
 char boot[64];FILE*f=fopen("/proc/sys/kernel/random/boot_id","r");if(!f)return -1;ok=fgets(boot,sizeof(boot),f)!=NULL;fclose(f);boot[strcspn(boot,"\r\n")]=0;
 return ok&&!strcmp(boot,l->boot)?0:-1;
}
static inline int reading_gui_absent(void){
 DIR*d=opendir("/proc");if(!d)return 0;struct dirent*e;int ok=1;
 while((e=readdir(d))){char*end;long id=strtol(e->d_name,&end,10);if(*end||id<=1)continue;
  struct guard_lease a={0};if(guard_read((pid_t)id,&a)<0||a.state=='Z')continue;
  const char*name=strrchr(a.exe,'/');if(!strcmp(a.exe,"/mnt/onboard/.adds/koreader/luajit")||(name&&!strcmp(name,"/nickel"))){ok=0;break;}
 }closedir(d);return ok;
}
static inline int reading_valid(void){
 struct reading_lease l;struct guard_lease o={0},w={0},c={0};
 if(reading_load(&l)<0||guard_read(l.owner,&o)<0||guard_read(l.wrapper,&w)<0||guard_read(l.child,&c)<0)return 0;
 return reading_gui_absent()&&o.uid==0&&o.started==l.owner_start&&!strcmp(o.exe,HANDOFF_HELPER)&&o.state!='Z'&&w.uid==0&&w.started==l.wrapper_start&&!strcmp(w.exe,"/bin/busybox")&&w.state=='T'&&c.uid==0&&c.started==l.child_start&&c.state!='Z'&&getsid(c.pid)==c.pid&&getsid(0)==c.pid;
}
/* The session leader is a gated child created by this guardian. Iterate exact
 * session members rather than signalling a potentially recycled process group. */
static inline int reading_members(pid_t session,int sig){
 DIR*d=opendir("/proc");if(!d)return -1;struct dirent*e;int count=0;
 while((e=readdir(d))){char*end;long id=strtol(e->d_name,&end,10);if(*end||id<=1)continue;struct guard_lease a={0};if(guard_read((pid_t)id,&a)<0||a.state=='Z')continue;
  if(getsid(a.pid)!=session)continue;count++;if(sig&&guard_same(&a))kill(a.pid,sig);
 }closedir(d);return count;
}
static inline int reading_private_actors(void){
 DIR*d=opendir("/proc");if(!d)return -1;struct dirent*e;int count=0;
 while((e=readdir(d))){char*end;long id=strtol(e->d_name,&end,10);if(*end||id<=1)continue;struct guard_lease a={0};if(guard_read((pid_t)id,&a)==0&&a.state!='Z'&&!strncmp(a.exe,RUNTIME_PREFIX,strlen(RUNTIME_PREFIX)))count++;}
 closedir(d);return count;
}
static inline int reading_quiet(pid_t child){return reading_members(child,0)==0&&reading_private_actors()==0;}
/* First permit normal bounded session cleanup. On failure only this owned
 * session is signalled; never unmount an unproven image or resume over actors. */
static inline int reading_stop(pid_t child){
 const char *marker=RUNTIME_PREFIX "lab/interactive-session-finish";
 int f=open(marker,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0400);if(f>=0){write(f,"reason=handoff-recovery\n",24);close(f);}
 double end=guard_now()+12;while(guard_now()<end){if(reading_quiet(child))return 0;guard_sleep();}
 reading_members(child,SIGTERM);end=guard_now()+15;
 while(guard_now()<end){if(reading_quiet(child))return 0;guard_sleep();}
 reading_members(child,SIGKILL);end=guard_now()+5;
 while(guard_now()<end){if(reading_quiet(child))return 0;guard_sleep();}return -1;
}
static inline int reading_watch(const struct guard_lease *owner,const struct guard_lease *wrapper,unsigned seconds){
 signal(SIGTERM,SIG_DFL);signal(SIGINT,SIG_DFL);signal(SIGHUP,SIG_IGN);
 double deadline=guard_now()+seconds;
 while(guard_now()<deadline&&getppid()==owner->pid&&guard_same(owner))guard_sleep();
 if(guard_same(owner))kill(owner->pid,SIGKILL);
 struct reading_lease lease={0};int quiet=0;
 if(reading_load(&lease)==0&&lease.owner==owner->pid&&lease.owner_start==owner->started){
  struct guard_lease actual={0};guard_read(lease.child,&actual);
  char path[64];snprintf(path,sizeof(path),"/proc/%d",lease.child);
  /* A zombie still has its start stamp, although its exe link is gone. A
   * missing leader may leave the original session alive; Linux reserves its
   * session ID until all members leave. An existing different leader fails. */
  int owned=(actual.started==lease.child_start&&getsid(lease.child)==lease.child)||(access(path,F_OK)<0&&errno==ENOENT);
  if(owned)quiet=reading_stop(lease.child)==0;
 }else quiet=reading_private_actors()==0;
 return quiet?guard_resume(wrapper):-1;
}
#endif
