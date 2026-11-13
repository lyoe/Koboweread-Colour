#define _GNU_SOURCE
#include "../runtime-prototype/close-inherited-fds.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
/* Fixed offline private-image growth. Caller verifies backup, no active image
 * mounts/loops, >=384MiB host space, and runs a read-only fs check first. */
int main(int argc,char **argv){
 if(argc!=3||geteuid()!=0||(strcmp(argv[1],"check")&&strcmp(argv[1],"grow"))||strcmp(argv[2],"/tmp/kobo-resize-tools"))return 2;
 if(close_inherited_fds()<0)return 3;
 int fd=open("/mnt/onboard/.adds/kobo-android-lab/graphics.img",O_RDWR|O_NOFOLLOW);struct stat st;unsigned char magic[2];
 if(fd<0||fstat(fd,&st)||!S_ISREG(st.st_mode)||(st.st_size!=768LL*1024*1024&&st.st_size!=1024LL*1024*1024)||pread(fd,magic,2,1080)!=2||magic[0]!=0x53||magic[1]!=0xef)return 4;
 if(!strcmp(argv[1],"grow")){
  if(st.st_size!=768LL*1024*1024)return 7;
  if(ftruncate(fd,1024LL*1024*1024)){perror("extend private image");return 8;}
 }
 if(chroot(argv[2])||chdir("/"))return 5;
 char file[64];snprintf(file,sizeof(file),"/proc/self/fd/%d",fd);
 if(!strcmp(argv[1],"check"))execl("/system/bin/e2fsck","e2fsck","-f","-n",file,(char*)0);
 else execl("/system/bin/resize2fs","resize2fs",file,"1024M",(char*)0);
 perror("exec filesystem tool");return 6;
}
