#define _GNU_SOURCE
#include "../runtime-prototype/close-inherited-fds.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/prctl.h>
#include <sys/time.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <ifaddrs.h>
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
/* One logical Android network, 100, aliases the existing unmarked host route.
 * No setsockopt(SO_MARK), routing changes, VPN, validation or Internet guarantee.
 * Protocol: AOSP netd android-7.0.0_r1 FwmarkCommand / FwmarkClient.
 */
#ifdef KOBO_TARGET_LIBRA2
#define HOST_WIFI_INTERFACE "wlan0"
#else
#define HOST_WIFI_INTERFACE "eth0"
#endif
static const char dir[]="/tmp/kobo-android-runtime/lab/network-state";
static const char socket_path[]="/tmp/kobo-android-runtime/dev/socket/fwmarkd";
static volatile sig_atomic_t stopped;
struct state { int connected,prefix,mtu; char iface[IFNAMSIZ],address[INET_ADDRSTRLEN],gateway[INET_ADDRSTRLEN],dns[4][INET_ADDRSTRLEN]; int dns_count; };
struct command { uint32_t id,netid,uid; };
_Static_assert(sizeof(struct command)==12,"API24 fwmark ABI");
static void stop(int sig){(void)sig;stopped=1;}
static long long millis(void){struct timespec t;if(clock_gettime(CLOCK_MONOTONIC,&t))return -1;return (long long)t.tv_sec*1000+t.tv_nsec/1000000;}
static int observe(struct state *s){
 memset(s,0,sizeof(*s));
 FILE *f=fopen("/proc/net/route","r");if(!f)return -1;
 char line[512],iface[IFNAMSIZ];unsigned long dest,gw,flags,metric,mask;int ref,use;unsigned long best=~0UL;
 while(fgets(line,sizeof(line),f))if(sscanf(line,"%15s %lx %lx %lx %d %d %lu %lx",iface,&dest,&gw,&flags,&ref,&use,&metric,&mask)==8 && !dest && !mask && (flags&3)==3 && metric<best){
  /* Pin the observed interface to the selected hardware profile. */
  if(strcmp(iface,HOST_WIFI_INTERFACE))continue;
  best=metric;strcpy(s->iface,iface);struct in_addr a={.s_addr=(uint32_t)gw};inet_ntop(AF_INET,&a,s->gateway,sizeof(s->gateway));
 }
 fclose(f);if(!s->iface[0])return 0;
 struct ifaddrs *all=NULL;if(getifaddrs(&all))return -1;
 for(struct ifaddrs *a=all;a;a=a->ifa_next){
  if(!a->ifa_addr||!a->ifa_netmask||a->ifa_addr->sa_family!=AF_INET||strcmp(a->ifa_name,s->iface)||(a->ifa_flags&(IFF_UP|IFF_RUNNING))!=(IFF_UP|IFF_RUNNING))continue;
  uint32_t bits=ntohl(((struct sockaddr_in*)a->ifa_netmask)->sin_addr.s_addr);int prefix=0;while(bits&0x80000000u){prefix++;bits<<=1;}if(bits)continue;
  const struct in_addr *ip=&((struct sockaddr_in*)a->ifa_addr)->sin_addr;if(!ip->s_addr)continue;
  inet_ntop(AF_INET,ip,s->address,sizeof(s->address));s->prefix=prefix;s->connected=1;break;
 }
 freeifaddrs(all);
 int fd=socket(AF_INET,SOCK_DGRAM|SOCK_CLOEXEC,0);if(fd>=0){struct ifreq q={0};strcpy(q.ifr_name,s->iface);if(!ioctl(fd,SIOCGIFMTU,&q))s->mtu=q.ifr_mtu;close(fd);}
 f=fopen("/etc/resolv.conf","r");if(f){char word[32],ip[64];while(fgets(line,sizeof(line),f)&&s->dns_count<4)if(sscanf(line,"%31s %63s",word,ip)==2&&!strcmp(word,"nameserver")){struct in_addr a;if(inet_pton(AF_INET,ip,&a)==1)strcpy(s->dns[s->dns_count++],ip);}fclose(f);}
 return 0;
}
static int describe(FILE *f,const struct state *s){
 if(fprintf(f,"version=1\nobserved_uptime_ms=%lld\nconnected=%d\nnetid=100\ninterface=%s\naddress=%s\nprefix=%d\ngateway=%s\nmtu=%d\ndns_count=%d\n",millis(),s->connected,s->iface,s->address,s->prefix,s->gateway,s->mtu,s->dns_count)<0)return -1;
 for(int i=0;i<s->dns_count;i++)if(fprintf(f,"dns%d=%s\n",i,s->dns[i])<0)return -1;
 return ferror(f)?-1:0;
}
static int publish(const struct state *s){
 char temporary[256],final[256];snprintf(temporary,sizeof(temporary),"%s/pending",dir);snprintf(final,sizeof(final),"%s/host-default.properties",dir);
 int fd=open(temporary,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC|O_NOFOLLOW,0644);if(fd<0)return -1;
 FILE *f=fdopen(fd,"w");if(!f){close(fd);unlink(temporary);return -1;}
 int bad=describe(f,s);if(fclose(f))bad=-1;if(!bad&&rename(temporary,final))bad=-1;if(bad)unlink(temporary);return bad;
}
static int permitted(uid_t uid){return uid==0||uid==1000||uid==10000||uid==10006||uid==10007;}
static void query(int client){
 struct timeval tv={1,0};setsockopt(client,SOL_SOCKET,SO_RCVTIMEO,&tv,sizeof(tv));setsockopt(client,SOL_SOCKET,SO_SNDTIMEO,&tv,sizeof(tv));
 struct ucred peer;socklen_t len=sizeof(peer);if(getsockopt(client,SOL_SOCKET,SO_PEERCRED,&peer,&len)||len!=sizeof(peer)||!permitted(peer.uid))return;
 struct command c={0};struct iovec io={&c,sizeof(c)};
 union { struct cmsghdr align; char bytes[CMSG_SPACE(sizeof(int)*16)]; } ancillary;
 memset(&ancillary,0,sizeof(ancillary));struct msghdr m={0};m.msg_iov=&io;m.msg_iovlen=1;m.msg_control=ancillary.bytes;m.msg_controllen=sizeof(ancillary.bytes);
 ssize_t n=recvmsg(client,&m,MSG_CMSG_CLOEXEC);int fd=-1,count=0,bad=0;
 for(struct cmsghdr *h=CMSG_FIRSTHDR(&m);h;h=CMSG_NXTHDR(&m,h)){
  if(h->cmsg_level!=SOL_SOCKET||h->cmsg_type!=SCM_RIGHTS||h->cmsg_len<CMSG_LEN(0)){bad=1;continue;}
  size_t bytes=h->cmsg_len-CMSG_LEN(0);if(bytes%sizeof(int))bad=1;
  int *v=(int*)CMSG_DATA(h);for(size_t i=0;i<bytes/sizeof(int);i++){count++;if(fd<0)fd=v[i];else close(v[i]);}
 }
 int result=-EINVAL;
 if(n==sizeof(c)&&!(m.msg_flags&(MSG_TRUNC|MSG_CTRUNC))&&!bad&&count==1){
  struct state s;int domain=0,type=0,mark=-1;socklen_t dl=sizeof(domain),tl=sizeof(type),ml=sizeof(mark);
  if(getsockopt(fd,SOL_SOCKET,SO_DOMAIN,&domain,&dl)||getsockopt(fd,SOL_SOCKET,SO_TYPE,&type,&tl)||getsockopt(fd,SOL_SOCKET,SO_MARK,&mark,&ml))result=-ENOTSOCK;
  else if(domain!=AF_INET||(type!=SOCK_STREAM&&type!=SOCK_DGRAM)||mark!=0)result=-EOPNOTSUPP;
  else if(c.id>2)result=-EOPNOTSUPP;
  else if((c.id==2&&c.netid!=0&&c.netid!=100)||(c.id!=2&&c.netid!=0))result=-ENONET;
  else if(c.uid!=0)result=-EINVAL;
  else if(c.id==2&&c.netid==0)result=0;
  else if(observe(&s)||!s.connected)result=-ENONET;
  else result=0; /* The verified unmarked IPv4 socket already uses this route. */
 }
 if(fd>=0)close(fd);
 send(client,&result,sizeof(result),MSG_NOSIGNAL);
 printf("HOST_DEFAULT_SOCKET_QUERY uid=%u command=%u netid=%u result=%d\n",peer.uid,c.id,c.netid,result);
}
int main(int argc,char **argv){
 if(argc==2&&!strcmp(argv[1],"--snapshot")){struct state s;if(observe(&s))return 1;return describe(stdout,&s)?1:0;}
 int max_seconds=360;
 if(argc==2&&!strcmp(argv[1],"--session"))max_seconds=720;else if(argc==2&&!strcmp(argv[1],"--reading-session"))max_seconds=2400;else if(argc!=1)return 2;
 if(geteuid()!=0||close_inherited_fds()<0)return 2;
 setvbuf(stdout,NULL,_IONBF,0);umask(0022);struct stat s;
 const char *parents[]={"/tmp/kobo-android-runtime/lab","/tmp/kobo-android-runtime/dev/socket"};
 for(unsigned i=0;i<2;i++)if(lstat(parents[i],&s)||!S_ISDIR(s.st_mode)||s.st_uid!=0||(s.st_mode&0022))return 3;
 if(mkdir(dir,0755)||chown(dir,1051,1051))return 4;
 if(lstat(socket_path,&s)==0||errno!=ENOENT)return 5;
 int server=socket(AF_UNIX,SOCK_STREAM|SOCK_CLOEXEC,0);if(server<0)return 6;
 struct sockaddr_un a={.sun_family=AF_UNIX};strcpy(a.sun_path,socket_path);
 if(bind(server,(struct sockaddr*)&a,sizeof(a))||chmod(socket_path,0666)||listen(server,8))return 7;
 gid_t groups[]={3003};if(setgroups(1,groups)||setgid(1051)||setuid(1051)||prctl(PR_SET_NO_NEW_PRIVS,1,0,0,0)||clearenv())return 8;
 signal(SIGTERM,stop);signal(SIGINT,stop);signal(SIGHUP,stop);signal(SIGPIPE,SIG_IGN);
 long long end=millis()+max_seconds*1000LL,next=0;printf("HOST_DEFAULT_NETWORK_READY uid=%u netid=100 max_seconds=%d\n",getuid(),max_seconds);
 while(!stopped&&millis()<end){
  if(millis()>=next){struct state state;if(observe(&state))memset(&state,0,sizeof(state));if(publish(&state)){perror("HOST_DEFAULT_STATE_PUBLISH_FAILED");return 9;}next=millis()+1000;}
  struct pollfd p={server,POLLIN,0};if(poll(&p,1,200)<=0)continue;
  int client=accept4(server,NULL,NULL,SOCK_CLOEXEC);if(client>=0){query(client);close(client);}
 }
 close(server);printf("HOST_DEFAULT_NETWORK_CLOSED\n");return 0;
}
