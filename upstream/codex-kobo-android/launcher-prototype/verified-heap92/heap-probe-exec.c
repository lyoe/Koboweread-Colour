#define _GNU_SOURCE
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <grp.h>
int main(int argc,char **argv){
 if(getuid()!=0||argc!=3)return 2;
 if(strcmp(argv[1],"0")&&strcmp(argv[1],"10007"))return 2;
 if(strcmp(argv[2],"baseline")&&strcmp(argv[2],"enabled"))return 2;
 uid_t uid=(uid_t)atoi(argv[1]);
 if(chroot("/tmp/kobo-android-runtime")||chdir("/")||setgroups(0,0)||setgid(uid)||setuid(uid))return 3;
 clearenv();setenv("LD_PRELOAD","/lab/libkobo_jit_config.so:/lab/heap-property-probe.so",1);setenv("EXPECT_HEAP",argv[2],1);
 execl("/system/bin/sh","sh","-c","true",(char*)0);return 4;
}
