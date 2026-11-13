#define _GNU_SOURCE
#include "../runtime-prototype/close-inherited-fds.h"
#include <sys/personality.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <grp.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <string.h>
/* Experimental fixed UID launcher; no privileged executable is installed setuid. */
int main(int argc,char **argv) {
 if(close_inherited_fds()<0){perror("close inherited descriptors");return 71;}

 if(argc<3 || geteuid()!=0)return 2;
 if(chroot(argv[1])<0 || chdir("/")<0){perror("chroot");return 3;}
 int await_attach=argc>3 && strcmp(argv[2],"--await-attach")==0;
 int executable_index=await_attach?3:2;
 if(await_attach){
  int log=open("/lab/apk-process.log",O_WRONLY|O_CREAT|O_TRUNC,0600);
  if(log<0 || dup2(log,1)<0 || dup2(log,2)<0){perror("app log");return 10;}
  if(log>2)close(log);
  unsetenv("CLASSPATH");
 }
 int old=personality(0xffffffffUL);
 if(old<0 || personality((unsigned)old|ADDR_COMPAT_LAYOUT)<0){perror("personality");return 4;}
 if(setenv("LD_PRELOAD","/lab/libkobo_vm.so",1)<0)return 5;
 if(setgroups(0,NULL)<0 || setgid(10000)<0 || setuid(10000)<0){perror("drop privileges");return 6;}
 if(getuid()!=10000 || geteuid()!=10000 || getgid()!=10000 || getegid()!=10000)return 7;
 if(prctl(PR_SET_NO_NEW_PRIVS,1,0,0,0)<0){perror("no_new_privs");return 8;}
 umask(0077);
 if(await_attach && raise(SIGSTOP)!=0)return 11;
 execv(argv[executable_index],argv+executable_index);perror("exec app runtime");return 9;
}
