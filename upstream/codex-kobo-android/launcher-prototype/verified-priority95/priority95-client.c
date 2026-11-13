#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <string.h>
#include <time.h>
static int score(int pid){char path[80];snprintf(path,sizeof(path),"/proc/%d/oom_score_adj",pid);FILE*f=fopen(path,"r");int n=2000;if(f){if(fscanf(f,"%d",&n)!=1)n=2000;fclose(f);}return n;}
static int send_priority(int sock,int pid,int uid,int value){uint32_t p[]={htonl(1),htonl((uint32_t)pid),htonl((uint32_t)uid),htonl((uint32_t)value)};return send(sock,p,sizeof(p),MSG_NOSIGNAL)==sizeof(p)?0:-1;}
static int await(int pid,int value){for(int i=0;i<100;i++){if(score(pid)==value)return 0;usleep(10000);}return -1;}
int main(int argc,char**argv){
 if(getuid()!=0||argc!=3)return 2;int pid=atoi(argv[1]),uid=atoi(argv[2]);if(pid<=1||uid!=10000)return 2;
 int old=score(pid);if(old< -1000||old>1000)return 3;int value=old>0?old-1:old+1;
 int s=socket(AF_UNIX,SOCK_SEQPACKET,0);struct sockaddr_un addr={.sun_family=AF_UNIX};strcpy(addr.sun_path,"/dev/socket/lmkd");struct timeval t={1,0};setsockopt(s,SOL_SOCKET,SO_SNDTIMEO,&t,sizeof(t));
 if(s<0||connect(s,(void*)&addr,sizeof(addr)))return 4;
 int sent=send_priority(s,pid,uid,value);int changed=sent==0&&await(pid,value)==0;
 int restored=send_priority(s,pid,uid,old)==0&&await(pid,old)==0;close(s);
 printf("PRIORITY95_ACTUAL_KERNEL pid=%d uid=%d before=%d trial=%d changed=%d restored=%d\n",pid,uid,old,value,changed,restored);return changed&&restored?0:5;
}
