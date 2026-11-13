#define _GNU_SOURCE
#include "../runtime-prototype/close-inherited-fds.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/prctl.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <resolv.h>
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
#ifdef KOBO_DNS_TEST_PRIMARY_AGAIN
/* Lab self-test build only: the host resolver always "times out" for names. */
static int test_getaddrinfo(const char *n,const char *s,const struct addrinfo *h,struct addrinfo **r){
 if(!h||!(h->ai_flags&AI_NUMERICHOST))return EAI_AGAIN;return getaddrinfo(n,s,h,r);
}
#define getaddrinfo test_getaddrinfo
#endif
static const char path[]="/tmp/kobo-android-runtime/dev/socket/dnsproxyd";
static volatile sig_atomic_t stopped;
static int host_default;
/* Resident launcher only (--host-default-resident): when the host resolver
 * (the router) returns EAI_AGAIN three times in a row, children ask public
 * resolvers first and the router is re-probed every 60 s; one primary success
 * switches back. The child's exit code reports what happened to the parent. */
enum { MODE_PRIMARY, MODE_FALLBACK_FIRST, MODE_PROBE };
enum { STATUS_OTHER=0, STATUS_PRIMARY_OK=1, STATUS_PRIMARY_AGAIN=10, STATUS_FALLBACK_SERVED=20 };
static int resident,lookup_mode,lookup_status;
static const char *const fallback_servers[]={"223.5.5.5","119.29.29.29"};
/* One UDP question to one server; a connected socket only receives that
 * server's replies. Returns addresses found, 0 with *rcode set, or -1. */
static int ask(const char *server,const char *name,int type,char out[][INET6_ADDRSTRLEN],int max,int *rcode){
 unsigned char q[512],r[1500];int ql=res_mkquery(0,name,1,type,NULL,0,NULL,q,sizeof(q));if(ql<12)return -1;
 int sock=socket(AF_INET,SOCK_DGRAM|SOCK_CLOEXEC,0);if(sock<0)return -1;
 struct sockaddr_in a={.sin_family=AF_INET,.sin_port=htons(53)};inet_pton(AF_INET,server,&a.sin_addr);
 struct timeval tv={1,500000};setsockopt(sock,SOL_SOCKET,SO_RCVTIMEO,&tv,sizeof(tv));
 int n=-1;
 if(!connect(sock,(struct sockaddr*)&a,sizeof(a))&&send(sock,q,ql,0)==ql){
  for(;;){n=recv(sock,r,sizeof(r),0);if(n<12){n=-1;break;}if(r[0]==q[0]&&r[1]==q[1]&&(r[2]&0x80))break;}
 }
 close(sock);if(n<12||(r[2]&2))return -1;
 *rcode=r[3]&15;if(*rcode)return 0;
 int qd=r[4]<<8|r[5],an=r[6]<<8|r[7],found=0;const unsigned char *at=r+12,*end=r+n;
 while(qd-->0){int k=dn_skipname(at,end);if(k<0||at+k+4>end)return -1;at+=k+4;}
 while(an-->0&&found<max){
  int k=dn_skipname(at,end);if(k<0||at+k+10>end)return -1;at+=k;
  int t=at[0]<<8|at[1],len=at[8]<<8|at[9];at+=10;if(at+len>end)return -1;
  if(t==1&&len==4)inet_ntop(AF_INET,at,out[found++],INET6_ADDRSTRLEN);
  else if(t==28&&len==16)inet_ntop(AF_INET6,at,out[found++],INET6_ADDRSTRLEN);
  at+=len;
 }
 return found;
}
/* Same result shape as getaddrinfo: each answer goes through a numeric lookup
 * with the caller's hints. The child exits right after replying, so the
 * concatenated list is not freed. */
static int fallback_lookup(const char *name,const char *service,const struct addrinfo *hints,struct addrinfo **result,const char **used){
 if(!name)return EAI_NONAME;
 int af=hints?hints->ai_family:AF_UNSPEC;
 for(unsigned s=0;s<sizeof(fallback_servers)/sizeof(*fallback_servers);s++){
  char ips[32][INET6_ADDRSTRLEN];int count=0,answered=0,missing=0;
  const int types[2]={af==AF_INET6?0:1,af==AF_INET?0:28};
  for(int i=0;i<2;i++){
   if(!types[i])continue;int rcode=0,k=ask(fallback_servers[s],name,types[i],ips+count,32-count,&rcode);
   if(k<0)continue;answered=1;if(rcode==3)missing=1;else if(!rcode)count+=k;
  }
  if(!answered)continue;
  *used=fallback_servers[s];
  struct addrinfo h={0};if(hints)h=*hints;h.ai_flags=(h.ai_flags&~(AI_ADDRCONFIG|AI_CANONNAME))|AI_NUMERICHOST;
  struct addrinfo *head=NULL,**tail=&head;
  for(int i=0;i<count;i++){struct addrinfo *one=NULL;if(!getaddrinfo(ips[i],service,&h,&one)){*tail=one;while(*tail)tail=&(*tail)->ai_next;}}
  if(!head)return missing||!count?EAI_NONAME:EAI_FAIL;
  *result=head;return 0;
 }
 return EAI_AGAIN;
}
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
 const char *name=strcmp(args[1],"^")?args[1]:NULL,*service=strcmp(args[2],"^")?args[2]:NULL,*via=NULL;int rc,own=0;
 if(resident&&lookup_mode==MODE_FALLBACK_FIRST){
  rc=fallback_lookup(name,service,hp,&result,&via);
  if(!rc){own=1;lookup_status=STATUS_FALLBACK_SERVED;}
  else if(rc==EAI_AGAIN){via=NULL;rc=getaddrinfo(name,service,hp,&result);lookup_status=rc==EAI_AGAIN?STATUS_PRIMARY_AGAIN:STATUS_PRIMARY_OK;}
 }else{
  rc=getaddrinfo(name,service,hp,&result);
  lookup_status=rc==EAI_AGAIN?STATUS_PRIMARY_AGAIN:STATUS_PRIMARY_OK;
  if(rc==EAI_AGAIN&&resident&&lookup_mode==MODE_PROBE){int retry=fallback_lookup(name,service,hp,&result,&via);if(retry!=EAI_AGAIN){rc=retry;own=!retry;}}
 }
 if(rc){failed(fd,rc==EAI_AGAIN?2:rc==EAI_NONAME?8:4);printf("HOST_DNS_LOOKUP_FAILED uid=%u resolver_error=%d%s%s\n",peer.uid,rc,via?" via=":"",via?via:"");return;}
 int total=0;for(struct addrinfo *a=result;a;a=a->ai_next){if(++total>64||(a->ai_family!=AF_INET&&a->ai_family!=AF_INET6)){if(!own)freeaddrinfo(result);failed(fd,4);return;}}
 if(all(fd,"222\0",4)){if(!own)freeaddrinfo(result);return;}
 for(struct addrinfo *a=result;a;a=a->ai_next){
  if(be(fd,1)||be(fd,hp?f:0)||be(fd,a->ai_family)||be(fd,a->ai_socktype)||be(fd,a->ai_protocol)||blob(fd,a->ai_addr,a->ai_addrlen)||blob(fd,a->ai_canonname,a->ai_canonname?strlen(a->ai_canonname)+1:0))break;
 }
 be(fd,0);if(!own)freeaddrinfo(result);printf("HOST_DNS_LOOKUP_OK uid=%u answers=%d netId=%d%s%s\n",peer.uid,total,net,via?" via=fallback:":"",via?via:"");
}
int main(int argc,char **argv){
 int max_seconds=360;
 if(argc==2&&!strcmp(argv[1],"--host-default"))host_default=1;
 else if(argc==2&&!strcmp(argv[1],"--host-default-session")){host_default=1;max_seconds=720;}
 else if(argc==2&&!strcmp(argv[1],"--host-default-reading-session")){host_default=1;max_seconds=2400;}
 else if(argc==2&&!strcmp(argv[1],"--host-default-resident")){host_default=1;resident=1;max_seconds=2592000;}
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
 time_t end=time(NULL)+max_seconds;pid_t children[8]={0};int again=0,fallback=0;time_t next_probe=0;printf("HOST_DNS_PROXY_READY uid=%u netId=%s max_children=8 max_seconds=%d\n",getuid(),host_default?"0,100":"0",max_seconds);
 while(!stopped&&time(NULL)<end){
  for(int i=0;i<8;i++){
   int status=0;if(!children[i]||waitpid(children[i],&status,WNOHANG)!=children[i])continue;children[i]=0;
   int code=WIFEXITED(status)?WEXITSTATUS(status):STATUS_OTHER;
   if(!resident)continue;
   if(code==STATUS_PRIMARY_OK){again=0;if(fallback){fallback=0;printf("HOST_DNS_PRIMARY_RECOVERED\n");}}
   else if(code==STATUS_PRIMARY_AGAIN&&++again>=3&&!fallback){fallback=1;next_probe=time(NULL)+60;printf("HOST_DNS_FALLBACK_ACTIVE consecutive_primary_failures=%d servers=223.5.5.5,119.29.29.29\n",again);}
  }
  struct pollfd p={server,POLLIN,0};if(poll(&p,1,500)<=0)continue;
  int client=accept4(server,NULL,NULL,SOCK_CLOEXEC);if(client<0)continue;
  int slot=0;while(slot<8&&children[slot])slot++;if(slot==8){close(client);continue;}
  lookup_mode=MODE_PRIMARY;if(fallback){lookup_mode=MODE_FALLBACK_FIRST;if(time(NULL)>=next_probe){lookup_mode=MODE_PROBE;next_probe=time(NULL)+60;}}
  pid_t pid=fork();if(pid==0){close(server);signal(SIGTERM,SIG_DFL);signal(SIGALRM,SIG_DFL);lookup_status=STATUS_OTHER;query(client);close(client);_exit(lookup_status);}if(pid>0)children[slot]=pid;close(client);
 }
 close(server);for(int i=0;i<8;i++)if(children[i]){kill(children[i],SIGTERM);waitpid(children[i],NULL,0);}printf("HOST_DNS_PROXY_CLOSED\n");return 0;
}
