/* Push cold anonymous memory into zram once:  kobo-squeeze MB
 * This kernel (4.1) swaps anonymous pages only when the inactive file list
 * runs low, and it has no memory cgroups or per-process reclaim, so pages
 * nobody touches (KOReader stopped behind Android, idle helpers) never move
 * on their own. Touching MB of fresh anonymous memory forces reclaim; with
 * swappiness 100 the least recently used anonymous pages go to zram along
 * with some file cache. The memory is returned at exit. Stops early when
 * MemAvailable would fall under 24 MB. Prints swap use before and after. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
static long meminfo(const char *key){
 FILE *f=fopen("/proc/meminfo","r");char line[128];long v=-1;size_t n=strlen(key);
 if(!f)return -1;
 while(fgets(line,sizeof line,f))if(!strncmp(line,key,n)&&line[n]==':'){v=atol(line+n+1);break;}
 fclose(f);return v;
}
int main(int argc,char **argv){
 if(argc!=2)return 2;
 long mb=atol(argv[1]);if(mb<1||mb>256)return 2;
 struct timespec t0,t1;clock_gettime(CLOCK_MONOTONIC,&t0);
 long swap0=meminfo("SwapTotal")-meminfo("SwapFree"),cached0=meminfo("Cached");
 size_t step=4u<<20;long done=0;
 for(long i=0;i<mb;i+=4){
  if(meminfo("MemAvailable")<24*1024 && meminfo("SwapFree")<8*1024)break;
  char *p=mmap(0,step,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
  if(p==MAP_FAILED)break;
  for(size_t o=0;o<step;o+=4096)p[o]=(char)(o>>12|1);
  done+=4;
 }
 clock_gettime(CLOCK_MONOTONIC,&t1);
 long swap1=meminfo("SwapTotal")-meminfo("SwapFree");
 printf("KOBO_SQUEEZE touched_mb=%ld swap_kb=%ld->%ld cached_kb=%ld->%ld seconds=%.1f\n",done,swap0,swap1,cached0,meminfo("Cached"),(t1.tv_sec-t0.tv_sec)+(t1.tv_nsec-t0.tv_nsec)/1e9);
 return 0;
}
