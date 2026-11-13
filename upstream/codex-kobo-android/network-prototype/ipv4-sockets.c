#define _GNU_SOURCE
#include <sys/socket.h>
#include <sys/syscall.h>
#include <netinet/in.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
/* Bionic's errno accessor differs from the build host's musl headers. */
extern int *__errno(void);
#undef errno
#define errno (*__errno())
/* Android 7 creates dual-stack AF_INET6 sockets even for IPv4 destinations.
 * If this kernel rejects AF_INET6, use a real AF_INET socket. Only IPv4-mapped
 * addresses and the IPv6 bind wildcard have an IPv4 equivalent. Native IPv6
 * destinations still fail with EAFNOSUPPORT. No TLS/DNS hooks or network marks.
 */
static int family(int fd){int f=0;socklen_t n=sizeof(f);if(syscall(SYS_getsockopt,fd,SOL_SOCKET,SO_DOMAIN,&f,&n)<0)return -1;return f;}
int socket(int domain,int type,int protocol){
 int fd=(int)syscall(SYS_socket,domain,type,protocol);
 if(fd<0&&domain==AF_INET6&&errno==EAFNOSUPPORT&&((type&0xf)==SOCK_STREAM||(type&0xf)==SOCK_DGRAM))fd=(int)syscall(SYS_socket,AF_INET,type,protocol);
 return fd;
}
static int convert(int fd,const struct sockaddr *in,socklen_t len,struct sockaddr_in *out,int binding){
 if(!in||len<sizeof(sa_family_t)||in->sa_family!=AF_INET6||family(fd)!=AF_INET)return 0;
 if(len<sizeof(struct sockaddr_in6)){errno=EINVAL;return -1;}
 const struct sockaddr_in6 *a=(const struct sockaddr_in6*)in;
 if(a->sin6_scope_id){errno=EAFNOSUPPORT;return -1;}
 memset(out,0,sizeof(*out));out->sin_family=AF_INET;out->sin_port=a->sin6_port;
 if(IN6_IS_ADDR_V4MAPPED(&a->sin6_addr)){memcpy(&out->sin_addr,&a->sin6_addr.s6_addr[12],4);return 1;}
 if(binding&&IN6_IS_ADDR_UNSPECIFIED(&a->sin6_addr))return 1;
 errno=EAFNOSUPPORT;return -1;
}
int connect(int fd,const struct sockaddr *a,socklen_t n){struct sockaddr_in v4;int r=convert(fd,a,n,&v4,0);if(r<0)return -1;return (int)syscall(SYS_connect,fd,r?(const struct sockaddr*)&v4:a,r?sizeof(v4):n);}
int bind(int fd,const struct sockaddr *a,socklen_t n){struct sockaddr_in v4;int r=convert(fd,a,n,&v4,1);if(r<0)return -1;return (int)syscall(SYS_bind,fd,r?(const struct sockaddr*)&v4:a,r?sizeof(v4):n);}
ssize_t sendto(int fd,const void *data,size_t size,int flags,const struct sockaddr *a,socklen_t n){struct sockaddr_in v4;int r=convert(fd,a,n,&v4,0);if(r<0)return -1;return syscall(SYS_sendto,fd,data,size,flags,r?(const struct sockaddr*)&v4:a,r?sizeof(v4):n);}
int setsockopt(int fd,int level,int option,const void *value,socklen_t n){
 if(level==IPPROTO_IPV6&&family(fd)==AF_INET){
  if(option==IPV6_V6ONLY&&n==sizeof(int)&&value&&*(const int*)value==0)return 0; /* Already IPv4-capable. */
  if(option==IPV6_TCLASS){level=IPPROTO_IP;option=IP_TOS;}else{errno=ENOPROTOOPT;return -1;}
 }
 return (int)syscall(SYS_setsockopt,fd,level,option,value,n);
}
int getsockopt(int fd,int level,int option,void *value,socklen_t *n){
 if(level==IPPROTO_IPV6&&family(fd)==AF_INET){
  if(option==IPV6_TCLASS){level=IPPROTO_IP;option=IP_TOS;}else{errno=ENOPROTOOPT;return -1;}
 }
 return (int)syscall(SYS_getsockopt,fd,level,option,value,n);
}
