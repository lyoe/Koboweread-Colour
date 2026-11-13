#define _GNU_SOURCE
#include "../runtime-prototype/close-inherited-fds.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <sys/personality.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
// Create the init-style inherited socket only inside the experiment chroot.
int main(int argc,char **argv) {
 if(close_inherited_fds()<0){perror("close inherited descriptors");return 71;}

 if(argc!=2)return 2;
 if(chroot(argv[1])<0||chdir("/")<0){perror("chroot");return 3;}
 int old=personality(0xffffffffUL);if(old<0||personality((unsigned)old|ADDR_COMPAT_LAYOUT)<0)return 8;
 if(setenv("LD_PRELOAD","/lab/libkobo_vm.so",1)<0)return 9;
 struct sockaddr_un address={.sun_family=AF_UNIX};
 strcpy(address.sun_path,"/dev/socket/installd");
 int fd=socket(AF_UNIX,SOCK_STREAM,0);if(fd<0){perror("socket");return 4;}
 unlink(address.sun_path);
 if(bind(fd,(struct sockaddr*)&address,sizeof(address))<0||chown(address.sun_path,0,1000)<0||chmod(address.sun_path,0660)<0||listen(fd,5)<0){perror("bind/listen");return 5;}
 char value[24];snprintf(value,sizeof(value),"%d",fd);
 setenv("ANDROID_SOCKET_installd",value,1);setenv("ANDROID_ROOT","/system",1);setenv("ANDROID_DATA","/data",1);setenv("ASEC_MOUNTPOINT","/mnt/asec",1);
 execl("/system/bin/installd","installd",(char*)0);perror("exec installd");return 6;
}
