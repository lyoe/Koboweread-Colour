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
 load("/default.prop");load("/system/build.prop");
 char value[92];
 if(__system_property_get("ro.product.cpu.abilist32",value)<=0){printf("PROPERTY_READBACK_FAILED\n");exit(3);}
 printf("PROPERTY_ABI %s\n",value);
 printf("ANDROID_PROPERTIES_LOADED %u\n",loaded);exit(loaded?0:2);
}
