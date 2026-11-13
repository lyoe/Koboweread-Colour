// SPDX-License-Identifier: GPL-3.0-or-later
/* resident-dns-check HOST: one lookup through the session's own DNS proxy
 * (Android's dnsproxyd protocol, netId 0), as the resident loop's health check.
 * Exit 0 only for a successful answer; prints the result and elapsed time. */
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <time.h>
#include <unistd.h>
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec/1e9;}
int main(int argc,char **argv){
 if(argc!=2||strlen(argv[1])>253||strchr(argv[1],' '))return 2;
 double began=now();int s=socket(AF_UNIX,SOCK_STREAM|SOCK_CLOEXEC,0);if(s<0)return 3;
 struct sockaddr_un a={.sun_family=AF_UNIX};strcpy(a.sun_path,"/tmp/kobo-android-runtime/dev/socket/dnsproxyd");
 struct timeval tv={12,0};setsockopt(s,SOL_SOCKET,SO_RCVTIMEO,&tv,sizeof(tv));setsockopt(s,SOL_SOCKET,SO_SNDTIMEO,&tv,sizeof(tv));
 if(connect(s,(struct sockaddr*)&a,sizeof(a))){puts("RESIDENT_DNS_CHECK result=no-proxy");return 4;}
 char q[300];int n=snprintf(q,sizeof(q),"getaddrinfo %s ^ -1 -1 -1 -1 0",argv[1]);
 char r[4]={0};int ok=send(s,q,n+1,MSG_NOSIGNAL)==n+1&&recv(s,r,4,MSG_WAITALL)==4;close(s);
 const char *result=!ok?"timeout":!memcmp(r,"222",4)?"ok":!memcmp(r,"401",4)?"failed":"invalid";
 printf("RESIDENT_DNS_CHECK result=%s host=%s seconds=%.2f\n",result,argv[1],now()-began);
 return strcmp(result,"ok")?1:0;
}
