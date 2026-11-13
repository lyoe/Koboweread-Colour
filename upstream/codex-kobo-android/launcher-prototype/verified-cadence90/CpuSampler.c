// Bounded read-only CPU/memory telemetry for this private Android runtime.
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#define R "/tmp/kobo-android-runtime"
struct role {const char *name;int pid;unsigned long long start;};
static int ticks(int pid,unsigned long long *start,unsigned long long *cpu){
 char p[64],s[4096];snprintf(p,sizeof(p),"/proc/%d/stat",pid);FILE*f=fopen(p,"r");if(!f)return -1;int ok=fgets(s,sizeof(s),f)!=NULL;fclose(f);if(!ok)return -1;
 char*r=strrchr(s,')');if(!r||r[2]=='Z')return -1;char*save=0,*v=strtok_r(r+2," ",&save);int field=3;*start=*cpu=0;
 while(v){if(field==14||field==15)*cpu+=strtoull(v,0,10);if(field==22){*start=strtoull(v,0,10);break;}field++;v=strtok_r(0," ",&save);}return *start?0:-1;
}
static void discover(struct role roles[4]){
 DIR*d=opendir("/proc");if(!d)return;struct dirent*e;
 while((e=readdir(d))){char*end;int pid=(int)strtol(e->d_name,&end,10);if(*end||pid<=1)continue;
  char p[64],exe[256],cmd[128]={0};struct stat st;snprintf(p,sizeof(p),"/proc/%d",pid);if(stat(p,&st)<0)continue;
  snprintf(p,sizeof(p),"/proc/%d/exe",pid);ssize_t n=readlink(p,exe,sizeof(exe)-1);if(n<0)continue;exe[n]=0;int role=-1;
  if(!strcmp(exe,R "/system/bin/surfaceflinger"))role=0;
  else if(!strcmp(exe,R "/system/bin/app_process32")){
   if(st.st_uid==1000)role=3;
   else if(st.st_uid==10007){snprintf(p,sizeof(p),"/proc/%d/cmdline",pid);FILE*f=fopen(p,"rb");if(f){fread(cmd,1,sizeof(cmd)-1,f);fclose(f);if(!strcmp(cmd,"com.tencent.weread.eink"))role=1;else if(!strcmp(cmd,"com.tencent.weread.eink:seg"))role=2;}}
  }
  if(role>=0&&!roles[role].pid){unsigned long long start,cpu;if(ticks(pid,&start,&cpu)==0){roles[role].pid=pid;roles[role].start=start;}}
 }closedir(d);
}
int main(int argc,char**argv){
 if(argc!=2||geteuid()!=0)return 2;char*end;long seconds=strtol(argv[1],&end,10);if(*end||seconds<10||seconds>180)return 2;
 struct role roles[4]={{"SF",0,0},{"APP",0,0},{"SEG",0,0},{"SYS",0,0}};setvbuf(stdout,0,_IOLBF,0);
 printf("# cpu_ticks_per_second=%ld\n",sysconf(_SC_CLK_TCK));puts("uptime,role,process_ticks,total_ticks,idle_iowait_ticks,mem_available_kib");
 for(int round=0;round<=seconds;round++){
  if(access(R "/lab/interactive-session-ready",F_OK)<0)break;
  if(round%5==0)discover(roles);
  unsigned long long v[8]={0},total=0,idle=0;FILE*f=fopen("/proc/stat","r");if(!f)return 3;
  int count=fscanf(f,"cpu %llu %llu %llu %llu %llu %llu %llu %llu",v,v+1,v+2,v+3,v+4,v+5,v+6,v+7);fclose(f);if(count!=8)return 3;for(int i=0;i<8;i++)total+=v[i];idle=v[3]+v[4];
  long available=-1;char line[256];f=fopen("/proc/meminfo","r");if(f){while(fgets(line,sizeof(line),f))if(sscanf(line,"MemAvailable: %ld kB",&available)==1)break;fclose(f);}
  struct timespec now;clock_gettime(CLOCK_MONOTONIC,&now);double when=now.tv_sec+now.tv_nsec/1e9;
  for(int i=0;i<4;i++){unsigned long long start,cpu;if(roles[i].pid&&ticks(roles[i].pid,&start,&cpu)==0&&start==roles[i].start)printf("%.3f,%s,%llu,%llu,%llu,%ld\n",when,roles[i].name,cpu,total,idle,available);else roles[i].pid=0;}
  if(round!=seconds){struct timespec delay={1,0};nanosleep(&delay,0);}
 }
 return 0;
}
