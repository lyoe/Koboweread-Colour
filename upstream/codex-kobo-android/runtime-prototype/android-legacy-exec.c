#include "close-inherited-fds.h"
#include <sys/personality.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
int main(int argc,char **argv){
 if(close_inherited_fds()<0){perror("close inherited descriptors");return 71;}
if(argc<3)return 2;if(chroot(argv[1])<0||chdir("/")<0){perror("chroot");return 3;}int old=personality(0xffffffffUL);if(old<0){perror("get personality");return 4;}if(personality((unsigned)old|ADDR_COMPAT_LAYOUT)<0){perror("legacy mmap personality");return 5;}if(getenv("KOBO_VM_SHIM") && setenv("LD_PRELOAD","/lab/libkobo_vm.so",1)<0)return 7;execv(argv[2],argv+2);perror("exec");return 6;}
