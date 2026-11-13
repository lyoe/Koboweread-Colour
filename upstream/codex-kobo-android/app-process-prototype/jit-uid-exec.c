#define _GNU_SOURCE
#include "../runtime-prototype/close-inherited-fds.h"
#include <sys/personality.h>
#include <sys/prctl.h>
#include <grp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc,char **argv){
 if(argc!=3||geteuid()!=0||strcmp(argv[1],"/tmp/kobo-android-runtime")||(strcmp(argv[2],"jit")&&strcmp(argv[2],"interpreter")))return 2;
 int jit=!strcmp(argv[2],"jit");if(close_inherited_fds()<0||chroot(argv[1])||chdir("/"))return 3;
 int old=personality(0xffffffffUL);if(old<0||personality((unsigned)old|ADDR_COMPAT_LAYOUT)<0)return 4;
 if(setgroups(0,NULL)||setgid(10000)||setuid(10000)||prctl(PR_SET_NO_NEW_PRIVS,1,0,0,0)||clearenv())return 5;
 if(setenv("ANDROID_ROOT","/system",1)||setenv("ANDROID_DATA","/data",1)||setenv("PATH","/system/bin",1)||setenv("LD_PRELOAD",jit?"/lab/libkobo_jit_config.so:/lab/libkobo_vm.so":"/lab/libkobo_vm.so",1)||setenv("CLASSPATH","/lab/jit-probe.dex",1)||setenv("BOOTCLASSPATH","/system/framework/core-oj.jar:/system/framework/core-libart.jar:/system/framework/conscrypt.jar:/system/framework/okhttp.jar:/system/framework/core-junit.jar:/system/framework/bouncycastle.jar:/system/framework/ext.jar:/system/framework/framework.jar:/system/framework/telephony-common.jar:/system/framework/voip-common.jar:/system/framework/ims-common.jar:/system/framework/apache-xml.jar:/system/framework/org.apache.http.legacy.boot.jar",1))return 6;
 execl("/system/bin/app_process32","app_process32",jit?"-Xusejit:true":"-Xint","-Xjitinitialsize:256K","-Xjitmaxsize:8M","-Xnorelocate","-Xnoimage-dex2oat","-Xnodex2oat","/system/bin","JitProbe",jit?"jit":"interpreter",(char*)0);
 perror("exec fixed ART probe");return 7;
}
