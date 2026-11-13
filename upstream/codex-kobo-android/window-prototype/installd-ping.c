#define _GNU_SOURCE
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
static int transfer(int fd,void *data,size_t count,int sending){
 char *p=data;while(count){ssize_t n=sending?write(fd,p,count):read(fd,p,count);if(n<0&&errno==EINTR)continue;if(n<=0)return -1;p+=n;count-=(size_t)n;}return 0;
}
static int command(int fd,const char *text){
 uint16_t n=(uint16_t)strlen(text);char reply[1024];
 if(transfer(fd,&n,2,1)||transfer(fd,(void*)text,n,1)||transfer(fd,&n,2,0)||!n||n>=sizeof(reply)||transfer(fd,reply,n,0))return -1;
 reply[n]=0;printf("INSTALLD_REPLY %s: %s\n",text,reply);
 if(!strcmp(text,"ping"))return strcmp(reply,"0")? -1:0;
 return strtol(reply,0,10)==0?0:-1;
}
int main(int argc,char **argv){
 int ping_only=argc==3&&!strcmp(argv[2],"--ping-only");
 if(argc!=2&&!ping_only)return 2;
 struct sockaddr_un address={.sun_family=AF_UNIX};char path[512];
 if(snprintf(address.sun_path,sizeof(address.sun_path),"%s/dev/socket/installd",argv[1])>=(int)sizeof(address.sun_path))return 3;
 int fd=socket(AF_UNIX,SOCK_STREAM,0);if(fd<0)return 4;
 struct timeval timeout={3,0};setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));
 if(connect(fd,(struct sockaddr*)&address,sizeof(address))<0){perror("connect");return 5;}
 if(command(fd,"ping"))return 6;
 puts("INSTALLD_PROTOCOL_PING_OK");
 if(ping_only){close(fd);return 0;}
 if(command(fd,"create_app_data ! com.kobo.lab.smoke 0 3 10000 default 24"))return 7;
 struct stat info;snprintf(path,sizeof(path),"%s/data/data/com.kobo.lab.smoke",argv[1]);
 int valid=stat(path,&info)==0&&S_ISDIR(info.st_mode)&&info.st_uid==10000;
 int cleaned=command(fd,"destroy_app_data ! com.kobo.lab.smoke 0 3 0")==0;
 close(fd);
 if(!valid||!cleaned)return 8;
 if(stat(path,&info)==0||errno!=ENOENT)return 9;
 puts("INSTALLD_APP_DATA_CREATE_UID_AND_DESTROY_OK");return 0;
}
