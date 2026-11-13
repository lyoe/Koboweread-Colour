#define _GNU_SOURCE
#include "../runtime-prototype/close-inherited-fds.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
int main(int argc,char**argv){
 if(argc!=3 || geteuid()!=0 || (strcmp(argv[1],"check") && strcmp(argv[1],"grow")) || strcmp(argv[2],"/tmp/kobo-resize-tools"))return 2;
 if(close_inherited_fds()<0)return 3;
 int fd=open("/mnt/onboard/.adds/kobo-android-lab/graphics.img",O_RDWR);struct stat st;
 if(fd<0 || fstat(fd,&st)<0 || !S_ISREG(st.st_mode) || st.st_size<384LL*1024*1024 || st.st_size>768LL*1024*1024)return 4;
 if(!strcmp(argv[1],"grow") && ftruncate(fd,768LL*1024*1024)<0){perror("extend private image file");return 7;}
 if(chroot(argv[2])<0 || chdir("/")<0)return 5;
 char file[64];snprintf(file,sizeof(file),"/proc/self/fd/%d",fd);
 if(!strcmp(argv[1],"check"))execl("/system/bin/e2fsck","e2fsck","-f","-n",file,(char*)0);
 else execl("/system/bin/resize2fs","resize2fs",file,"768M",(char*)0);
 perror("exec filesystem tool");return 6;
}
