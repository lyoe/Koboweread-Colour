#include "../runtime-prototype/close-inherited-fds.h"
#include <sys/personality.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
int main(int argc,char **argv){
 if(close_inherited_fds()<0){perror("close inherited descriptors");return 71;}
if(argc<3)return 2;if(chroot(argv[1])<0||chdir("/")<0){perror("chroot");return 3;}int old=personality(0xffffffffUL);if(old<0){perror("get personality");return 4;}if(personality((unsigned)old|ADDR_COMPAT_LAYOUT)<0){perror("legacy mmap personality");return 5;}struct stat marker;int jit=0;if(lstat("/lab/framework-jit-enabled",&marker)==0){if(!S_ISREG(marker.st_mode)||marker.st_uid!=0||(marker.st_mode&07777)!=0400)return 8;jit=1;}
if(setenv("KOBO_SYSTEM_SERVER_UID","1000",1)<0 || setenv("LD_PRELOAD",jit?"/lab/libkobo_system_uid.so:/lab/libkobo_jit_config.so:/lab/libkobo_vm.so":"/lab/libkobo_system_uid.so:/lab/libkobo_vm.so",1)<0)return 7;if(jit)fprintf(stderr,"ANDROID_SYSTEM_SERVER_JIT_REQUESTED max_cache_mib=8\n");execv(argv[2],argv+2);perror("exec");return 6;}
