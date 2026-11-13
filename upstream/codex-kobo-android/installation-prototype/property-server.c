// Initialize a real Android property area inside the experiment chroot only.
extern int __system_property_area_init(void);
extern int __system_property_get(const char*,char*);
extern int __system_property_add(const char*,unsigned,const char*,unsigned);
extern unsigned strlen(const char*);
extern void *fopen(const char*,const char*);
extern char *fgets(char*,int,void*);
extern int fclose(void*);
extern int printf(const char*,...);
extern void exit(int);
extern int *__errno(void);
extern char *strerror(int);
extern void *memset(void*,int,unsigned);
extern void *memchr(const void*,int,unsigned);
extern int strcmp(const char*,const char*);
extern int strncmp(const char*,const char*,unsigned);
extern char *getenv(const char*);
extern int socket(int,int,int);
extern int bind(int,const void*,unsigned);
extern int listen(int,int);
extern int accept(int,void*,unsigned*);
extern int close(int);
extern int unlink(const char*);
extern int chmod(const char*,unsigned);
extern int chown(const char*,unsigned,unsigned);
extern int read(int,void*,unsigned);
extern int setsockopt(int,int,int,const void*,unsigned);
extern int getsockopt(int,int,int,void*,unsigned*);
extern int puts(const char*);
extern int fflush(void*);
extern const void *__system_property_find(const char*);
extern int __system_property_update(void*,const char*,unsigned);
struct local_address { unsigned short family; char path[108]; };
struct property_message { unsigned command; char name[32]; char value[92]; };
struct credentials { int pid; unsigned uid,gid; };
static void serve(void) {
 const char *path="/dev/socket/property_service";
 struct local_address address={1,"/dev/socket/property_service"};
 int server=socket(1,1,0);
 if(server<0){puts("PROPERTY_SOCKET_FAILED");exit(4);}
 unlink(path);
 if(bind(server,&address,sizeof(address))<0 || chown(path,0,1000)<0 || chmod(path,0660)<0 || listen(server,8)<0) {
  puts("PROPERTY_BIND_FAILED");close(server);exit(5);
 }
 puts("ANDROID_PROPERTY_SERVER_READY root-and-system-only volatile");fflush((void*)0);
 for(;;) {
  int client=accept(server,(void*)0,(void*)0);
  if(client<0){if(*__errno()==4)continue;exit(6);}
  struct credentials peer;unsigned length=sizeof(peer);
  if(getsockopt(client,1,17,&peer,&length)<0 || (peer.uid!=0 && peer.uid!=1000)){close(client);continue;}
  struct {int seconds,microseconds;} timeout={2,0};
  setsockopt(client,1,20,&timeout,sizeof(timeout));
  struct property_message msg;unsigned received=0;
  while(received<sizeof(msg)) {
   int n=read(client,(char*)&msg+received,sizeof(msg)-received);
   if(n<=0)break;received+=(unsigned)n;
  }
  if(received==sizeof(msg) && msg.command==1
      && memchr(msg.name,0,sizeof(msg.name)) && memchr(msg.value,0,sizeof(msg.value))
      && msg.name[0] && strncmp(msg.name,"ro.",3) && strncmp(msg.name,"ctl.",4)) {
   const void *existing=__system_property_find(msg.name);
   int rc=existing?__system_property_update((void*)existing,msg.value,strlen(msg.value))
      :__system_property_add(msg.name,strlen(msg.name),msg.value,strlen(msg.value));
   if(rc)printf("PROPERTY_UPDATE_FAILED %s\n",msg.name);
  }
  close(client);
 }
}
static unsigned loaded;
static void load(const char *name) {
 void *f=fopen(name,"r");if(!f)return;
 char line[1024];
 while(fgets(line,sizeof(line),f)) {
  if(line[0]=='#')continue;
  char *p=line;while(*p && *p!='=')p++;if(*p!='=')continue;*p++=0;
  unsigned n=strlen(p);while(n&&(p[n-1]=='\n'||p[n-1]=='\r'))p[--n]=0;
  if(strlen(line)<32 && n<92 && __system_property_add(line,strlen(line),p,n)==0)loaded++;
 }
 fclose(f);
}
__attribute__((constructor)) static void initialize(void) {
 int rc=__system_property_area_init();
 if(rc!=0 && rc!=-2){printf("PROPERTY_AREA_INIT_FAILED errno=%d %s\n",*__errno(),strerror(*__errno()));exit(1);}
 printf("PROPERTY_AREA_STATUS %d (no SELinux on Kobo)\n",rc);
#ifdef KOBO_LIBRA2_NATIVE
 if(__system_property_add("ro.sf.lcd_density",strlen("ro.sf.lcd_density"),"320",3)!=0){puts("NATIVE_DENSITY_PROPERTY_FAILED");exit(7);}
 puts("NATIVE_DENSITY_PROPERTY_READY dpi=320");
#endif
 load("/default.prop");load("/system/build.prop");
 char value[92];
 if(__system_property_get("ro.product.cpu.abilist32",value)<=0){printf("PROPERTY_READBACK_FAILED\n");exit(3);}
 printf("PROPERTY_ABI %s\n",value);
 printf("ANDROID_PROPERTIES_LOADED %u\n",loaded);
 if(!loaded)exit(2);
 if(getenv("KOBO_PROPERTY_DAEMON"))serve();
 exit(0);
}
