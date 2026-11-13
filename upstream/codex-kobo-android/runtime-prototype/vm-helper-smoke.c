#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
struct request {uint32_t start,length;};
struct hint_request {uint32_t start,length,advice;};
#define HINT _IOW('K',0x42,struct hint_request)
static int random_hint_visible(void *p){
 FILE *f=fopen("/proc/self/smaps","r");if(!f)return -1;char line[1024];unsigned long low,high;int active=0,result=-1;
 while(fgets(line,sizeof(line),f)){if(sscanf(line,"%lx-%lx",&low,&high)==2)active=(unsigned long)p>=low&&(unsigned long)p<high;else if(active&&!strncmp(line,"VmFlags:",8)){result=strstr(line," rr")!=NULL;break;}}fclose(f);return result;
}
#define DISCARD _IOW('K',0x41,struct request)
static void check(int yes,const char *s){if(!yes){perror(s);exit(1);}}
int main(int argc,char **argv){
 if(argc!=3)return 2;
 int fd=open(argv[1],O_RDWR);check(fd>=0,"open helper");
 unsigned char *p=mmap(0,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);check(p!=MAP_FAILED,"anonymous mmap");memset(p,0x5a,4096);
 struct hint_request h={(uint32_t)(uintptr_t)p,4096,1};
 check(ioctl(fd,HINT,&h)==0,"random hint");check(random_hint_visible(p)==1,"random flag visible in smaps");
 h.advice=0;check(ioctl(fd,HINT,&h)==0,"normal hint");check(random_hint_visible(p)==0,"random flag cleared in smaps");
 struct request r={(uint32_t)(uintptr_t)p,4096};
 check(ioctl(fd,DISCARD,&r)==0,"discard anonymous page");for(int i=0;i<4096;i++)check(p[i]==0,"anonymous zero refill");
 r.start++;errno=0;check(ioctl(fd,DISCARD,&r)==-1&&errno==EINVAL,"unaligned rejection");r.start--;
 munmap(p,4096);errno=0;check(ioctl(fd,DISCARD,&r)==-1&&errno==ENOMEM,"unmapped rejection");
 int f=open(argv[2],O_RDWR|O_CREAT|O_EXCL,0600);check(f>=0,"create private test file");unsigned char data[4096];memset(data,'A',4096);check(write(f,data,4096)==4096,"write test file");
 p=mmap(0,4096,PROT_READ|PROT_WRITE,MAP_PRIVATE,f,0);check(p!=MAP_FAILED,"file private mmap");memset(p,'B',4096);r.start=(uint32_t)(uintptr_t)p;
 check(ioctl(fd,DISCARD,&r)==0,"discard private file page");for(int i=0;i<4096;i++)check(p[i]=='A',"original file refill");munmap(p,4096);close(f);unlink(argv[2]);close(fd);
 puts("PASS: page discard, private-file reload, range validation, actual VMA random/normal hints");return 0;
}
