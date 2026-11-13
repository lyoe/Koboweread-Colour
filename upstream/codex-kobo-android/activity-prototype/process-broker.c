#define _GNU_SOURCE
#include "../runtime-prototype/close-inherited-fds.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/personality.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <signal.h>
#include <fcntl.h>
#include <grp.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define PACKAGE "com.kobo.lab.smoke"
#define CLIENTS 8
#define ARGS 64
static int write_all(int fd,const void *p,size_t n){const char*b=p;while(n){ssize_t k=write(fd,b,n);if(k<0&&errno==EINTR)continue;if(k<=0)return -1;b+=k;n-=k;}return 0;}
static int line(FILE*f,char*s,size_t n){if(!fgets(s,n,f))return -1;size_t l=strlen(s);if(!l||s[l-1]!='\n')return -1;s[--l]=0;if(l&&s[l-1]=='\r')s[--l]=0;return 0;}
static int number(const char*s,long min,long max,long*out){char*end;errno=0;long n=strtol(s,&end,10);if(errno||!s[0]||*end||n<min||n>max)return -1;*out=n;return 0;}
static pid_t spawn(int count,char args[][1024]){
 int uid=-1,gid=-1,sdk=-1,runtime=0,entry=0,nice=0,group_count=0;gid_t groups[16];
 for(int i=0;i<count;i++){
  const char*a=args[i];long v;
  if(!strcmp(a,"--runtime-args")){if(runtime++)return -1;}
  else if(!strncmp(a,"--setuid=",9)){if(uid!=-1||number(a+9,10000,10000,&v))return -1;uid=(int)v;}
  else if(!strncmp(a,"--setgid=",9)){if(gid!=-1||number(a+9,10000,10000,&v))return -1;gid=(int)v;}
  else if(!strncmp(a,"--target-sdk-version=",21)){if(sdk!=-1||number(a+21,24,24,&v))return -1;sdk=(int)v;}
  else if(!strncmp(a,"--setgroups=",12)){
   if(group_count)return -1;char copy[1024];strcpy(copy,a+12);char*save=NULL;
   for(char*p=strtok_r(copy,",",&save);p;p=strtok_r(NULL,",",&save)){
    if(group_count==16||number(p,1,65535,&v)||!(v==9997||v==50000))return -1;
    groups[group_count++]=(gid_t)v;
   }
  }
  else if(!strcmp(a,"--nice-name=" PACKAGE)){if(nice++)return -1;}
  else if(!strcmp(a,"--seinfo=default")){}
  else if(!strcmp(a,"--instruction-set=arm")){}
  else if(!strcmp(a,"--app-data-dir=/data/user/0/" PACKAGE)||!strcmp(a,"--app-data-dir=/data/data/" PACKAGE)){}
  else if(!strcmp(a,"android.app.ActivityThread")){if(entry++||i!=count-1)return -1;}
  else {fprintf(stderr,"UNSUPPORTED_PROCESS_ARGUMENT %s\n",a);return -1;}
 }
 if(uid!=10000||gid!=uid||sdk!=24||runtime!=1||entry!=1||nice!=1)return -1;
 int pipefd[2];if(pipe2(pipefd,O_CLOEXEC)<0)return -1;
 pid_t pid=fork();
 if(pid==0){
  close(pipefd[0]);int error;
  signal(SIGCHLD,SIG_DFL);
  if(setpgid(0,0)<0)goto fail;
  int input=open("/dev/null",O_RDONLY),log=open("/lab/apk-process.log",O_WRONLY|O_CREAT|O_APPEND,0600);
  if(input<0||log<0||dup2(input,0)<0||dup2(log,1)<0||dup2(log,2)<0)goto fail;
  if(input>2)close(input);if(log>2)close(log);
  if(setenv("LD_PRELOAD","/lab/libkobo_vm.so",1)<0)goto fail;
  unsetenv("CLASSPATH");
  if(setgroups(group_count,groups)<0||setgid(gid)<0||setuid(uid)<0)goto fail;
  if(getuid()!=10000||geteuid()!=10000||getgid()!=10000||getegid()!=10000){errno=EPERM;goto fail;}
  if(prctl(PR_SET_NO_NEW_PRIVS,1,0,0,0)<0)goto fail;
  umask(0077);
  char*argv[]={"/system/bin/app_process32","-Xint","-Xnorelocate","-Xnoimage-dex2oat","-Xnodex2oat","/system/bin","--application","--nice-name=" PACKAGE,"android.app.ActivityThread",NULL};
  execv(argv[0],argv);
 fail:error=errno;write_all(pipefd[1],&error,sizeof(error));_exit(127);
 }
 close(pipefd[1]);if(pid<0){close(pipefd[0]);return -1;}
 int error=0;ssize_t n;do{n=read(pipefd[0],&error,sizeof(error));}while(n<0&&errno==EINTR);close(pipefd[0]);
 if(n!=0){kill(pid,SIGKILL);fprintf(stderr,"PROCESS_EXEC_FAILED errno=%d\n",error);return -1;}
 FILE*record=fopen("/lab/app-probe.pid","w");if(!record){kill(pid,SIGKILL);return -1;}fprintf(record,"%d\n",pid);fclose(record);
 printf("PROCESS_BROKER_EXEC_OK pid=%d uid=10000 gids=%d\n",pid,group_count);fflush(stdout);return pid;
}
static int request(FILE*f){
 char text[1024],args[ARGS][1024];long n;
 if(line(f,text,sizeof(text))||number(text,1,ARGS,&n))return -1;
 for(int i=0;i<n;i++)if(line(f,args[i],sizeof(args[i])))return -1;
 int fd=fileno(f);
 if(n==1&&!strcmp(args[0],"--query-abi-list")){
  const char*abis="armeabi-v7a,armeabi";uint32_t size=htonl(strlen(abis));
  if(write_all(fd,&size,4)||write_all(fd,abis,strlen(abis)))return -1;
  puts("PROCESS_BROKER_NATIVE_ABI_QUERY_OK");fflush(stdout);return 0;
 }
 pid_t pid=spawn((int)n,args);uint32_t result=htonl((uint32_t)pid);unsigned char wrapper=0;
 if(pid<0){puts("PROCESS_BROKER_REQUEST_REJECTED");fflush(stdout);}
 return write_all(fd,&result,4)||write_all(fd,&wrapper,1)?-1:0;
}
int main(int argc,char**argv){
 if(close_inherited_fds()<0){perror("close inherited descriptors");return 71;}

 if(argc!=2||geteuid()!=0)return 2;
 if(chroot(argv[1])<0||chdir("/")<0)return 3;
 int old=personality(0xffffffffUL);if(old<0||personality((unsigned)old|ADDR_COMPAT_LAYOUT)<0)return 4;
 signal(SIGCHLD,SIG_IGN);signal(SIGPIPE,SIG_IGN);
 int listener=socket(AF_UNIX,SOCK_STREAM|SOCK_CLOEXEC,0);if(listener<0)return 5;
 struct sockaddr_un address={.sun_family=AF_UNIX};strcpy(address.sun_path,"/dev/socket/zygote");
 if(access(address.sun_path,F_OK)==0){fprintf(stderr,"Existing zygote socket; refusing\n");return 6;}
 if(bind(listener,(struct sockaddr*)&address,sizeof(address))<0||chmod(address.sun_path,0600)<0||listen(listener,8)<0){perror("bind/listen");return 7;}
 FILE*clients[CLIENTS]={0};puts("KOBO_FIXED_PACKAGE_PROCESS_BROKER_READY");fflush(stdout);
 for(;;){
  fd_set ready;FD_ZERO(&ready);FD_SET(listener,&ready);int max=listener;
  for(int i=0;i<CLIENTS;i++)if(clients[i]){int fd=fileno(clients[i]);FD_SET(fd,&ready);if(fd>max)max=fd;}
  int selected=select(max+1,&ready,NULL,NULL,NULL);if(selected<0){if(errno==EINTR)continue;return 8;}
  if(FD_ISSET(listener,&ready)){
   int fd=accept4(listener,NULL,NULL,SOCK_CLOEXEC);if(fd>=0){
    struct ucred peer;socklen_t length=sizeof(peer);int slot=-1;
    for(int i=0;i<CLIENTS;i++)if(!clients[i]){slot=i;break;}
    if(slot<0||getsockopt(fd,SOL_SOCKET,SO_PEERCRED,&peer,&length)<0||peer.uid!=0){close(fd);}
    else{struct timeval timeout={5,0};setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));clients[slot]=fdopen(fd,"r");if(!clients[slot])close(fd);}
   }
  }
  for(int i=0;i<CLIENTS;i++)if(clients[i]&&FD_ISSET(fileno(clients[i]),&ready)){
   if(request(clients[i])<0){fclose(clients[i]);clients[i]=NULL;}
  }
 }
}
