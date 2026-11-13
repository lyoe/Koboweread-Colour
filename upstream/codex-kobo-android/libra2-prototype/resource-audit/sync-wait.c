// SPDX-License-Identifier: MIT
// Android ARM32 preload probe: preserve sync operations, log fixed metadata only.
#include <stddef.h>
#include <stdint.h>
extern int getuid(void),getpid(void),access(const char *,int);
extern long syscall(long,...);
extern int *__errno(void);
extern long write(int,const void *,size_t);
extern long readlink(const char *,char *,size_t);
extern int snprintf(char *,size_t,const char *,...);
extern char *strstr(const char *,const char *);
extern int strncmp(const char *,const char *,size_t);
struct stamp {long seconds,nanoseconds;};
extern int clock_gettime(int,struct stamp *);
static unsigned records;
static int enabled;
static uint64_t deadline;
static uint64_t clock_ns(int which){struct stamp t;if(clock_gettime(which,&t))return 0;return (uint64_t)t.seconds*1000000000+(unsigned long)t.nanoseconds;}
static const char *file_kind(int fd){
 char p[64],name[512];snprintf(p,sizeof p,"/proc/self/fd/%d",fd);long n=readlink(p,name,sizeof name-1);if(n<0)return "unknown";name[n]=0;
 if(strstr(name,"-wal")||strstr(name,"-journal"))return "journal";
 if(strstr(name,"/databases/")||strstr(name,".db")||strstr(name,".sqlite"))return "database";
 if(!strncmp(name,"/data/",6))return "data";
 if(!strncmp(name,"/system/",8))return "system";
 return "other";
}
__attribute__((constructor)) static void start(void){
 enabled=getuid()==10007&&access("/lab/sync-wait/enabled",0)==0;
 deadline=clock_ns(1)+600000000000ULL;
 if(enabled){char line[180];int n=snprintf(line,sizeof line,"SYNC_WAIT_READY pid=%d arm_kernel_sync_syscalls=1 cap=2048 max_seconds=600\n",getpid());if(n>0&&(size_t)n<sizeof line)write(2,line,(size_t)n);}
}
static int forward(int fd,int data_only){
 int saved=*__errno();
 // The collector creates this marker only after interactive_ready. Startup
 // traffic must not consume the bounded operation sample before user input.
 int active=enabled&&__atomic_load_n(&records,__ATOMIC_RELAXED)<2048&&clock_ns(1)<deadline&&access("/lab/sync-wait/collect",0)==0;
 const char *kind=active?file_kind(fd):"unknown";
 uint64_t began=active?clock_ns(1):0,cpu=active?clock_ns(3):0;
 *__errno()=saved;
 int result;
 // API 24 ARM Bionic sync wrappers map to these same Linux syscalls. Avoid
 // cross-ABI libdl constants: Android ARM32 RTLD_NEXT is -2, not -1.
 result=(int)syscall(data_only?148L:118L,fd);
 int error=*__errno();
 if(active){
  uint64_t end=clock_ns(1),cpu_end=clock_ns(3);unsigned index=__sync_fetch_and_add(&records,1);
  if(index<2048){char line[256];int valid=began&&cpu&&end>=began&&cpu_end>=cpu;int n=snprintf(line,sizeof line,"SYNC_WAIT pid=%d tid=%ld operation=%s kind=%s result=%d errno=%d clock_valid=%d monotonic_ns=%llu wall_ns=%llu cpu_ns=%llu\n",getpid(),syscall(224L),data_only?"fdatasync":"fsync",kind,result,result<0?error:0,valid,(unsigned long long)began,(unsigned long long)(valid?end-began:0),(unsigned long long)(valid?cpu_end-cpu:0));if(n>0&&(size_t)n<sizeof line)write(2,line,(size_t)n);}
 }
 *__errno()=error;return result;
}
int fsync(int fd){return forward(fd,0);}
int fdatasync(int fd){return forward(fd,1);}
