#define _GNU_SOURCE
#include "../runtime-prototype/close-inherited-fds.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/prctl.h>
#include <grp.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <stddef.h>
static int all(int fd,const char *p,size_t n){while(n){ssize_t k=write(fd,p,n);if(k<0&&errno==EINTR)continue;if(k<=0)return -1;p+=k;n-=k;}return 0;}
/* Only an already-running original WeRead UID 10007 in the fixed private root.
 * Raw stdio relay, no listening TCP port. Max duration 60 seconds. */
int main(int argc,char **argv){
 if(argc!=2||geteuid()!=0||close_inherited_fds()<0)return 2;
 char *end;long pid=strtol(argv[1],&end,10);if(!*argv[1]||*end||pid<2||pid>4194304)return 3;
 char name[128],value[512];snprintf(name,sizeof(name),"/proc/%ld/root",pid);ssize_t n=readlink(name,value,sizeof(value)-1);if(n<0)return 4;value[n]=0;if(strcmp(value,"/tmp/kobo-android-runtime"))return 5;
 snprintf(name,sizeof(name),"/proc/%ld/exe",pid);n=readlink(name,value,sizeof(value)-1);if(n<0)return 6;value[n]=0;if(strcmp(value,"/tmp/kobo-android-runtime/system/bin/app_process32"))return 7;
 snprintf(name,sizeof(name),"/proc/%ld/cmdline",pid);FILE *f=fopen(name,"rb");if(!f)return 8;n=fread(value,1,sizeof(value)-1,f);fclose(f);if(n<=0)return 9;value[n]=0;if(strcmp(value,"com.tencent.weread.eink"))return 10;
 if(setgroups(0,NULL)||setgid(10007)||setuid(10007)||prctl(PR_SET_NO_NEW_PRIVS,1,0,0,0))return 14;
 int fd=socket(AF_UNIX,SOCK_STREAM|SOCK_CLOEXEC,0);if(fd<0)return 11;
 struct sockaddr_un address={.sun_family=AF_UNIX};int len=snprintf(address.sun_path+1,sizeof(address.sun_path)-1,"webview_devtools_remote_%ld",pid);
 if(connect(fd,(struct sockaddr*)&address,offsetof(struct sockaddr_un,sun_path)+1+len))return 12;
 struct ucred peer;socklen_t size=sizeof(peer);if(getsockopt(fd,SOL_SOCKET,SO_PEERCRED,&peer,&size)||peer.uid!=10007||peer.pid!=pid)return 13;
 alarm(60);signal(SIGPIPE,SIG_IGN);int input=1;char buffer[8192];
 while(1){struct pollfd p[2]={{input?0:-1,POLLIN,0},{fd,POLLIN,0}};if(poll(p,2,1000)<0){if(errno==EINTR)continue;break;}
  if(p[0].revents&(POLLIN|POLLHUP)){n=read(0,buffer,sizeof(buffer));if(n<=0){input=0;shutdown(fd,SHUT_WR);}else if(all(fd,buffer,n))break;}
  if(p[1].revents&(POLLIN|POLLHUP|POLLERR)){n=read(fd,buffer,sizeof(buffer));if(n<=0)break;if(all(1,buffer,n))break;}
 }
 close(fd);return 0;
}
