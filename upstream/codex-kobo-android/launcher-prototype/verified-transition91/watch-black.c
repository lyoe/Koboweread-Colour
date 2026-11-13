// Bounded diagnostic trigger; only follows the existing bridge's own metadata.
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
#include <signal.h>
#include <sys/stat.h>
#define READY "/tmp/kobo-android-runtime/lab/interactive-session-ready"
#define LOG "/tmp/kobo-android-runtime/lab/live-display.log"
static volatile sig_atomic_t stop;
static void end(int n){(void)n;stop=1;}
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec/1e9;}
int main(int argc,char**argv){
 if(argc!=2||geteuid()!=0)return 2;char*endptr;long seconds=strtol(argv[1],&endptr,10);if(*endptr||seconds<10||seconds>180)return 2;
 struct stat original,current;if(stat(READY,&original))return 3;
 setvbuf(stdout,0,_IOLBF,0);signal(SIGTERM,end);signal(SIGINT,end);signal(SIGHUP,SIG_IGN);
 FILE*f=fopen(LOG,"r");if(!f)return 3;fseek(f,0,SEEK_END);long offset=ftell(f);fclose(f);
 double deadline=now()+seconds,pending=0,last_trigger=-100;int captures=0;pid_t child=-1;
 puts("TRANSITION91_ARMED captures_max=2 delay_seconds=1.5");
 while(!stop&&now()<deadline&&stat(READY,&current)==0&&current.st_ino==original.st_ino&&current.st_mtime==original.st_mtime){
  if(child>0){int status;pid_t done=waitpid(child,&status,WNOHANG);if(done==child){printf("TRANSITION91_CAPTURE_FINISHED sample=%d exit=%d\n",captures,WIFEXITED(status)?WEXITSTATUS(status):128);child=-1;if(captures==2)break;}}
  f=fopen(LOG,"r");if(!f)break;
  if(fseek(f,offset,SEEK_SET)<0){fclose(f);break;}char line[512];
  while(fgets(line,sizeof(line),f)){
   if(!strchr(line,'\n')){fseek(f,offset,SEEK_SET);break;}
   offset=ftell(f);
   unsigned frame,sequence,hash;double painted;
   if(sscanf(line,"LIVE_DISPLAY_FRAME frame=%u sequence=%u gray_fnv=%x monotonic=%lf",&frame,&sequence,&hash,&painted)==4&&hash==0x0c9d0dc5&&now()-painted<3&&now()-last_trigger>8&&child<0&&!pending){
    pending=now()+1.5;last_trigger=now();printf("TRANSITION91_BLACK_SEEN frame=%u uptime=%.3f\n",frame,painted);
   }
  }
  offset=ftell(f);fclose(f);
  if(pending&&now()>=pending&&child<0&&captures<2){
   captures++;char number[8];snprintf(number,sizeof(number),"%d",captures);child=fork();
   if(child==0){execl("/bin/sh","/bin/sh","/mnt/onboard/.adds/kobo-android-lab/capture-main91.sh",number,(char*)0);_exit(127);}
   if(child<0)return 4;pending=0;
  }
  struct timespec sleep_time={0,100000000};nanosleep(&sleep_time,0);
 }
 if(child>0){int status;waitpid(child,&status,0);}printf("TRANSITION91_FINISHED captures=%d\n",captures);return 0;
}
