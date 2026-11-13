/* Restrict this explicitly launched vold to experiment-owned mount metadata.
 * Actual storage commands still execute in the original daemon. */
extern void*dlsym(void*,const char*);
extern int*__errno(void);
extern char*getenv(const char*);
extern int geteuid(void);
extern int dprintf(int,const char*,...);
extern void exit(int);
extern int strcmp(const char*,const char*);
extern char*strchr(const char*,int);
extern int __system_property_get(const char*,char*);
/* Android 7 ARM32 RTLD_NEXT differs from LP64: bionic dlfcn.h. */
static void*(*original_fopen)(const char*,const char*);
static void*(*original_opendir)(const char*);
void*fopen(const char*path,const char*mode){
 if(!original_fopen)original_fopen=dlsym((void*)0xfffffffe,"fopen");
 if(!original_fopen){*__errno()=38;return (void*)0;}
 if(!strcmp(path,"/proc/mounts")||!strcmp(path,"/proc/self/mounts")){
  if(strchr(mode,'w')||strchr(mode,'a')||strchr(mode,'+')){*__errno()=13;return (void*)0;}
  path="/lab/vold-owned-mounts";
 }
 return original_fopen(path,mode);
}
void*fopen64(const char*path,const char*mode){return fopen(path,mode);}
void*opendir(const char*path){
 if(!original_opendir)original_opendir=dlsym((void*)0xfffffffe,"opendir");
 if(!original_opendir){*__errno()=38;return (void*)0;}
 // This storage daemon is not allowed to enumerate or terminate host processes.
 if(!strcmp(path,"/proc")||!strcmp(path,"/proc/"))path="/lab/vold-owned-proc";
 return original_opendir(path);
}
__attribute__((constructor))static void scoped_vold(void){
 const char*enabled=getenv("KOBO_VOLD_PROC_SCOPE");
 if(!enabled||strcmp(enabled,"1")||geteuid()!=0)exit(70);
 char hardware[92]={0},crypto[92]={0};__system_property_get("ro.hardware",hardware);__system_property_get("ro.crypto.state",crypto);
 dprintf(2,"VOLD_EXPERIMENT_PROC_SCOPE hardware=%s crypto=%s\n",hardware,crypto);
}
/* Bionic may bind libc-internal fopen locally, so intercept this entry point too. */
void*setmntent(const char*path,const char*mode){return fopen(path,mode);}
