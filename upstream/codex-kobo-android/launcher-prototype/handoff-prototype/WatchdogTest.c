// Runs only against child fixtures created by this test, never KOReader.
#define _GNU_SOURCE
#include "reading-lease.h"
#include <sys/wait.h>
#include <poll.h>
#define CHECK(x) do { if(!(x)){fprintf(stderr,"CHECK_FAILED line=%d: %s\n",__LINE__,#x);abort();} } while(0)
static void one(int crash){
 int pipefd[2];CHECK(pipe(pipefd)==0);pid_t target=fork();CHECK(target>=0);
 if(target==0){close(pipefd[0]);raise(SIGSTOP);CHECK(write(pipefd[1],"R",1)==1);for(;;)pause();}
 close(pipefd[1]);int st;CHECK(waitpid(target,&st,WUNTRACED)==target&&WIFSTOPPED(st));
 struct guard_lease lease={0};CHECK(guard_read(target,&lease)==0&&lease.state=='T');
 struct guard_lease wrong=lease;wrong.started++;CHECK(guard_resume(&wrong)==-1);
 struct guard_lease actual={0};CHECK(guard_read(target,&actual)==0&&actual.state=='T');
 pid_t controller=fork();CHECK(controller>=0);
 if(controller==0){
  struct guard_lease owner={0};CHECK(guard_read(getpid(),&owner)==0);
  pid_t watcher=fork();CHECK(watcher>=0);
  if(watcher==0){int r=guard_watch(&owner,&lease,1);_exit(r==0?0:1);}
  if(crash)raise(SIGKILL);
  int ws;CHECK(waitpid(watcher,&ws,0)==watcher&&WIFEXITED(ws)&&WEXITSTATUS(ws)==0);_exit(0);
 }
 struct pollfd pollfd={pipefd[0],POLLIN,0};CHECK(poll(&pollfd,1,5000)==1);char b;CHECK(read(pipefd[0],&b,1)==1&&b=='R');close(pipefd[0]);
 CHECK(waitpid(controller,&st,0)==controller);if(crash)CHECK(WIFSIGNALED(st)&&WTERMSIG(st)==SIGKILL);else CHECK(WIFEXITED(st)&&WEXITSTATUS(st)==0);
 CHECK(kill(target,SIGKILL)==0);CHECK(waitpid(target,&st,0)==target);
}
static void lease_rejection_fixture(void){
 CHECK(access(HANDOFF_ROOT,F_OK)<0);CHECK(mkdir(HANDOFF_ROOT,0700)==0);
 struct reading_lease l={0};CHECK(reading_load(&l)<0);
 char boot[64];FILE*f=fopen("/proc/sys/kernel/random/boot_id","r");CHECK(f&&fgets(boot,sizeof(boot),f));fclose(f);boot[strcspn(boot,"\r\n")]=0;
 int fd=open(HANDOFF_LEASE,O_WRONLY|O_CREAT|O_EXCL,0600);CHECK(fd>=0);
 char text[256];int n=snprintf(text,sizeof(text),"2 1 3 1 4 1 %s\n",boot);CHECK(write(fd,text,n)==n);close(fd);
 CHECK(reading_load(&l)==0);CHECK(!reading_valid());
 CHECK(chmod(HANDOFF_LEASE,0644)==0);CHECK(reading_load(&l)<0);CHECK(unlink(HANDOFF_LEASE)==0);
 CHECK(symlink("/proc/sys/kernel/random/boot_id",HANDOFF_LEASE)==0);CHECK(reading_load(&l)<0);CHECK(unlink(HANDOFF_LEASE)==0);
 fd=open(HANDOFF_LEASE,O_WRONLY|O_CREAT|O_EXCL,0600);CHECK(fd>=0);const char *stale="2 1 3 1 4 1 stale-boot\n";CHECK(write(fd,stale,strlen(stale))==(ssize_t)strlen(stale));close(fd);CHECK(reading_load(&l)<0);
 CHECK(unlink(HANDOFF_LEASE)==0);CHECK(rmdir(HANDOFF_ROOT)==0);
 puts("HANDOFF85_LEASE_MISSING_PERMISSIONS_SYMLINK_STALE_BOOT_DEAD_OWNER_REJECTED_OK");
}
static void reading_cleanup_fixture(void){
 CHECK(reading_private_actors()==0);CHECK(access(HANDOFF_ROOT,F_OK)<0);
 int gate[2];CHECK(pipe(gate)==0);pid_t child=fork();CHECK(child>=0);
 if(child==0){close(gate[0]);CHECK(setsid()==getpid());pid_t worker=fork();CHECK(worker>=0);if(worker==0){close(gate[1]);for(;;)pause();}CHECK(write(gate[1],"Y",1)==1);close(gate[1]);for(;;)pause();}
 close(gate[1]);char b;CHECK(read(gate[0],&b,1)==1);close(gate[0]);
 CHECK(reading_members(child,0)==2);CHECK(!reading_quiet(child));
 CHECK(reading_stop(child)==0);CHECK(reading_quiet(child));int st;CHECK(waitpid(child,&st,0)==child);CHECK(WIFSIGNALED(st)&&WTERMSIG(st)==SIGTERM);
 CHECK(getsid(0)!=child);puts("HANDOFF85_OWNED_SESSION_CHILDREN_CLEANED_BEFORE_RESUME_OK");
}
static void reading_watchdog_fixture(void){
 CHECK(reading_private_actors()==0);CHECK(mkdir(HANDOFF_ROOT,0700)==0);
 int resume[2],ready[2];CHECK(pipe(resume)==0&&pipe(ready)==0);
 pid_t target=fork();CHECK(target>=0);if(!target){close(resume[0]);raise(SIGSTOP);CHECK(write(resume[1],"R",1)==1);for(;;)pause();}
 close(resume[1]);int st;CHECK(waitpid(target,&st,WUNTRACED)==target&&WIFSTOPPED(st));struct guard_lease wrapper={0};CHECK(guard_read(target,&wrapper)==0);
 pid_t job=fork();CHECK(job>=0);if(!job){close(ready[0]);CHECK(setsid()==getpid());CHECK(write(ready[1],"Y",1)==1);close(ready[1]);for(;;)pause();}
 close(ready[1]);char b;CHECK(read(ready[0],&b,1)==1);close(ready[0]);struct guard_lease job_lease={0};CHECK(guard_read(job,&job_lease)==0);
 pid_t owner=fork();CHECK(owner>=0);if(!owner){
  struct guard_lease me={0};CHECK(guard_read(getpid(),&me)==0);char boot[64];FILE*f=fopen("/proc/sys/kernel/random/boot_id","r");CHECK(f&&fgets(boot,sizeof(boot),f));fclose(f);boot[strcspn(boot,"\r\n")]=0;
  f=fopen(HANDOFF_LEASE,"w");CHECK(f);CHECK(chmod(HANDOFF_LEASE,0600)==0);fprintf(f,"%d %llu %d %llu %d %llu %s\n",me.pid,me.started,wrapper.pid,wrapper.started,job_lease.pid,job_lease.started,boot);CHECK(fclose(f)==0);
  pid_t watch=fork();CHECK(watch>=0);if(!watch)_exit(reading_watch(&me,&wrapper,30)==0?0:1);raise(SIGKILL);_exit(1);
 }
 struct pollfd pollfd={resume[0],POLLIN,0};CHECK(poll(&pollfd,1,25000)==1);CHECK(read(resume[0],&b,1)==1&&b=='R');close(resume[0]);
 CHECK(reading_quiet(job));CHECK(waitpid(job,&st,0)==job&&WIFSIGNALED(st)&&WTERMSIG(st)==SIGTERM);
 CHECK(waitpid(owner,&st,0)==owner&&WIFSIGNALED(st)&&WTERMSIG(st)==SIGKILL);
 CHECK(kill(target,SIGKILL)==0);CHECK(waitpid(target,&st,0)==target);CHECK(unlink(HANDOFF_LEASE)==0);CHECK(rmdir(HANDOFF_ROOT)==0);
 puts("HANDOFF85_REAL_WATCHDOG_OWNER_SIGKILL_CLEANS_SESSION_BEFORE_WRAPPER_RESUME_OK");
}
int main(void){lease_rejection_fixture();reading_cleanup_fixture();reading_watchdog_fixture();one(0);one(1);puts("HANDOFF84_WATCHDOG_DEADLINE_CONTROLLER_SIGKILL_AND_PID_REUSE_GUARD_OK");return 0;}
