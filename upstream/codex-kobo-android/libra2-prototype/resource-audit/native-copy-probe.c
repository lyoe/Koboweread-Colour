#include <stddef.h>
#include <stdint.h>
extern void *dlopen(const char *,int);
extern void *dlsym(void *,const char *);
extern void *malloc(size_t);
extern void free(void *);
struct stamp {long seconds,nanoseconds;};
extern int clock_gettime(int,struct stamp *);
static uint64_t cpu(void){struct stamp t;if(clock_gettime(3,&t))return 0;return (uint64_t)t.seconds*1000000000+(unsigned long)t.nanoseconds;}
static void *object(int candidate){return dlopen(candidate?"libc.so":"/data/local/tmp/copy-probe/old-fb.so",0);}
long long Java_CopyProbe_pagesize(void *env,void *type,int candidate){
 (void)env;(void)type;void *h=object(candidate);if(!h)return -1;
 unsigned long (*aux)(unsigned long)=(unsigned long(*)(unsigned long))dlsym(h,"getauxval");return aux?(long long)aux(6):-2;
}
long long Java_CopyProbe_measure(void *env,void *type,int candidate){
 (void)env;(void)type;void *h=object(candidate);if(!h)return -1;
 void *(*copy)(void *,const void *,size_t)=(void *(*)(void *,const void *,size_t))dlsym(h,"memcpy");if(!copy)return -2;
 size_t bytes=2*1024*1024;unsigned char *src=malloc(bytes),*dst=malloc(bytes);if(!src||!dst){free(src);free(dst);return -3;}
 for(size_t i=0;i<bytes;i++){src[i]=(unsigned char)(i*17+3);dst[i]=0;}
 copy(dst,src,bytes);uint64_t start=cpu();
 for(unsigned n=0;n<64;n++){src[0]=(unsigned char)n;src[bytes-1]=(unsigned char)(n+1);copy(dst,src,bytes);if(dst[0]!=src[0]||dst[bytes-1]!=src[bytes-1]){free(src);free(dst);return -4;}}
 uint64_t end=cpu();int valid=1;for(size_t i=4096;i<bytes-1;i+=4096)if(src[i]!=dst[i])valid=0;
 free(src);free(dst);return valid&&end>=start?(long long)(end-start):-5;
}

#ifdef COPY_PROBE_CONSTRUCTOR
extern int snprintf(char *,size_t,const char *,...);
extern long write(int,const void *,size_t);
extern void _exit(int);
__attribute__((constructor)) static void run_probe(void){
 char line[256];int n=snprintf(line,sizeof line,"COPY_AUX old=%lld libc=%lld\n",Java_CopyProbe_pagesize(0,0,0),Java_CopyProbe_pagesize(0,0,1));write(1,line,(size_t)n);
 for(int i=0;i<3;i++)for(int candidate=0;candidate<2;candidate++){
  long long t=Java_CopyProbe_measure(0,0,candidate);if(t<0)_exit(6);
  n=snprintf(line,sizeof line,"COPY_CPU round=%d source=%s bytes=134217728 ns=%lld\n",i,candidate?"bionic":"old-shim",t);write(1,line,(size_t)n);
 }
 write(1,"COPY_PROBE_PASS\n",16);_exit(0);
}
#endif
