#include <string.h>
#include <unistd.h>
struct prop_info;
extern const struct prop_info *__system_property_find(const char *);
extern int __system_property_read(const struct prop_info *,char *,char *);
/* Per-process ART configuration. The private emulator base property disables
 * JIT globally; AndroidRuntime appends that property after command-line flags.
 * Load only in the fixed UID probe, enabled WeRead process, or root-marked system server.
 * All other properties use original Bionic lookup/read. Avoid dlsym here:
 * property reads occur during the dynamic linker's initialization.
 */
int __system_property_get(const char *name,char *value){
 const char *setting=0;uid_t uid=getuid();
 if(uid==1000||uid==10000||uid==10007){
  if(!strcmp(name,"dalvik.vm.usejit"))setting="true";
  else if(!strcmp(name,"dalvik.vm.usejitprofiles"))setting="false";
  else if(!strcmp(name,"dalvik.vm.jitinitialsize"))setting="256K";
  else if(!strcmp(name,"dalvik.vm.jitmaxsize"))setting="8M";
 }
 if(setting){size_t n=strlen(setting);memcpy(value,setting,n+1);return (int)n;}
 const struct prop_info *property=__system_property_find(name);
 if(!property){value[0]=0;return 0;}
 return __system_property_read(property,0,value);
}
