// SPDX-License-Identifier: GPL-3.0-or-later
/* resident-sched idle|normal: switch every thread of the private Android
 * processes (root and executable inside /tmp/kobo-android-runtime) between
 * SCHED_IDLE and SCHED_OTHER. Nice values are kept by the kernel. No host
 * process, OOM setting or kernel parameter is touched. Raw syscalls: musl's
 * sched_* wrappers are stubs that return ENOSYS. */
#define _GNU_SOURCE
#include <dirent.h>
#include <limits.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>
#define ROOT "/tmp/kobo-android-runtime"
static int inside(const char *pid){
 char link[64],target[PATH_MAX];ssize_t n;
 snprintf(link,sizeof(link),"/proc/%s/root",pid);n=readlink(link,target,sizeof(target)-1);
 if(n<0)return 0;target[n]=0;if(strcmp(target,ROOT))return 0;
 snprintf(link,sizeof(link),"/proc/%s/exe",pid);n=readlink(link,target,sizeof(target)-1);
 if(n<0)return 0;target[n]=0;return !strncmp(target,ROOT "/",sizeof(ROOT));
}
int main(int argc,char **argv){
 if(argc!=2 || geteuid()!=0)return 2;
 int policy;if(!strcmp(argv[1],"idle"))policy=SCHED_IDLE;else if(!strcmp(argv[1],"normal"))policy=SCHED_OTHER;else return 2;
 DIR *proc=opendir("/proc");if(!proc)return 3;
 unsigned processes=0,threads=0,failed=0;struct dirent *p;
 while((p=readdir(proc))){
  if(p->d_name[0]<'1' || p->d_name[0]>'9' || !inside(p->d_name))continue;
  processes++;char path[64];snprintf(path,sizeof(path),"/proc/%s/task",p->d_name);
  DIR *tasks=opendir(path);if(!tasks)continue;struct dirent *t;
  while((t=readdir(tasks))){
   if(t->d_name[0]<'1' || t->d_name[0]>'9')continue;
   int current=(int)syscall(SYS_sched_getscheduler,atoi(t->d_name));
   // Leave real-time threads (if any) exactly as Android configured them.
   if(current<0 || (current!=SCHED_OTHER && current!=SCHED_IDLE && current!=SCHED_BATCH))continue;
   struct sched_param param={0};
   if(syscall(SYS_sched_setscheduler,atoi(t->d_name),policy,&param)<0)failed++;else threads++;
  }
  closedir(tasks);
 }
 closedir(proc);
 printf("RESIDENT_SCHED policy=%s processes=%u threads=%u failed=%u\n",argv[1],processes,threads,failed);
 return 0;
}
