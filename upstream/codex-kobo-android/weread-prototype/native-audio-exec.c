#define _GNU_SOURCE
#include "../runtime-prototype/close-inherited-fds.h"
#include <sys/personality.h>
#include <sys/prctl.h>
#include <grp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* A fixed private-image executable; never accepts arbitrary commands or IDs. */
int main(int argc,char **argv) {
 if(argc!=2 || strcmp(argv[1],"/tmp/kobo-android-runtime") || geteuid()!=0)return 2;
 if(close_inherited_fds()<0)return 3;
 if(chroot(argv[1])<0 || chdir("/")<0)return 4;
 int old=personality(0xffffffffUL);
 if(old<0 || personality((unsigned)old|ADDR_COMPAT_LAYOUT)<0)return 5;
 gid_t groups[]={1005};
 if(setgroups(1,groups)<0 || setgid(1041)<0 || setuid(1041)<0)return 6;
 if(prctl(PR_SET_NO_NEW_PRIVS,1,0,0,0)<0)return 7;
 if(clearenv()<0 || setenv("ANDROID_ROOT","/system",1)<0 ||
    setenv("ANDROID_DATA","/data",1)<0 ||
    setenv("PATH","/system/bin",1)<0 ||
    setenv("LD_PRELOAD","/lab/libkobo_vm.so",1)<0)return 8;
 execl("/system/bin/audioserver","audioserver",(char*)0);
 perror("exec private audioserver");return 9;
}
