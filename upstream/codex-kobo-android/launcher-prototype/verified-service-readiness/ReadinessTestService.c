#define _DEFAULT_SOURCE
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
int main(int argc,char **argv){
 if(argc!=4)return 2;
 if(!strcmp(argv[1],"socket-delayed")){puts("TEST_SERVICE_READY");fflush(stdout);usleep(400000);}
 int fd=socket(AF_UNIX,SOCK_STREAM,0);if(fd<0)return 3;
 struct sockaddr_un a={.sun_family=AF_UNIX};if(strlen(argv[2])>=sizeof(a.sun_path))return 4;
 strcpy(a.sun_path,argv[2]);if(bind(fd,(void*)&a,sizeof(a))||listen(fd,1))return 5;
 FILE *resource=fopen(argv[3],"w");if(!resource)return 6;fputs("ready\n",resource);fclose(resource);
 if(!strcmp(argv[1],"marker-delayed")){usleep(400000);puts("TEST_SERVICE_READY");fflush(stdout);}
 while(1)pause();
}
