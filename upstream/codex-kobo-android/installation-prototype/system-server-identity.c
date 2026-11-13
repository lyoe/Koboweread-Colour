/* Drop identity after exec: Linux 4.1 has no ambient capabilities. */
extern char*getenv(const char*);extern int strcmp(const char*,const char*);
extern int getuid(void),geteuid(void),getgid(void),getegid(void),setuid(unsigned),setgid(unsigned),setgroups(unsigned,const unsigned*);
extern int prctl(int,...),capset(void*,const void*),capget(void*,void*),setenv(const char*,const char*,int),unsetenv(const char*);
extern int dprintf(int,const char*,...);extern void _exit(int);
struct cap_header{unsigned version;int pid;};struct cap_data{unsigned effective,permitted,inheritable;};
__attribute__((constructor)) static void system_identity(void){
 const char*enabled=getenv("KOBO_SYSTEM_SERVER_UID");if(!enabled||strcmp(enabled,"1000")||geteuid()!=0)_exit(71);
 unsigned bits=(1u<<5)|(1u<<23)|(1u<<24);struct cap_header header={0x20080522,0};struct cap_data data[2]={{bits,bits,0},{0,0,0}};
 if(prctl(8,1,0,0,0)<0||setgroups(0,(void*)0)<0||setgid(1000)<0||setuid(1000)<0||capset(&header,data)<0||prctl(8,0,0,0,0)<0)_exit(72);
 if(getuid()!=1000||geteuid()!=1000||getgid()!=1000||getegid()!=1000||capget(&header,data)<0||data[0].effective!=bits||data[1].effective)_exit(73);
 if(setenv("LD_PRELOAD","/lab/libkobo_vm.so",1)<0||unsetenv("KOBO_SYSTEM_SERVER_UID")<0)_exit(74);
 dprintf(2,"ANDROID_SYSTEM_SERVER_IDENTITY uid=1000 gid=1000 effective_caps=%08x\n",data[0].effective);
}
