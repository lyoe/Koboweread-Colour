// SPDX-License-Identifier: GPL-3.0-or-later
/* Socket-pair regression check only; never open the physical input device. */
#define main physical_touch_program_main
#include "touch-forward.c"
#undef main
#include <sys/wait.h>
static int whole_read(int fd,void *data,size_t count){
 char *p=data;while(count){ssize_t n=read(fd,p,count);if(n<0&&errno==EINTR)continue;if(n<=0)return -1;p+=n;count-=n;}return 0;
}
int main(void){
 int sockets[2];if(socketpair(AF_UNIX,SOCK_STREAM,0,sockets))return 2;
 pid_t pid=fork();if(pid<0)return 3;
 if(!pid){close(sockets[0]);unsigned replies[]={1,2,3,7};for(unsigned i=0;i<4;i++){
  uint32_t words[5];if(whole_read(sockets[1],words,sizeof(words)) || ntohl(words[0])!=0x4b545031 || ntohl(words[4])!=i+1)_exit(4);
  uint32_t ack=htonl(replies[i]);if(write(sockets[1],&ack,4)!=4)_exit(5);
 }close(sockets[1]);_exit(0);}
 close(sockets[1]);unsigned serial=0;int expected[]={0,2,3,-1};
 for(unsigned i=0;i<4;i++)if(send_event(sockets[0],0,1,2,&serial)!=expected[i])return 6;
 close(sockets[0]);int status;if(waitpid(pid,&status,0)!=pid||!WIFEXITED(status)||WEXITSTATUS(status))return 7;
 if(skipped_events!=1 || rejected_events!=1)return 8;
 puts("PHYSICAL_TOUCH_ACK_PROTOCOL_OK accepted skipped rejected invalid_ack distinct_counters no_physical_input_opened");return 0;
}
