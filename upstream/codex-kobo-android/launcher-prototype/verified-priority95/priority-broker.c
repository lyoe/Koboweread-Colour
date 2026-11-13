#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <poll.h>
#include <errno.h>
#include <signal.h>
#include "priority-policy.h"
#define ROOT "/tmp/kobo-android-runtime"
#define SOCK ROOT "/dev/socket/lmkd"
static volatile sig_atomic_t stopped;
static void stop95(int n){(void)n;stopped=1;}
static int link_is(int fd,const char *entry,const char *expected){char value[256];ssize_t n=readlinkat(fd,entry,value,sizeof(value)-1);if(n<0)return 0;value[n]=0;return !strcmp(value,expected);}
static int process_fd(int pid,int uid,int peer){
 char path[64];snprintf(path,sizeof(path),"/proc/%d",pid);int fd=open(path,O_DIRECTORY|O_RDONLY|O_CLOEXEC);if(fd<0)return -1;struct stat st;
 int ok=fstat(fd,&st)==0&&(uid<0||(int)st.st_uid==uid)&&link_is(fd,"root",ROOT);
 if(ok){ok=link_is(fd,"exe",ROOT "/system/bin/app_process32");if(peer&&uid==0)ok=ok||link_is(fd,"exe",ROOT "/lab/priority95-client");}
 if(!ok){close(fd);return -1;}return fd;
}
static int apply95(struct request95 r){
 int fd=process_fd(r.pid,r.uid,0);if(fd<0)return -1;
 // Open through the validated proc-directory FD so PID reuse cannot retarget this write.
 int out=openat(fd,"oom_score_adj",O_RDWR|O_CLOEXEC|O_NOFOLLOW);if(out<0){close(fd);return -1;}
 char value[32];int n=snprintf(value,sizeof(value),"%d\n",r.score);int ok=write(out,value,(size_t)n)==n;
 if(ok){lseek(out,0,SEEK_SET);int got=read(out,value,sizeof(value)-1);if(got<=0)ok=0;else{value[got]=0;ok=atoi(value)==r.score;}}
 close(out);close(fd);return ok?0:-1;
}
int main(void){
 if(geteuid()!=0)return 2;struct stat existing;if(lstat(SOCK,&existing)==0||errno!=ENOENT)return 3;
 setvbuf(stdout,0,_IOLBF,0);signal(SIGTERM,stop95);signal(SIGINT,stop95);signal(SIGHUP,stop95);signal(SIGPIPE,SIG_IGN);
 int server=socket(AF_UNIX,SOCK_SEQPACKET|SOCK_CLOEXEC|SOCK_NONBLOCK,0);if(server<0)return 4;
 struct sockaddr_un addr={.sun_family=AF_UNIX};strcpy(addr.sun_path,SOCK);
 if(bind(server,(void*)&addr,sizeof(addr))||chown(SOCK,0,1000)||chmod(SOCK,0660)||listen(server,4)){close(server);unlink(SOCK);return 5;}
 struct stat socket_identity;if(lstat(SOCK,&socket_identity)){close(server);return 6;}
 struct pollfd p[5]={{server,POLLIN,0},{-1,POLLIN,0},{-1,POLLIN,0},{-1,POLLIN,0},{-1,POLLIN,0}};
 puts("PRIORITY95_READY kernel_oom_priority=1 pressure_kill=0 protocol=api24");
 while(!stopped){int n=poll(p,5,200);if(n<0&&errno==EINTR)continue;if(n<0)break;
  if(p[0].revents&POLLIN){int c=accept4(server,0,0,SOCK_CLOEXEC|SOCK_NONBLOCK);if(c>=0){struct ucred cr;socklen_t len=sizeof(cr);int fd=-1;if(!getsockopt(c,SOL_SOCKET,SO_PEERCRED,&cr,&len)&&(cr.uid==0||cr.uid==1000))fd=process_fd(cr.pid,(int)cr.uid,1);int slot=1;while(slot<5&&p[slot].fd>=0)slot++;if(fd<0||slot==5){puts("PRIORITY95_PEER_REJECTED");close(c);}else p[slot].fd=c;if(fd>=0)close(fd);}}
  for(int i=1;i<5;i++)if(p[i].fd>=0&&p[i].revents){
   if(p[i].revents&POLLIN){uint32_t data[14];ssize_t got=recv(p[i].fd,data,sizeof(data),MSG_DONTWAIT);struct request95 r;
    if(got<=0||decode95(data,(size_t)got,&r)){close(p[i].fd);p[i].fd=-1;continue;}
    if(r.command==0)printf("PRIORITY95_TARGET_ADVISORY pairs=%d pressure_kill=unavailable kernel_oom=unchanged\n",r.pairs);
    else if(r.command==2)printf("PRIORITY95_REMOVE_TRACKING pid=%d tracked_kill_list=none\n",r.pid);
    else printf("PRIORITY95_APPLY pid=%d uid=%d score=%d result=%s\n",r.pid,r.uid,r.score,apply95(r)==0?"verified":"rejected_or_exited");
   }
   if(p[i].fd>=0&&(p[i].revents&(POLLERR|POLLHUP|POLLNVAL))){close(p[i].fd);p[i].fd=-1;}
  }
 }
 for(int i=0;i<5;i++)if(p[i].fd>=0)close(p[i].fd);
 struct stat current;if(!lstat(SOCK,&current)&&current.st_dev==socket_identity.st_dev&&current.st_ino==socket_identity.st_ino)unlink(SOCK);
 puts("PRIORITY95_CLOSED");return 0;
}
