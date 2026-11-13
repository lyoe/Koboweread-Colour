// SPDX-License-Identifier: MIT
// Fixed-label per-thread CPU accounting, no stacks, memory or page contents.
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <time.h>
#define ROOT "/tmp/kobo-android-runtime"
#define READY ROOT "/lab/interactive-session-ready"
static volatile sig_atomic_t stopped;
static void stop(int n){(void)n;stopped=1;}
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec/1e9;}
static int load(const char *p,char *b,size_t cap){int fd=open(p,O_RDONLY|O_CLOEXEC|O_NOFOLLOW);if(fd<0)return -1;ssize_t n=read(fd,b,cap-1);close(fd);if(n<0)return -1;b[n]=0;return (int)n;}
struct counters {unsigned long long born,user,kernel;char state,name[80];};
static int stat_at(int pid,int tid,struct counters *out){
 char path[100],buf[4096];
 if(tid)snprintf(path,sizeof path,"/proc/%d/task/%d/stat",pid,tid);else snprintf(path,sizeof path,"/proc/%d/stat",pid);
 if(load(path,buf,sizeof buf)<0)return -1;
 char *left=strchr(buf,'('),*right=strrchr(buf,')');if(!left||!right||right<=left||right-left>=(int)sizeof out->name)return -1;
 memcpy(out->name,left+1,(size_t)(right-left-1));out->name[right-left-1]=0;out->state=right[2];
 char *save=0;int field=3;memset(&out->born,0,sizeof out->born);out->user=out->kernel=0;
 for(char *s=strtok_r(right+2," ",&save);s;s=strtok_r(NULL," ",&save),field++){
  if(field==14)out->user=strtoull(s,NULL,10);else if(field==15)out->kernel=strtoull(s,NULL,10);
  else if(field==22){out->born=strtoull(s,NULL,10);return 0;}
 }
 return -1;
}
static int link_is(int pid,const char *field,const char *s){char p[100],b[1024];snprintf(p,sizeof p,"/proc/%d/%s",pid,field);ssize_t n=readlink(p,b,sizeof b-1);if(n<0)return 0;b[n]=0;return !strcmp(b,s);}
static const char *actor(int pid){
 if(!link_is(pid,"root",ROOT))return NULL;
 if(link_is(pid,"exe",ROOT "/system/bin/surfaceflinger"))return "compositor";
 if(!link_is(pid,"exe",ROOT "/system/bin/app_process32"))return NULL;
 char p[100],b[4096];snprintf(p,sizeof p,"/proc/%d/status",pid);if(load(p,b,sizeof b)<0)return NULL;
 char *u=strstr(b,"\nUid:");unsigned uid=0;if(!u||sscanf(u,"\nUid: %u",&uid)!=1)return NULL;
 if(uid==1000)return "system";
 if(uid!=10007)return NULL;
 snprintf(p,sizeof p,"/proc/%d/cmdline",pid);if(load(p,b,sizeof b)<0)return NULL;
 if(!strcmp(b,"com.tencent.weread.eink"))return "app";
 if(!strcmp(b,"com.tencent.weread.eink:seg"))return "seg";
 return NULL;
}
static const char *category(const char *n,int leader){
 if(leader)return "main";
 if(!strcmp(n,"HeapTaskDaemon"))return "gc-daemon";
 if(!strncmp(n,"Heap thread poo",15))return "gc-worker";
 if(!strncmp(n,"Jit thread pool",15))return "jit";
 if(!strcmp(n,"RenderThread"))return "render";
 if(!strncmp(n,"Binder",6))return "binder";
 if(!strcmp(n,"android.display"))return "display-service";
 if(!strcmp(n,"ActivityManager"))return "activity-service";
 if(!strcmp(n,"WindowManager"))return "window-service";
 if(!strcmp(n,"PackageManager"))return "package-service";
 if(!strcmp(n,"android.ui"))return "ui-service";
 if(!strncmp(n,"EventThread",11)||!strncmp(n,"DispSync",8))return "vsync";
 if(!strncmp(n,"Finalizer",9))return "finalizer";
 if(!strncmp(n,"ReferenceQueue",14))return "reference";
 if(!strncmp(n,"Signal Catcher",14))return "signal";
 if(!strncmp(n,"Chrome",6)||!strncmp(n,"CrBrowser",9)||!strncmp(n,"Compositor",10))return "webview";
 if(!strncmp(n,"glide",5))return "image";
 if(!strncmp(n,"Rx",2))return "rx";
 if(!strcmp(n,"autoflush"))return "autoflush";
 if(!strncmp(n,"MPool",5)||!strncmp(n,"pool-",5))return "pool";
 return "other";
}
static void sample_actor(int pid,double t){
 const char *role=actor(pid);if(!role)return;
 struct counters process,again;if(stat_at(pid,0,&process))return;
 char path[100];snprintf(path,sizeof path,"/proc/%d/task",pid);DIR *d=opendir(path);if(!d)return;
 printf("process t=%.3f actor=%s pid=%d born=%llu user=%llu kernel=%llu\n",t,role,pid,process.born,process.user,process.kernel);
 struct dirent *e;unsigned count=0;
 while(!stopped&&(e=readdir(d))&&count<512){
  char *end;long tid=strtol(e->d_name,&end,10);if(*end||tid<=0||tid>2147483647)continue;
  struct counters c;if(stat_at(pid,(int)tid,&c))continue;count++;
  printf("thread t=%.3f actor=%s category=%s pid=%d process_born=%llu tid=%ld born=%llu user=%llu kernel=%llu state=%c\n",t,role,category(c.name,(int)tid==pid),pid,process.born,tid,c.born,c.user,c.kernel,c.state);
 }
 closedir(d);
 if(count>=512)printf("THREAD_LIMIT pid=%d\n",pid);
 if(stat_at(pid,0,&again)||again.born!=process.born)printf("PROCESS_REPLACED pid=%d born=%llu\n",pid,process.born);
}
int main(int argc,char **argv){
 if(argc!=2||geteuid()!=0)return 2;char *end;long seconds=strtol(argv[1],&end,10);if(*end||seconds<10||seconds>900)return 2;
 struct stat first,current;if(lstat(READY,&first)||!S_ISREG(first.st_mode)||first.st_uid||(first.st_mode&0777)!=0400)return 3;
 signal(SIGINT,stop);signal(SIGTERM,stop);signal(SIGHUP,stop);setvbuf(stdout,NULL,_IOLBF,0);
 printf("THREAD_CPU_BEGIN t=%.3f hz=%ld interval_seconds=2 seconds=%ld\n",now(),sysconf(_SC_CLK_TCK),seconds);
 double deadline=now()+seconds;unsigned samples=0;
 while(!stopped&&now()<deadline){
  if(lstat(READY,&current)||current.st_dev!=first.st_dev||current.st_ino!=first.st_ino||access(ROOT "/lab/interactive-session-finish",F_OK)==0)break;
  double t=now();DIR *d=opendir("/proc");if(!d)return 4;struct dirent *e;unsigned seen=0;
  while((e=readdir(d))&&seen<512){char *q;long pid=strtol(e->d_name,&q,10);if(*q||pid<=0||pid>2147483647)continue;sample_actor((int)pid,t);seen++;}
  closedir(d);struct counters own;if(!stat_at(getpid(),0,&own))printf("sampler t=%.3f user=%llu kernel=%llu\n",t,own.user,own.kernel);
  samples++;double wait=2-(now()-t);if(wait>0){struct timespec delay={(time_t)wait,(long)((wait-(time_t)wait)*1e9)};nanosleep(&delay,NULL);}
 }
 printf("THREAD_CPU_END t=%.3f samples=%u\n",now(),samples);return 0;
}
