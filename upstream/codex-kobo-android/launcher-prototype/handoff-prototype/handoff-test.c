// SPDX-License-Identifier: GPL-3.0-or-later
#define _GNU_SOURCE
#include "../../runtime-prototype/close-inherited-fds.h"
#include "reading-lease.h"
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#define READER_EXE "/mnt/onboard/.adds/koreader/luajit"
#include "../../display-bridge-prototype/reader-identity.h"
#define ROOT "/tmp/kobo-weread-handoff"
#define RESULT "/mnt/onboard/.adds/kobo-android-lab/handoff-test-result.txt"
static volatile sig_atomic_t interrupted;
static void stop(int n){(void)n;interrupted=1;}
static int regular_output(const char *path,const char *text){
 int f=open(path,O_WRONLY|O_CREAT|O_TRUNC|O_CLOEXEC|O_NOFOLLOW,0600);if(f<0)return -1;
 struct stat s;size_t n=strlen(text);int ok=fstat(f,&s)==0&&S_ISREG(s.st_mode)&&s.st_uid==0&&write(f,text,n)==(ssize_t)n&&fsync(f)==0;
 int rc=close(f);return ok&&rc==0?0:-1;
}
static int status(const char *phase,const struct guard_lease *owner,const char *boot){
 char text[512];snprintf(text,sizeof(text),"phase=%s\nguardian_pid=%d\nguardian_start=%llu\nboot=%s\n",phase,owner->pid,owner->started,boot);
 if(regular_output(ROOT "/status.new",text)<0)return -1;return rename(ROOT "/status.new",ROOT "/status");
}
static int nickel_running(void){
 DIR*d=opendir("/proc");if(!d)return -1;struct dirent*e;int found=0;
 while((e=readdir(d))){char *end;long id=strtol(e->d_name,&end,10);if(*end||id<=0)continue;char p[64],name[64];snprintf(p,sizeof(p),"/proc/%ld/comm",id);FILE*f=fopen(p,"r");if(!f)continue;if(fgets(name,sizeof(name),f)&&!strcmp(name,"nickel\n"))found=1;fclose(f);}
 closedir(d);return found;
}
static int reader_count(void){
 DIR*d=opendir("/proc");if(!d)return -1;struct dirent*e;int count=0;
 while((e=readdir(d))){char*end;long id=strtol(e->d_name,&end,10);if(*end||id<=0)continue;struct reader r={0};if(identity((pid_t)id,&r,0)==0)count++;}
 closedir(d);return count;
}
static long memory_kib(pid_t pid){char p[64],line[256];snprintf(p,sizeof(p),"/proc/%d/smaps",pid);FILE*f=fopen(p,"r");if(!f)return -1;long total=0,v;while(fgets(line,sizeof(line),f))if(sscanf(line,"Private_Dirty: %ld kB",&v)==1||sscanf(line,"Private_Clean: %ld kB",&v)==1)total+=v;fclose(f);return total;}
static int wrapper_valid(struct guard_lease*g){
 if(g->uid!=0||strcmp(g->exe,"/bin/busybox"))return 0;
 char p[64],cmd[512];snprintf(p,sizeof(p),"/proc/%d/cmdline",g->pid);int fd=open(p,O_RDONLY|O_CLOEXEC);if(fd<0)return 0;ssize_t n=read(fd,cmd,sizeof(cmd));close(fd);
 const char expected[]="/bin/sh\0/tmp/koreader.sh\0";
 return n==(ssize_t)sizeof(expected)-1&&!memcmp(cmd,expected,sizeof(expected)-1);
}
static int restart_exit_seen(pid_t pid,unsigned long long started){
 char p[64],line[4096];snprintf(p,sizeof(p),"/proc/%d/stat",pid);FILE*f=fopen(p,"r");if(!f)return 0;
 int ok=fgets(line,sizeof(line),f)!=NULL;fclose(f);if(!ok)return 0;char*r=strrchr(line,')');if(!r||r[2]!='Z')return 0;
 char*save=NULL,*token=strtok_r(r+2," ",&save);int field=3;unsigned long long actual=0;long code=-1;
 while(token){if(field==22)actual=strtoull(token,NULL,10);if(field==52)code=strtol(token,NULL,10);field++;token=strtok_r(NULL," ",&save);}
 return actual==started&&code==(85<<8);
}
int main(int argc,char**argv){
 if(argc!=2||(strcmp(argv[1],"dry-run")&&strcmp(argv[1],"probe")&&strcmp(argv[1],"read-test"))||geteuid()!=0)return 2;
 int reading=!strcmp(argv[1],"read-test");
 if(close_inherited_fds()<0)return 3;umask(077);setvbuf(stdout,NULL,_IOLBF,0);
 if(access("/mnt/onboard/.adds/kobo-android-lab/weread-launch.lock",F_OK)==0||nickel_running()!=0)return 4;
 FILE*f=fopen("/proc/mounts","r");char line[1024];if(!f)return 4;int mounted=0;while(fgets(line,sizeof(line),f))if(strstr(line," /tmp/kobo-android-runtime "))mounted=1;fclose(f);if(mounted)return 4;
 if(!strcmp(argv[1],"probe")){
  struct reader rs[2]={{0}};struct guard_lease wr={0};int found=0;double end=guard_now()+4;
  while(guard_now()<end){if(find_readers(rs,'S')==0){found=1;break;}guard_sleep();}
  if(!found)return 6;int main_index=rs[0].parent==rs[1].pid?1:0;
  if(guard_read(rs[main_index].parent,&wr)<0||!wrapper_valid(&wr))return 6;
  printf("HANDOFF84_READONLY_PROBE_OK wrapper=%d ui=%d helper=%d\n",wr.pid,rs[main_index].pid,rs[1-main_index].pid);return 0;
 }
 if(mkdir(ROOT,0700)<0)return 5;
 if(regular_output(RESULT,"result=pending\n")<0){rmdir(ROOT);return 12;}
 int rc=6,paused=0;pid_t watcher=-1;struct guard_lease wrapper={0},owner={0};struct reader readers[2]={{0}};char boot[64]={0};long before_private=-1;int empty_observed=0,new_readers_observed=0,restart_code_verified=0,hold_started=0;pid_t launch_child=-1;int launch_result=-1;
 signal(SIGTERM,stop);signal(SIGINT,stop);signal(SIGHUP,SIG_IGN);signal(SIGPIPE,SIG_IGN);
 double limit=guard_now()+4;
 int found=0;
 while(!interrupted&&guard_now()<limit){if(find_readers(readers,'S')==0){found=1;break;}guard_sleep();}
 if(!found||!same_reader(&readers[0])||!same_reader(&readers[1]))goto cleanup;
 int main_index=readers[0].parent==readers[1].pid?1:0;
 if(guard_read(readers[main_index].parent,&wrapper)<0||!wrapper_valid(&wrapper)||guard_read(getpid(),&owner)<0)goto cleanup;
 f=fopen("/proc/sys/kernel/random/boot_id","r");if(!f)goto cleanup;if(!fgets(boot,sizeof(boot),f)){fclose(f);goto cleanup;}fclose(f);boot[strcspn(boot,"\r\n")]=0;
 before_private=memory_kib(readers[0].pid)+memory_kib(readers[1].pid);
 watcher=fork();if(watcher<0)goto cleanup;if(watcher==0){
  int recovered;
  if(!reading)recovered=guard_watch(&owner,&wrapper,60);
  else recovered=reading_watch(&owner,&wrapper,900);
  regular_output(RESULT,recovered==0?"result=13\nwatchdog_recovered=1\n":"result=14\nwatchdog_recovered=0\n");
  if(recovered==0){unlink(HANDOFF_LEASE);unlink(ROOT "/status.new");unlink(ROOT "/status");rmdir(ROOT);}_exit(recovered==0?0:1);
 }
 if(!guard_same(&wrapper)||kill(wrapper.pid,SIGSTOP)<0)goto cleanup;paused=1;
 limit=guard_now()+1;int stopped=0;
 while(guard_now()<limit){struct guard_lease a={0};if(guard_read(wrapper.pid,&a)==0&&a.started==wrapper.started&&a.state=='T'){stopped=1;break;}guard_sleep();}
 if(!stopped||status("prepared",&owner,boot)<0)goto cleanup;
 printf("HANDOFF84_PREPARED wrapper=%d ui=%d helper=%d private_kib=%ld\n",wrapper.pid,readers[main_index].pid,readers[1-main_index].pid,before_private);
 limit=guard_now()+15;
 while(!interrupted&&guard_now()<limit){if(!same_reader(&readers[0])&&!same_reader(&readers[1])){empty_observed=1;break;}guard_sleep();}
 restart_code_verified=empty_observed&&restart_exit_seen(readers[main_index].pid,readers[main_index].started);
 if(!empty_observed||!restart_code_verified||reader_count()!=0||nickel_running()!=0){rc=7;goto cleanup;}
 hold_started=1;
 if(reading){
  int gate[2],ready[2];if(pipe2(gate,O_CLOEXEC)<0||pipe2(ready,O_CLOEXEC)<0)goto cleanup;
  launch_child=fork();if(launch_child<0)goto cleanup;
  if(launch_child==0){
   close(gate[1]);close(ready[0]);signal(SIGTERM,SIG_DFL);signal(SIGINT,SIG_DFL);
   if(setsid()<0)_exit(20);char yes='1';if(write(ready[1],&yes,1)!=1)_exit(20);close(ready[1]);
   if(read(gate[0],&yes,1)!=1||yes!='1')_exit(20);close(gate[0]);
   int log=open("/mnt/onboard/.adds/kobo-android-lab/handoff-reading-private.log",O_WRONLY|O_CREAT|O_TRUNC|O_NOFOLLOW,0600);
   if(log<0)_exit(21);dup2(log,1);dup2(log,2);close(log);
   execl("/bin/sh","/bin/sh","/mnt/onboard/.adds/kobo-android-lab/launch-weread.sh","180",(char*)NULL);_exit(22);
  }
  close(gate[0]);close(ready[1]);char yes;int ok=read(ready[0],&yes,1)==1;close(ready[0]);struct guard_lease child={0};
  char lease[512];
  if(ok&&guard_read(launch_child,&child)==0&&getsid(launch_child)==launch_child){
   snprintf(lease,sizeof(lease),"%d %llu %d %llu %d %llu %s\n",owner.pid,owner.started,wrapper.pid,wrapper.started,child.pid,child.started,boot);
   ok=regular_output(HANDOFF_LEASE,lease)==0;
  }else ok=0;
  if(ok)ok=write(gate[1],"1",1)==1;close(gate[1]);
  if(!ok){rc=15;goto cleanup;}
  status("reading",&owner,boot);printf("HANDOFF85_READING_STARTED child=%d seconds=180\n",launch_child);
  int ws=0;limit=guard_now()+780;
  for(;;){pid_t got=waitpid(launch_child,&ws,WNOHANG);if(got==launch_child){launch_result=WIFEXITED(ws)?WEXITSTATUS(ws):128+WTERMSIG(ws);break;}
   if(got<0||interrupted||guard_now()>limit){rc=16;goto cleanup;}guard_sleep();}
  rc=launch_result;goto cleanup;
 }
 if(status("holding",&owner,boot)<0)goto cleanup;
 printf("HANDOFF84_UI_EXITED nickel=absent hold_seconds=5\n");limit=guard_now()+5;
 while(!interrupted&&guard_now()<limit){if(nickel_running()!=0||reader_count()!=0){rc=8;goto cleanup;}guard_sleep();}
 rc=interrupted?9:0;
cleanup:
 if(launch_child>0){
  if(!reading_quiet(launch_child)&&reading_stop(launch_child)<0){
   puts("HANDOFF85_ACTORS_REMAIN_WRAPPER_NOT_RESUMED");return 17; /* independent watchdog remains armed */
  }
  waitpid(launch_child,NULL,WNOHANG);
 }
 if(paused){if(guard_resume(&wrapper)<0)rc=10;else puts("HANDOFF84_WRAPPER_RESUMED");}
 if(watcher>0){kill(watcher,SIGKILL);waitpid(watcher,NULL,0);}
 if(paused&&empty_observed){limit=guard_now()+20;while(!interrupted&&guard_now()<limit){struct reader fresh[2]={{0}};if(find_readers(fresh,'S')==0&&!same_reader(&readers[0])&&!same_reader(&readers[1])){new_readers_observed=1;break;}guard_sleep();}if(!new_readers_observed&&rc==0)rc=11;if(new_readers_observed&&nickel_running()!=0)rc=8;}
 char report[640];snprintf(report,sizeof(report),"result=%d\nui_exit_observed=%d\nrestart_code_verified=%d\nnickel_during_hold=%s\nnew_readers_observed=%d\nprevious_private_kib=%ld\nhold_seconds=%d\n",rc,empty_observed,restart_code_verified,hold_started?"absent":"not_tested",new_readers_observed,before_private,reading?0:5);
 size_t used=strlen(report);snprintf(report+used,sizeof(report)-used,"mode=%s\nreading_result=%d\n",reading?"read-test":"dry-run",launch_result);
 if(regular_output(RESULT,report)<0)rc=12;
 unlink(HANDOFF_LEASE);unlink(ROOT "/status.new");unlink(ROOT "/status");rmdir(ROOT);
 printf("HANDOFF85_FINISHED mode=%s result=%d new_readers=%d\n",reading?"read-test":"dry-run",rc,new_readers_observed);return rc;
}
