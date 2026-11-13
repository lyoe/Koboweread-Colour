// SPDX-License-Identifier: MIT
// Bounded, reversible nice adjustment for two fixed app worker pools only.
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <time.h>
#define ROOT "/tmp/kobo-android-runtime"
#define READY ROOT "/lab/interactive-session-ready"
static volatile sig_atomic_t stopped;
static void stop(int n){(void)n;stopped=1;}
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec/1e9;}
static int load(const char *p,char *b,size_t cap){int fd=open(p,O_RDONLY|O_CLOEXEC|O_NOFOLLOW);if(fd<0)return -1;ssize_t n=read(fd,b,cap-1);close(fd);if(n<0)return -1;b[n]=0;return (int)n;}
static int stat_at(int pid,int tid,unsigned long long *born,char name[80]){
 char p[100],b[4096];if(tid)snprintf(p,sizeof p,"/proc/%d/task/%d/stat",pid,tid);else snprintf(p,sizeof p,"/proc/%d/stat",pid);
 if(load(p,b,sizeof b)<0)return -1;char *left=strchr(b,'('),*right=strrchr(b,')');if(!left||!right||right<=left||right-left>=80)return -1;
 memcpy(name,left+1,(size_t)(right-left-1));name[right-left-1]=0;int field=3;char *save=0;
 for(char *s=strtok_r(right+2," ",&save);s;s=strtok_r(NULL," ",&save),field++)if(field==22){*born=strtoull(s,NULL,10);return 0;}
 return -1;
}
static int link_is(int pid,const char *field,const char *s){char p[100],b[1024];snprintf(p,sizeof p,"/proc/%d/%s",pid,field);ssize_t n=readlink(p,b,sizeof b-1);if(n<0)return 0;b[n]=0;return !strcmp(b,s);}
static int app(int pid){
 if(!link_is(pid,"root",ROOT)||!link_is(pid,"exe",ROOT "/system/bin/app_process32"))return 0;
 char p[100],b[4096];snprintf(p,sizeof p,"/proc/%d/status",pid);if(load(p,b,sizeof b)<0)return 0;
 char *u=strstr(b,"\nUid:");unsigned uid=0;if(!u||sscanf(u,"\nUid: %u",&uid)!=1||uid!=10007)return 0;
 snprintf(p,sizeof p,"/proc/%d/cmdline",pid);if(load(p,b,sizeof b)<0)return 0;
 return !strcmp(b,"com.tencent.weread.eink")||!strcmp(b,"com.tencent.weread.eink:seg");
}
static const char *pool(const char *n){if(!strncmp(n,"WRCommonPool",12))return "common";if(!strncmp(n,"WRNetworkPool",13))return "network";return NULL;}
struct change {int pid,tid,original,changed;unsigned long long process_born,born;};
static struct change records[512];static unsigned count;
static int restore(void){
 int errors=0;unsigned restored=0,gone=0,app_changed=0;
 for(unsigned i=0;i<count;i++){
  struct change *r=&records[i];if(!r->changed)continue;unsigned long long pb,tb;char name[80];
  if(!app(r->pid)||stat_at(r->pid,0,&pb,name)||pb!=r->process_born||stat_at(r->pid,r->tid,&tb,name)||tb!=r->born){gone++;continue;}
  errno=0;int current=getpriority(PRIO_PROCESS,(id_t)r->tid);if(errno){errors++;continue;}
  if(current!=5){app_changed++;continue;}
  if(setpriority(PRIO_PROCESS,(id_t)r->tid,r->original)){errors++;continue;}
  errno=0;int actual=getpriority(PRIO_PROCESS,(id_t)r->tid);if(errno||actual!=r->original){errors++;continue;}restored++;
 }
 printf("POOL_PRIORITY_RESTORE restored=%u exited_or_replaced=%u application_changed=%u errors=%d\n",restored,gone,app_changed,errors);return errors;
}
static int scan(int boost,double t){
 DIR *procs=opendir("/proc");if(!procs)return -1;struct dirent *entry;
 while(!stopped&&(entry=readdir(procs))){
  char *end;long p=strtol(entry->d_name,&end,10);if(*end||p<=0||p>2147483647||!app((int)p))continue;
  unsigned long long pb;char name[80];if(stat_at((int)p,0,&pb,name))continue;
  char path[100];snprintf(path,sizeof path,"/proc/%ld/task",p);DIR *tasks=opendir(path);if(!tasks)continue;struct dirent *e;
  while(!stopped&&(e=readdir(tasks))){
   long tid=strtol(e->d_name,&end,10);if(*end||tid<=0||tid>2147483647||tid==p)continue;
   unsigned long long born;if(stat_at((int)p,(int)tid,&born,name))continue;const char *kind=pool(name);if(!kind)continue;
   errno=0;int old=getpriority(PRIO_PROCESS,(id_t)tid);if(errno)continue;
   unsigned index=0;for(;index<count;index++)if(records[index].pid==p&&records[index].process_born==pb&&records[index].tid==tid&&records[index].born==born)break;
   if(index==count){
    if(count==512){closedir(tasks);closedir(procs);return -1;}
    records[count++]=(struct change){(int)p,(int)tid,old,0,pb,born};
    printf("POOL_PRIORITY_OBSERVED t=%.3f pool=%s pid=%ld tid=%ld nice=%d\n",t,kind,p,tid,old);
   }
   if(!boost||old<10)continue;
   unsigned long long check;char again[80];
   if(stat_at((int)p,(int)tid,&check,again)||check!=born||!pool(again))continue;
   if(setpriority(PRIO_PROCESS,(id_t)tid,5)){printf("POOL_PRIORITY_APPLY_FAILED errno=%d\n",errno);closedir(tasks);closedir(procs);return -1;}
   records[index].original=old;records[index].changed=1;
   printf("POOL_PRIORITY_APPLIED t=%.3f pool=%s pid=%ld tid=%ld original=%d target=5\n",t,kind,p,tid,records[index].original);
  }
  closedir(tasks);
 }
 closedir(procs);return 0;
}
int main(int argc,char **argv){
 if(argc!=3||geteuid()!=0||(strcmp(argv[1],"observe")&&strcmp(argv[1],"boost")))return 2;
 char *end;long seconds=strtol(argv[2],&end,10);if(*end||seconds<10||seconds>300)return 2;
 struct stat first,current;if(lstat(READY,&first)||!S_ISREG(first.st_mode)||first.st_uid||(first.st_mode&0777)!=0400)return 3;
 signal(SIGINT,stop);signal(SIGTERM,stop);signal(SIGHUP,stop);setvbuf(stdout,NULL,_IOLBF,0);
 int boost=!strcmp(argv[1],"boost"),error=0;double deadline=now()+seconds,next=0;
 printf("POOL_PRIORITY_BEGIN mode=%s monotonic=%.3f seconds=%ld target_nice=5\n",argv[1],now(),seconds);
 while(!stopped&&now()<deadline){
  if(lstat(READY,&current)||current.st_dev!=first.st_dev||current.st_ino!=first.st_ino||access(ROOT "/lab/interactive-session-finish",F_OK)==0)break;
  double t=now();if(t>=next){if(scan(boost,t)){error=1;break;}next=t+2;}
  struct timespec delay={0,200000000};nanosleep(&delay,NULL);
 }
 if(restore())error=1;printf("POOL_PRIORITY_END mode=%s monotonic=%.3f result=%d\n",argv[1],now(),error);return error?4:0;
}
