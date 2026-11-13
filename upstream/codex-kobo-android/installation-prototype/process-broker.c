#define _GNU_SOURCE
#include "../runtime-prototype/close-inherited-fds.h"
#include "app-priority-limit.h"
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
#include <dirent.h>
static int host_network;
/* Only the fixed APK's manifest-declared SegmentService subprocess is added.
 * Resolve its credentials against the same PMS registration as the main app;
 * do not accept arbitrary colon-suffixed process names. */
static const char *identity_package(const char *name){
 return !strcmp(name,"com.tencent.weread.eink:seg")?"com.tencent.weread.eink":name;
}
struct registered_process {char name[96],seinfo[96],data[256];int uid,sdk;};
static int registered(const char*name,struct registered_process*out){
 name=identity_package(name);
 if(strcmp(name,"com.kobo.lab.smoke") && strcmp(name,"com.android.defcontainer") && strcmp(name,"com.kobo.lab.installed") && strcmp(name,"com.tencent.weread.eink"))return -1;
 struct stat st;if(stat("/data/system/kobo-install-process-allowlist.txt",&st)<0 || (st.st_uid!=0 && st.st_uid!=1000) || (st.st_mode&077)!=0)return -1;
 FILE*f=fopen("/data/system/kobo-install-process-allowlist.txt","r");if(!f)return -1;
 struct registered_process row;int found=0;
 while(fscanf(f,"%95s %d %95s %255s %d",row.name,&row.uid,row.seinfo,row.data,&row.sdk)==5){if(!strcmp(row.name,name)){*out=row;found=1;break;}}
 fclose(f);return found && out->uid>=10000 && out->uid<20000?0:-1;
}
/* No external volume is supplied. Vold may leave root:root 0700 scaffolding;
 * ordinary application UIDs cannot traverse it. Reject every other entry or mount. */
static int unavailable_external_storage(void){
 DIR*d=opendir("/storage");if(!d)return 0;struct dirent*e;int unavailable=1;
 while((e=readdir(d))){
  if(!strcmp(e->d_name,".") || !strcmp(e->d_name,".."))continue;
  if(strcmp(e->d_name,"emulated")){unavailable=0;break;}
  struct stat st;
  if(lstat("/storage/emulated",&st)<0 || !S_ISDIR(st.st_mode) || st.st_uid!=0 || st.st_gid!=0 || (st.st_mode&07777)!=0700){unavailable=0;break;}
 }
 closedir(d);if(!unavailable)return 0;
 FILE*f=fopen("/proc/mounts","r");if(!f)return 0;char line[2048],where[1024];
 while(fgets(line,sizeof(line),f))if(sscanf(line,"%*s %1023s",where)==1 && (!strncmp(where,"/tmp/kobo-android-runtime/storage",33)||!strncmp(where,"/storage",8))){unavailable=0;break;}
 fclose(f);return unavailable;
}
#define CLIENTS 8
#define ARGS 64
static int write_all(int fd,const void *p,size_t n){const char*b=p;while(n){ssize_t k=write(fd,b,n);if(k<0&&errno==EINTR)continue;if(k<=0)return -1;b+=k;n-=k;}return 0;}
static int line(FILE*f,char*s,size_t n){if(!fgets(s,n,f))return -1;size_t l=strlen(s);if(!l||s[l-1]!='\n')return -1;s[--l]=0;if(l&&s[l-1]=='\r')s[--l]=0;return 0;}
static int number(const char*s,long min,long max,long*out){char*end;errno=0;long n=strtol(s,&end,10);if(errno||!s[0]||*end||n<min||n>max)return -1;*out=n;return 0;}
static pid_t spawn(int count,char args[][1024]){
 int uid=-1,gid=-1,sdk=-1,runtime=0,entry=0,nice=0,group_count=0,external=0;gid_t groups[16];char name[96]={0},seinfo[96]={0},data_dir[256]={0};
 for(int i=0;i<count;i++){
  const char*a=args[i];long v;
  if(!strcmp(a,"--runtime-args")){if(runtime++)return -1;}
  else if(!strncmp(a,"--setuid=",9)){if(uid!=-1||number(a+9,10000,19999,&v))return -1;uid=(int)v;}
  else if(!strncmp(a,"--setgid=",9)){if(gid!=-1||number(a+9,10000,19999,&v))return -1;gid=(int)v;}
  else if(!strncmp(a,"--target-sdk-version=",21)){if(sdk!=-1||number(a+21,24,31,&v))return -1;sdk=(int)v;}
  else if(!strncmp(a,"--setgroups=",12)){
   if(group_count)return -1;char copy[1024];strcpy(copy,a+12);char*save=NULL;
   for(char*p=strtok_r(copy,",",&save);p;p=strtok_r(NULL,",",&save)){
    if(group_count==16||number(p,1,65535,&v))return -1;
    groups[group_count++]=(gid_t)v;
   }
  }
  else if(!strncmp(a,"--nice-name=",12)){if(nice++||strlen(a+12)>=sizeof(name))return -1;strcpy(name,a+12);}
  else if(!strncmp(a,"--seinfo=",9)){if(seinfo[0]||strlen(a+9)>=sizeof(seinfo))return -1;strcpy(seinfo,a+9);}
  else if(!strcmp(a,"--instruction-set=arm")){}
  else if(!strncmp(a,"--app-data-dir=",15)){if(data_dir[0]||strlen(a+15)>=sizeof(data_dir))return -1;strcpy(data_dir,a+15);}
  else if(!strcmp(a,"--mount-external-default")||!strcmp(a,"--mount-external-read")||!strcmp(a,"--mount-external-write")){if(external++)return -1;}
  else if(!strcmp(a,"android.app.ActivityThread")){if(entry++||i!=count-1)return -1;}
  else {fprintf(stderr,"UNSUPPORTED_PROCESS_ARGUMENT %s\n",a);return -1;}
 }
 struct registered_process identity;
 if(gid!=uid||runtime!=1||entry!=1||nice!=1||registered(name,&identity)||identity.uid!=uid||identity.sdk!=sdk||strcmp(identity.seinfo,seinfo)||strcmp(identity.data,data_dir))return -1;
 for(int i=0;i<group_count;i++){int g=groups[i];if(g!=9997&&g!=50000+(uid-10000)&&g!=1015&&g!=1023&&g!=1028&&g!=2001&&!((g==3002||g==3003)&&!strcmp(identity_package(name),"com.tencent.weread.eink")))return -1;}
 if(external){
  if(!unavailable_external_storage()){fprintf(stderr,"EXTERNAL_STORAGE_VIEW_UNSUPPORTED\n");return -1;}
  fprintf(stderr,"PROCESS_EXTERNAL_STORAGE_UNAVAILABLE package=%s requested_flag=%d empty_or_root_locked no_mount_namespace\n",name,external);
 }
 int pipefd[2];if(pipe2(pipefd,O_CLOEXEC)<0)return -1;
 pid_t pid=fork();
 if(pid==0){
  close(pipefd[0]);int error;
  signal(SIGCHLD,SIG_DFL);
  if(setpgid(0,0)<0)goto fail;
  int input=open("/dev/null",O_RDONLY),log=open("/lab/apk-process.log",O_WRONLY|O_CREAT|O_APPEND,0600);
  if(input<0||log<0||dup2(input,0)<0||dup2(log,1)<0||dup2(log,2)<0)goto fail;
  if(input>2)close(input);if(log>2)close(log);
  // Do not pass host SSH, shell, or user environment variables to application processes.
  if(clearenv()<0)goto fail;
  if(setenv("ANDROID_ROOT","/system",1)<0)goto fail;
  if(setenv("ANDROID_DATA","/data",1)<0)goto fail;
  if(setenv("ANDROID_STORAGE","/storage",1)<0)goto fail;
  if(setenv("ANDROID_ASSETS","/system/app",1)<0)goto fail;
  if(setenv("EXTERNAL_STORAGE","/storage/emulated/legacy",1)<0)goto fail;
  if(setenv("PATH","/system/bin:/system/xbin",1)<0)goto fail;
  if(setenv("BOOTCLASSPATH","/system/framework/core-oj.jar:/system/framework/core-libart.jar:/system/framework/conscrypt.jar:/system/framework/okhttp.jar:/system/framework/core-junit.jar:/system/framework/bouncycastle.jar:/system/framework/ext.jar:/system/framework/framework.jar:/system/framework/telephony-common.jar:/system/framework/voip-common.jar:/system/framework/ims-common.jar:/system/framework/apache-xml.jar:/system/framework/org.apache.http.legacy.boot.jar",1)<0)goto fail;
  int weread=!strcmp(identity_package(name),"com.tencent.weread.eink");
  int weread_jit=0;struct stat jit_marker;
  if(weread&&lstat("/lab/weread-jit-enabled",&jit_marker)==0){
   if(!S_ISREG(jit_marker.st_mode)||jit_marker.st_uid!=0||(jit_marker.st_mode&077)!=0){errno=EPERM;goto fail;}
   weread_jit=1;
  }
  const char *preload=host_network&&weread?"/lab/libkobo_vm.so:/lab/libkobo_ipv4.so":"/lab/libkobo_vm.so";
  if(weread_jit)preload=host_network?"/lab/libkobo_jit_config.so:/lab/libkobo_vm.so:/lab/libkobo_ipv4.so":"/lab/libkobo_jit_config.so:/lab/libkobo_vm.so";
  if(setenv("LD_PRELOAD",preload,1)<0)goto fail;
  if(weread&&host_network&&access("/lab/webview-read-diagnostics-enabled",F_OK)==0){if(setenv("KOBO_WEREAD_WEB_DIAGNOSTICS","1",1)<0)goto fail;}
  if(weread&&host_network&&access("/lab/webview-software-draw-enabled",F_OK)==0){if(setenv("KOBO_WEREAD_DRAW_DIAGNOSTIC","1",1)<0)goto fail;}
  if(weread){if(setenv("CLASSPATH","/lab/weread-process-entry.dex",1)<0)goto fail;}
  else unsetenv("CLASSPATH");
  if(android_app_priority_limit()<0)goto fail;
  if(setgroups(group_count,groups)<0||setgid(gid)<0||setuid(uid)<0)goto fail;
  if(getuid()!=(unsigned)uid||geteuid()!=(unsigned)uid||getgid()!=(unsigned)gid||getegid()!=(unsigned)gid){errno=EPERM;goto fail;}
  if(prctl(PR_SET_NO_NEW_PRIVS,1,0,0,0)<0)goto fail;
  umask(0077);
  char nice_arg[128];snprintf(nice_arg,sizeof(nice_arg),"--nice-name=%s",name);
  char*argv[]={"/system/bin/app_process32","-Xint","-Xnorelocate","-Xnoimage-dex2oat","-Xnodex2oat","/system/bin","--application",nice_arg,weread?"WeReadProcessEntry":"android.app.ActivityThread",NULL};
  char*jit_argv[]={"/system/bin/app_process32","-Xusejit:true","-Xjitinitialsize:256K","-Xjitmaxsize:8M","-Xnorelocate","-Xnoimage-dex2oat","-Xnodex2oat","/system/bin","--application",nice_arg,"WeReadProcessEntry",NULL};
  if(weread_jit){printf("WEREAD_PROCESS_JIT_CONFIGURATION_ENABLED max_cache_mib=8\n");fflush(stdout);}
  char **command=weread_jit?jit_argv:argv;
  execv(command[0],command);
 fail:error=errno;write_all(pipefd[1],&error,sizeof(error));_exit(127);
 }
 close(pipefd[1]);if(pid<0){close(pipefd[0]);return -1;}
 int error=0;ssize_t n;do{n=read(pipefd[0],&error,sizeof(error));}while(n<0&&errno==EINTR);close(pipefd[0]);
 if(n!=0){kill(pid,SIGKILL);fprintf(stderr,"PROCESS_EXEC_FAILED errno=%d\n",error);return -1;}
 FILE*record=fopen("/lab/install-processes.pid","a");if(!record){kill(pid,SIGKILL);return -1;}fprintf(record,"%d\n",pid);fclose(record);
 printf("PROCESS_BROKER_EXEC_OK package=%s pid=%d uid=%d gids=%d\n",name,pid,uid,group_count);fflush(stdout);return pid;
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
 if(pid<0){puts("PROCESS_BROKER_REQUEST_REJECTED");for(int i=0;i<n;i++)printf("REJECTED_ARG_%d=%s\n",i,args[i]);fflush(stdout);}
 // Android 7's client throws after a negative PID and does not consume a wrapper byte.
 if(write_all(fd,&result,4))return -1;
 return pid<0?0:write_all(fd,&wrapper,1);
}
int main(int argc,char**argv){
 if(close_inherited_fds()<0){perror("close inherited descriptors");return 71;}

 if(argc!=2||geteuid()!=0)return 2;
 if(chroot(argv[1])<0||chdir("/")<0)return 3;
 const char *network=getenv("KOBO_WEREAD_HOST_NETWORK");host_network=network&&!strcmp(network,"1");
 if(host_network){struct stat endpoint;if(lstat("/dev/socket/dnsproxyd",&endpoint)||!S_ISSOCK(endpoint.st_mode)||endpoint.st_uid!=0)return 12;printf("PROCESS_BROKER_HOST_DEFAULT_NETWORK_IPV4_ENABLED\n");}
 int old=personality(0xffffffffUL);if(old<0||personality((unsigned)old|ADDR_COMPAT_LAYOUT)<0)return 4;
 signal(SIGCHLD,SIG_IGN);signal(SIGPIPE,SIG_IGN);
 int listener=socket(AF_UNIX,SOCK_STREAM|SOCK_CLOEXEC,0);if(listener<0)return 5;
 struct sockaddr_un address={.sun_family=AF_UNIX};strcpy(address.sun_path,"/dev/socket/zygote");
 if(access(address.sun_path,F_OK)==0){fprintf(stderr,"Existing zygote socket; refusing\n");return 6;}
 if(bind(listener,(struct sockaddr*)&address,sizeof(address))<0||chown(address.sun_path,0,1000)<0||chmod(address.sun_path,0660)<0||listen(listener,8)<0){perror("bind/listen");return 7;}
 FILE*clients[CLIENTS]={0};puts("KOBO_INSTALL_PACKAGE_PROCESS_BROKER_READY");fflush(stdout);
 for(;;){
  fd_set ready;FD_ZERO(&ready);FD_SET(listener,&ready);int max=listener;
  for(int i=0;i<CLIENTS;i++)if(clients[i]){int fd=fileno(clients[i]);FD_SET(fd,&ready);if(fd>max)max=fd;}
  int selected=select(max+1,&ready,NULL,NULL,NULL);if(selected<0){if(errno==EINTR)continue;return 8;}
  if(FD_ISSET(listener,&ready)){
   int fd=accept4(listener,NULL,NULL,SOCK_CLOEXEC);if(fd>=0){
    struct ucred peer;socklen_t length=sizeof(peer);int slot=-1;
    for(int i=0;i<CLIENTS;i++)if(!clients[i]){slot=i;break;}
    if(slot<0||getsockopt(fd,SOL_SOCKET,SO_PEERCRED,&peer,&length)<0||(peer.uid!=0 && peer.uid!=1000)){close(fd);}
    else{struct timeval timeout={5,0};setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));clients[slot]=fdopen(fd,"r");if(!clients[slot])close(fd);}
   }
  }
  for(int i=0;i<CLIENTS;i++)if(clients[i]&&FD_ISSET(fileno(clients[i]),&ready)){
   if(request(clients[i])<0){fclose(clients[i]);clients[i]=NULL;}
  }
 }
}
