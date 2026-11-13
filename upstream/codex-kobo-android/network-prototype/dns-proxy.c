#define _GNU_SOURCE
#include "../runtime-prototype/close-inherited-fds.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/prctl.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <grp.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
/* Host-side, unprivileged resolver for Android 7's real dnsproxyd protocol.
 * Only the inherited host default route (netId 0), no route/firewall changes.
 * Host libc performs actual DNS; no synthetic addresses or success responses.
 */
static const char path[]="/tmp/kobo-android-runtime/dev/socket/dnsproxyd";
static volatile sig_atomic_t stopped;
static int host_default;
static void stop(int sig){(void)sig;stopped=1;}
static int all(int fd,const void *p,size_t n){while(n){ssize_t k=send(fd,p,n,MSG_NOSIGNAL);if(k<=0)return -1;p=(const char*)p+k;n-=k;}return 0;}
static int be(int fd,uint32_t n){n=htonl(n);return all(fd,&n,4);}
static int blob(int fd,const void *p,size_t n){return be(fd,n)|| (n?all(fd,p,n):0);}
static void failed(int fd,int code){int32_t e=code;all(fd,"401\0",4);all(fd,&e,4);}
static int number(const char *s,int *n){char *end;errno=0;long v=strtol(s,&end,10);if(errno||!*s||*end||v< -1||v>2147483647)return -1;*n=v;return 0;}
static void query(int fd){
 alarm(9);struct timeval tv={3,0};setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&tv,sizeof(tv));setsockopt(fd,SOL_SOCKET,SO_SNDTIMEO,&tv,sizeof(tv));
 struct ucred peer;socklen_t len=sizeof(peer);
 if(getsockopt(fd,SOL_SOCKET,SO_PEERCRED,&peer,&len)||len!=sizeof(peer))return;
 if(peer.uid!=0&&peer.uid!=1000&&peer.uid!=10000&&peer.uid!=10006&&peer.uid!=10007){failed(fd,4);return;}
 char buf[1024];size_t used=0;
 while(used<sizeof(buf)){ssize_t n=recv(fd,buf+used,1,0);if(n!=1)return;if(buf[used++]==0)break;}
 if(!used||buf[used-1]){failed(fd,4);return;}
 char *args[9],*save=NULL;int count=0;
 for(char *s=strtok_r(buf," ",&save);s&&count<9;s=strtok_r(NULL," ",&save))args[count++]=s;
 int f,af,st,pr,net;
 if(count!=8||strcmp(args[0],"getaddrinfo")||strlen(args[1])>253||strlen(args[2])>16||number(args[3],&f)||number(args[4],&af)||number(args[5],&st)||number(args[6],&pr)||number(args[7],&net)||(net!=0&&!(host_default&&net==100))){failed(fd,4);return;}
 struct addrinfo h={0},*result=NULL;struct addrinfo *hp=&h;
 if(f==-1&&af==-1&&st==-1&&pr==-1)hp=NULL;
 else{
  if(f<0||(f&~0xd0f)||(af!=AF_UNSPEC&&af!=AF_INET&&af!=AF_INET6)||(st!=0&&st!=SOCK_STREAM&&st!=SOCK_DGRAM)||(pr!=0&&pr!=IPPROTO_TCP&&pr!=IPPROTO_UDP)){failed(fd,12);return;}
  h.ai_family=af;h.ai_socktype=st;h.ai_protocol=pr;
  if(f&1)h.ai_flags|=AI_PASSIVE;if(f&2)h.ai_flags|=AI_CANONNAME;if(f&4)h.ai_flags|=AI_NUMERICHOST;if(f&8)h.ai_flags|=AI_NUMERICSERV;
  if(f&0x100)h.ai_flags|=AI_ALL;if(f&0x400)h.ai_flags|=AI_ADDRCONFIG;if(f&0x800)h.ai_flags|=AI_V4MAPPED;
 }
 int rc=getaddrinfo(strcmp(args[1],"^")?args[1]:NULL,strcmp(args[2],"^")?args[2]:NULL,hp,&result);
 if(rc){failed(fd,rc==EAI_AGAIN?2:rc==EAI_NONAME?8:4);printf("HOST_DNS_LOOKUP_FAILED uid=%u resolver_error=%d\n",peer.uid,rc);return;}
 int total=0;for(struct addrinfo *a=result;a;a=a->ai_next){if(++total>64||(a->ai_family!=AF_INET&&a->ai_family!=AF_INET6)){freeaddrinfo(result);failed(fd,4);return;}}
 if(all(fd,"222\0",4)){freeaddrinfo(result);return;}
 for(struct addrinfo *a=result;a;a=a->ai_next){
  if(be(fd,1)||be(fd,hp?f:0)||be(fd,a->ai_family)||be(fd,a->ai_socktype)||be(fd,a->ai_protocol)||blob(fd,a->ai_addr,a->ai_addrlen)||blob(fd,a->ai_canonname,a->ai_canonname?strlen(a->ai_canonname)+1:0))break;
 }
 be(fd,0);freeaddrinfo(result);printf("HOST_DNS_LOOKUP_OK uid=%u answers=%d netId=%d\n",peer.uid,total,net);
}
int main(int argc,char **argv){
 int max_seconds=360;
 if(argc==2&&!strcmp(argv[1],"--host-default"))host_default=1;
 else if(argc==2&&!strcmp(argv[1],"--host-default-session")){host_default=1;max_seconds=720;}
 else if(argc==2&&!strcmp(argv[1],"--host-default-reading-session")){host_default=1;max_seconds=2400;}
 else if(argc!=1)return 2;
 if(geteuid()!=0||close_inherited_fds()<0)return 2;setvbuf(stdout,NULL,_IONBF,0);
 struct stat s;if(lstat("/tmp/kobo-android-runtime/dev/socket",&s)||!S_ISDIR(s.st_mode)||s.st_uid!=0||(s.st_mode&0022))return 3;
 if(lstat(path,&s)==0||errno!=ENOENT)return 4;
 int server=socket(AF_UNIX,SOCK_STREAM|SOCK_CLOEXEC,0);if(server<0)return 5;
 struct sockaddr_un addr={.sun_family=AF_UNIX};strcpy(addr.sun_path,path);
 if(bind(server,(struct sockaddr*)&addr,sizeof(addr))||chmod(path,0666)||listen(server,8))return 6;
 gid_t groups[]={3003};if(setgroups(1,groups)||setgid(1051)||setuid(1051)||prctl(PR_SET_NO_NEW_PRIVS,1,0,0,0))return 7;
 if(clearenv())return 8;
 signal(SIGTERM,stop);signal(SIGINT,stop);signal(SIGHUP,stop);signal(SIGPIPE,SIG_IGN);
 time_t end=time(NULL)+max_seconds;pid_t children[8]={0};printf("HOST_DNS_PROXY_READY uid=%u netId=%s max_children=8 max_seconds=%d\n",getuid(),host_default?"0,100":"0",max_seconds);
 while(!stopped&&time(NULL)<end){
  for(int i=0;i<8;i++)if(children[i]&&waitpid(children[i],NULL,WNOHANG)==children[i])children[i]=0;
  struct pollfd p={server,POLLIN,0};if(poll(&p,1,500)<=0)continue;
  int client=accept4(server,NULL,NULL,SOCK_CLOEXEC);if(client<0)continue;
  int slot=0;while(slot<8&&children[slot])slot++;if(slot==8){close(client);continue;}
  pid_t pid=fork();if(pid==0){close(server);signal(SIGTERM,SIG_DFL);signal(SIGALRM,SIG_DFL);query(client);close(client);_exit(0);}if(pid>0)children[slot]=pid;close(client);
 }
 close(server);for(int i=0;i<8;i++)if(children[i]){kill(children[i],SIGTERM);waitpid(children[i],NULL,0);}printf("HOST_DNS_PROXY_CLOSED\n");return 0;
}
