#define _GNU_SOURCE
#include "../runtime-prototype/close-inherited-fds.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static int inherited_socket(const char*name){
 struct sockaddr_un address={.sun_family=AF_UNIX};snprintf(address.sun_path,sizeof(address.sun_path),"/dev/socket/%s",name);
 if(access(address.sun_path,F_OK)==0){fprintf(stderr,"Existing %s socket; refusing\n",name);return -1;}
 int fd=socket(AF_UNIX,SOCK_STREAM,0);if(fd<0)return -1;
 if(bind(fd,(struct sockaddr*)&address,sizeof(address))<0||chmod(address.sun_path,0600)<0||listen(fd,8)<0){perror("bind/listen");close(fd);return -1;}
 char key[64],value[24];snprintf(key,sizeof(key),"ANDROID_SOCKET_%s",name);snprintf(value,sizeof(value),"%d",fd);
 if(setenv(key,value,1)<0){close(fd);return -1;}return fd;
}
int main(int argc,char**argv){
 if(close_inherited_fds()<0){perror("close inherited descriptors");return 71;}

 if(argc!=2||geteuid()!=0||strcmp(argv[1],"/tmp/kobo-android-runtime"))return 2;
 if(chroot(argv[1])<0||chdir("/")<0){perror("chroot");return 3;}
 if(access("/lab/vold-owned-mounts",R_OK)<0||access("/lab/vold-owned-proc",R_OK|X_OK)<0)return 4;
 if(inherited_socket("vold")<0||inherited_socket("cryptd")<0)return 5;
 setenv("KOBO_VOLD_PROC_SCOPE","1",1);
 setenv("LD_PRELOAD","/lab/libkobo_vold_scope.so",1);
 setenv("ANDROID_ROOT","/system",1);setenv("ANDROID_DATA","/data",1);
 execl("/system/bin/vold","vold","--blkid_context","u:r:blkid:s0","--blkid_untrusted_context","u:r:blkid_untrusted:s0","--fsck_context","u:r:fsck:s0","--fsck_untrusted_context","u:r:fsck_untrusted:s0",(char*)0);
 perror("exec vold");return 6;
}
