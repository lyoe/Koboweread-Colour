// SPDX-License-Identifier: MIT
// Metadata-only file working-set snapshot and bounded shared page-cache keeper.
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <time.h>
#define ROOT "/tmp/kobo-android-runtime"
#define READY ROOT "/lab/interactive-session-ready"
#define FINISH ROOT "/lab/interactive-session-finish"
#define LIMIT (24u*1024u*1024u)
static volatile sig_atomic_t stopped;
static void stop(int n){(void)n;stopped=1;}
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec/1e9;}
static int read_file(const char *p,char *b,size_t n){int fd=open(p,O_RDONLY|O_CLOEXEC|O_NOFOLLOW);if(fd<0)return -1;ssize_t got=read(fd,b,n-1);close(fd);if(got<0)return -1;b[got]=0;return (int)got;}
static int link_is(int pid,const char *field,const char *s){char p[100],b[1200];snprintf(p,sizeof p,"/proc/%d/%s",pid,field);ssize_t n=readlink(p,b,sizeof b-1);if(n<0)return 0;b[n]=0;return !strcmp(s,b);}
static unsigned long long birth(int pid){char p[100],b[4096];snprintf(p,sizeof p,"/proc/%d/stat",pid);if(read_file(p,b,sizeof b)<0)return 0;char *s=strrchr(b,')');if(!s)return 0;s+=2;char *save=0;int field=3;for(char *t=strtok_r(s," ",&save);t;t=strtok_r(0," ",&save),field++)if(field==22)return strtoull(t,0,10);return 0;}
static int valid_app(int pid){char p[100],b[4096];if(!link_is(pid,"root",ROOT)||!link_is(pid,"exe",ROOT "/system/bin/app_process32"))return 0;snprintf(p,sizeof p,"/proc/%d/status",pid);if(read_file(p,b,sizeof b)<0)return 0;char *u=strstr(b,"\nUid:");unsigned uid=0;if(!u||sscanf(u,"\nUid: %u",&uid)!=1||uid!=10007)return 0;snprintf(p,sizeof p,"/proc/%d/cmdline",pid);return read_file(p,b,sizeof b)>0&&!strcmp(b,"com.tencent.weread.eink");}
static const char *kind(const char *p){
 if(strstr(p,"..")||strpbrk(p," \t\r\n"))return NULL;
 if(!strcmp(p,"/data/app/com.tencent.weread.eink-1/base.apk"))return "apk";
 if(!strcmp(p,"/data/app/com.tencent.weread.eink-1/oat/arm/base.odex"))return "odex";
 const char *dot=strrchr(p,'.');
 if(dot&&!strcmp(dot,".so")&&(!strncmp(p,"/system/lib/",12)||!strncmp(p,"/data/app/com.tencent.weread.eink-1/lib/arm/",44)))return "library";
 if(dot&&(!strcmp(dot,".oat")||!strcmp(dot,".art"))&&!strncmp(p,"/system/framework/arm/",22))return "boot";
 if(dot&&(!strcmp(dot,".ttf")||!strcmp(dot,".otf"))&&!strncmp(p,"/system/fonts/",14))return "font";
 return NULL;
}
static int code_open(const char *p,struct stat *st){
 char full[1200],resolved[4096];if(!kind(p)||snprintf(full,sizeof full,ROOT "%s",p)>=(int)sizeof full)return -1;
 if(!realpath(full,resolved)||strcmp(full,resolved))return -1;
 int fd=open(full,O_RDONLY|O_CLOEXEC|O_NOFOLLOW);if(fd<0)return -1;
 if(fstat(fd,st)||!S_ISREG(st->st_mode)||st->st_size<=0){close(fd);return -1;}return fd;
}
static int same_ready(const struct stat *first){struct stat s;return !lstat(READY,&s)&&s.st_ino==first->st_ino&&s.st_dev==first->st_dev&&s.st_uid==0&&S_ISREG(s.st_mode)&&access(FINISH,F_OK)!=0;}
static unsigned long available(void){char b[4096];if(read_file("/proc/meminfo",b,sizeof b)<0)return 0;char *p=strstr(b,"MemAvailable:");unsigned long n=0;if(p)sscanf(p,"MemAvailable: %lu",&n);return n;}
static int snapshot(int pid,const struct stat *ready,long page){
 if(!valid_app(pid))return 3;unsigned long long born=birth(pid);if(!born)return 3;
 char path[100];snprintf(path,sizeof path,"/proc/%d/maps",pid);FILE *maps=fopen(path,"r");if(!maps)return 4;
 snprintf(path,sizeof path,"/proc/%d/pagemap",pid);int pm=open(path,O_RDONLY|O_CLOEXEC);if(pm<0){fclose(maps);return 4;}
 printf("HOT_PAGES_BEGIN pid=%d born=%llu page_bytes=%ld ready_dev=%llu ready_ino=%llu file_backed_only=1\n",pid,born,page,(unsigned long long)ready->st_dev,(unsigned long long)ready->st_ino);
 char line[4096];unsigned file=0;size_t scanned=0,present=0;int rc=0;
 while(fgets(line,sizeof line,maps)){
  unsigned long lo,hi;unsigned long long offset,inode;char perm[5],dev[32];int at=0;
  if(sscanf(line,"%lx-%lx %4s %llx %31s %llu %n",&lo,&hi,perm,&offset,dev,&inode,&at)!=6||!at||perm[0]!='r'||perm[1]=='w')continue;
  char *p=line+at;p[strcspn(p,"\r\n")]=0;
  if(strncmp(p,ROOT "/",strlen(ROOT)+1))continue;p+=strlen(ROOT);
  if(!kind(p)||hi<=lo)continue;
  struct stat st;int fd=code_open(p,&st);if(fd<0)continue;close(fd);
  if((unsigned long long)st.st_ino!=inode)continue;
  printf("FILE %u %llu %llu %llu %s %s\n",file,(unsigned long long)st.st_dev,(unsigned long long)st.st_ino,(unsigned long long)st.st_size,kind(p),p);
  for(unsigned long address=lo;address<hi;){
   uint64_t entries[256];size_t count=(hi-address)/(unsigned long)page;if(count>256)count=256;
   if(!count||scanned+count>262144){rc=5;break;}
   ssize_t bytes=pread(pm,entries,count*8,(off_t)(address/(unsigned long)page)*8);
   if(bytes!=(ssize_t)(count*8)){rc=6;break;}
   for(size_t i=0;i<count;i++)if((entries[i]&(UINT64_C(1)<<63))&&(entries[i]&(UINT64_C(1)<<61))){
    unsigned long long off=offset+(address-lo)+i*(unsigned long)page;
    if(off<(unsigned long long)st.st_size){printf("PAGE %u %llu\n",file,off);present++;}
   }
   address+=count*(unsigned long)page;scanned+=count;
  }
  file++;if(rc)break;
 }
 close(pm);fclose(maps);
 if(!valid_app(pid)||birth(pid)!=born||!same_ready(ready))rc=7;
 if(!file&&!rc)rc=12;
 printf("HOT_PAGES_END files=%u scanned=%zu present=%zu result=%d\n",file,scanned,present,rc);return rc;
}
struct mapping {void *address;size_t bytes;};
static struct mapping mappings[8192];
static int keep(const char *plan,const struct stat *ready,long page){
 int fd=open(plan,O_RDONLY|O_CLOEXEC|O_NOFOLLOW);if(fd<0)return 3;struct stat st;
 if(fstat(fd,&st)||!S_ISREG(st.st_mode)||st.st_uid||st.st_size>2*1024*1024||(st.st_mode&0777)!=0600){close(fd);return 3;}
 FILE *f=fdopen(fd,"r");if(!f){close(fd);return 3;}
 char header[256],extra;unsigned long long session_dev,session_ino;
 if(!fgets(header,sizeof header,f)||sscanf(header,"SESSION %llu %llu %c",&session_dev,&session_ino,&extra)!=2||session_dev!=(unsigned long long)ready->st_dev||session_ino!=(unsigned long long)ready->st_ino){fclose(f);return 3;}
 struct rlimit limit={LIMIT,LIMIT};if(setrlimit(RLIMIT_MEMLOCK,&limit)){fclose(f);return 4;}
 size_t total=0;unsigned count=0;int rc=0;char line[1600];double begin=now();
 while(fgets(line,sizeof line,f)){
  unsigned long long dev,ino,size,offset,pages;char p[1100],extra;
  if(sscanf(line,"RANGE %llu %llu %llu %llu %llu %1099s %c",&dev,&ino,&size,&offset,&pages,p,&extra)!=6||!pages||pages>LIMIT/(unsigned long)page||offset%(unsigned long)page||offset>=size||pages*(unsigned long)page>size-offset+(unsigned long)page-1||total+pages*(unsigned long)page>LIMIT||count>=8192){rc=5;break;}
  if(stopped||!same_ready(ready)||available()<131072){rc=6;break;}
  struct stat code;int codefd=code_open(p,&code);
  if(codefd<0){rc=7;break;}
  if((unsigned long long)code.st_dev!=dev||(unsigned long long)code.st_ino!=ino||(unsigned long long)code.st_size!=size){close(codefd);rc=7;break;}
  size_t bytes=(size_t)pages*(unsigned long)page;
  void *m=mmap(NULL,bytes,PROT_READ,MAP_SHARED,codefd,(off_t)offset);close(codefd);
  if(m==MAP_FAILED){rc=8;break;}
  if(mlock(m,bytes)){munmap(m,bytes);rc=9;break;}
  mappings[count++]=(struct mapping){m,bytes};total+=bytes;
 }
 if(ferror(f))rc=10;fclose(f);if(!total&&!rc)rc=11;
 printf("HOT_CACHE_PREFETCH bytes=%zu ranges=%u elapsed_ms=%.1f result=%d\n",total,count,(now()-begin)*1000,rc);
 if(!rc){
  unsigned long minimum=available();double deadline=now()+600;
  printf("HOT_CACHE_HOLD_BEGIN monotonic=%.3f available_kib=%lu\n",now(),minimum);
  while(!stopped&&same_ready(ready)&&now()<deadline){
   unsigned long n=available();if(n<minimum)minimum=n;if(n<131072){puts("HOT_CACHE_LOW_MEMORY_RELEASE");break;}
   struct timespec delay={0,200000000};nanosleep(&delay,NULL);
  }
  printf("HOT_CACHE_HOLD_END monotonic=%.3f minimum_available_kib=%lu\n",now(),minimum);
 }
 for(unsigned i=0;i<count;i++)munmap(mappings[i].address,mappings[i].bytes);
 puts("HOT_CACHE_RELEASED");return rc;
}
int main(int argc,char **argv){
 if(argc!=3||geteuid()!=0)return 2;long page=sysconf(_SC_PAGESIZE);if(page!=4096)return 2;
 struct stat ready;if(lstat(READY,&ready)||!S_ISREG(ready.st_mode)||ready.st_uid||(ready.st_mode&0777)!=0400)return 3;
 signal(SIGINT,stop);signal(SIGTERM,stop);signal(SIGHUP,stop);setvbuf(stdout,NULL,_IOLBF,0);
 if(!strcmp(argv[1],"snapshot")){char *end;long pid=strtol(argv[2],&end,10);if(*end||pid<=0||pid>2147483647)return 2;return snapshot((int)pid,&ready,page);}
 if(!strcmp(argv[1],"keep"))return keep(argv[2],&ready,page);return 2;
}
