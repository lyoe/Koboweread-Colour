#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#define ROOT "/tmp/kobo-android-runtime"
static volatile sig_atomic_t stop;
static void end(int n){(void)n;stop=1;}
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec/1e9;}
int main(void){if(geteuid()!=0)return 2;struct stat ready,cur;if(lstat(ROOT "/lab/interactive-session-ready",&ready)||!S_ISREG(ready.st_mode)||ready.st_uid)return 3;FILE*f=fopen(ROOT "/lab/touch-probe.log","r");if(!f)return 4;fseek(f,0,SEEK_END);pid_t child=-1;int captures=0;double event=0,deadline=now()+120;signal(SIGINT,end);signal(SIGTERM,end);setvbuf(stdout,0,_IOLBF,0);printf("SERVER_WAIT_WATCH_READY monotonic=%.3f max_seconds=120 captures_max=2\n",now());while(!stop&&now()<deadline){if(lstat(ROOT "/lab/interactive-session-ready",&cur)||cur.st_ino!=ready.st_ino||cur.st_dev!=ready.st_dev)break;clearerr(f);char b[2048];while(fgets(b,sizeof b,f))if(!event&&strstr(b,"ANDROID_WEREAD_INTERACTIVE_GESTURE_DISPATCHED")){event=now();printf("SERVER_WAIT_GESTURE_SEEN monotonic=%.3f\n",event);}if(child>0){int status;pid_t done=waitpid(child,&status,WNOHANG);if(done==child){printf("SERVER_WAIT_CAPTURE_FINISHED sample=%d status=%d\n",captures,WIFEXITED(status)?WEXITSTATUS(status):128);child=-1;if(captures==2)break;}}if(event&&child<0&&captures<2&&now()>=event+(captures?3.0:1.0)){char n[4];snprintf(n,sizeof n,"%d",++captures);child=fork();if(child==0){execl("/bin/sh","/bin/sh","/mnt/onboard/.adds/kobo-android-lab/capture-server-212.sh",n,(char*)0);_exit(127);}if(child<0)return 5;printf("SERVER_WAIT_CAPTURE_REQUEST sample=%d monotonic=%.3f\n",captures,now());}struct timespec delay={0,100000000};nanosleep(&delay,0);}fclose(f);if(child>0){int status;waitpid(child,&status,0);}printf("SERVER_WAIT_WATCH_DONE samples=%d\n",captures);return 0;}
