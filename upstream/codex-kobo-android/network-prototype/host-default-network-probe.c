#define _GNU_SOURCE
#include "../runtime-prototype/close-inherited-fds.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/prctl.h>
#include <sys/time.h>
#include <grp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
struct command { uint32_t id,netid,uid; };
static int test(uint32_t id,uint32_t netid,int descriptors,int family,int expected){
 int fds[2]={-1,-1},channel=socket(AF_UNIX,SOCK_STREAM|SOCK_CLOEXEC,0);if(channel<0)return -1;
 struct timeval tv={2,0};setsockopt(channel,SOL_SOCKET,SO_RCVTIMEO,&tv,sizeof(tv));setsockopt(channel,SOL_SOCKET,SO_SNDTIMEO,&tv,sizeof(tv));
 struct sockaddr_un a={.sun_family=AF_UNIX};strcpy(a.sun_path,"/tmp/kobo-android-runtime/dev/socket/fwmarkd");
 if(connect(channel,(struct sockaddr*)&a,sizeof(a)))goto bad;
 struct command c={id,netid,0};struct iovec io={&c,sizeof(c)};struct msghdr m={0};m.msg_iov=&io;m.msg_iovlen=1;
 union{struct cmsghdr align;char bytes[CMSG_SPACE(sizeof(int)*2)];} u;
 if(descriptors){
  for(int i=0;i<descriptors;i++){fds[i]=socket(family,SOCK_DGRAM|SOCK_CLOEXEC,0);if(fds[i]<0)goto bad;}
  memset(&u,0,sizeof(u));m.msg_control=u.bytes;m.msg_controllen=CMSG_SPACE(sizeof(int)*descriptors);struct cmsghdr *h=CMSG_FIRSTHDR(&m);h->cmsg_level=SOL_SOCKET;h->cmsg_type=SCM_RIGHTS;h->cmsg_len=CMSG_LEN(sizeof(int)*descriptors);memcpy(CMSG_DATA(h),fds,sizeof(int)*descriptors);
 }
 if(sendmsg(channel,&m,MSG_NOSIGNAL)!=sizeof(c))goto bad;
 int result=999;if(recv(channel,&result,sizeof(result),MSG_WAITALL)!=sizeof(result))goto bad;
 close(channel);for(int i=0;i<2;i++)if(fds[i]>=0)close(fds[i]);
 printf("HOST_DEFAULT_PROBE uid=%u command=%u netid=%u descriptors=%d family=%d result=%d expected=%d\n",getuid(),id,netid,descriptors,family,result,expected);
 return result==expected?0:-1;
bad:close(channel);for(int i=0;i<2;i++)if(fds[i]>=0)close(fds[i]);return -1;
}
int main(int argc,char **argv){
 (void)argv;if(argc!=1||geteuid()!=0||close_inherited_fds()<0)return 2;
 gid_t groups[]={3003};if(setgroups(1,groups)||setgid(10000)||setuid(10000)||prctl(PR_SET_NO_NEW_PRIVS,1,0,0,0)||clearenv())return 3;
 setvbuf(stdout,NULL,_IONBF,0);
 if(test(2,100,1,AF_INET,0)||test(2,0,1,AF_INET,0)||test(1,0,1,AF_INET,0)||test(0,0,1,AF_INET,0)||test(2,101,1,AF_INET,-ENONET)||test(3,100,1,AF_INET,-EOPNOTSUPP)||test(2,100,0,AF_INET,-EINVAL)||test(2,100,2,AF_INET,-EINVAL)||test(2,100,1,AF_UNIX,-EOPNOTSUPP))return 4;
 printf("HOST_DEFAULT_ORDINARY_UID_PROTOCOL_POSITIVE_AND_NEGATIVE_TESTS_OK\n");return 0;
}
