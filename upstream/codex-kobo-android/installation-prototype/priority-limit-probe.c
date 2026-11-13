#define _GNU_SOURCE
#include "app-priority-limit.h"
#include "../runtime-prototype/close-inherited-fds.h"
#include <sys/prctl.h>
#include <sys/wait.h>
#include <grp.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
static int child(int enabled){
 struct rlimit zero={0,0};
 if(enabled?android_app_priority_limit()<0:setrlimit(RLIMIT_NICE,&zero)<0)return 10;
 if(setgroups(0,NULL)||setgid(10000)||setuid(10000)||prctl(PR_SET_NO_NEW_PRIVS,1,0,0,0))return 11;
 FILE*f=fopen("/proc/self/status","r");if(!f)return 12;char line[256];int caps=0;
 while(fgets(line,sizeof(line),f)){unsigned long long v;if(sscanf(line,"CapEff: %llx",&v)==1)caps=v==0;}
 fclose(f);if(!caps)return 13;
 if(!enabled){errno=0;int rc=setpriority(PRIO_PROCESS,0,-2);if(rc==0 || (errno!=EACCES&&errno!=EPERM))return 14;puts("BASELINE_UID10000_PRIORITY_RAISE_DENIED caps=0");return 0;}
 int values[]={-8,0,10,-2,0};
 for(unsigned i=0;i<sizeof(values)/sizeof(values[0]);i++){
  if(setpriority(PRIO_PROCESS,0,values[i]))return 15;
  errno=0;int actual=getpriority(PRIO_PROCESS,0);if(errno||actual!=values[i])return 16;
 }
 puts("ANDROID_UID10000_ACTUAL_PRIORITY_TRANSITIONS_OK caps=0 no_new_privs=1 nice_limit=40");return 0;
}
int main(int argc,char**argv){
 (void)argv;if(argc!=1||geteuid()!=0||close_inherited_fds()<0)return 2;
 setvbuf(stdout,NULL,_IOLBF,0);
 for(int mode=0;mode<=1;mode++){pid_t pid=fork();if(pid<0)return 3;if(pid==0)_exit(child(mode));int s;if(waitpid(pid,&s,0)!=pid||!WIFEXITED(s)||WEXITSTATUS(s)!=0){printf("PRIORITY_PROBE_FAILED mode=%d status=%d\n",mode,s);return 4;}}
 puts("ANDROID_APP_PRIORITY_LIMIT_PROBE_OK");return 0;
}
