// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef KOBO_HANDOFF_GUARD_H
#define KOBO_HANDOFF_GUARD_H
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <sys/stat.h>
struct guard_lease { pid_t pid,parent;uid_t uid;unsigned long long started;char state,exe[256]; };
static double guard_now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec/1e9;}
static void guard_sleep(void){struct timespec t={0,100000000};nanosleep(&t,NULL);}
static int guard_read(pid_t pid,struct guard_lease *out){
 char p[64],line[4096];snprintf(p,sizeof(p),"/proc/%d/stat",pid);FILE*f=fopen(p,"r");if(!f)return -1;
 int ok=fgets(line,sizeof(line),f)!=NULL;fclose(f);if(!ok)return -1;
 char *r=strrchr(line,')');int parent;if(!r||sscanf(r+2,"%c %d",&out->state,&parent)!=2)return -1;
 out->pid=pid;out->parent=parent;out->started=0;char *save=NULL,*token=strtok_r(r+2," ",&save);int field=3;
 while(token){if(field==22){out->started=strtoull(token,NULL,10);break;}token=strtok_r(NULL," ",&save);field++;}
 snprintf(p,sizeof(p),"/proc/%d/exe",pid);ssize_t n=readlink(p,out->exe,sizeof(out->exe)-1);if(n<0||!out->started)return -1;out->exe[n]=0;snprintf(p,sizeof(p),"/proc/%d",pid);struct stat st;if(stat(p,&st)<0)return -1;out->uid=st.st_uid;return 0;
}
static int guard_same(const struct guard_lease *g){struct guard_lease a={0};return guard_read(g->pid,&a)==0&&a.started==g->started&&a.uid==g->uid&&!strcmp(a.exe,g->exe)&&a.state!='Z';}
static int guard_resume(const struct guard_lease *g){return guard_same(g)?kill(g->pid,SIGCONT):-1;}
/* Independent forked watchdog; also reacts when its owning controller dies.
 * The lease was captured and scoped by the caller before any process is stopped. */
static int guard_watch(const struct guard_lease *owner,const struct guard_lease *target,unsigned seconds){
 signal(SIGTERM,SIG_DFL);signal(SIGINT,SIG_DFL);signal(SIGHUP,SIG_IGN);
 double end=guard_now()+seconds;
 while(guard_now()<end&&getppid()==owner->pid&&guard_same(owner))guard_sleep();
 return guard_resume(target);
}
#endif
