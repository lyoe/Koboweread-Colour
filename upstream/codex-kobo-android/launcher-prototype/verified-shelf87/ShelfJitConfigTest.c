#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
static uid_t uid;
static int present;
static uid_t fixture_uid(void){return uid;}
static int fixture_access(const char *p,int mode){if(strcmp(p,"/lab/weread-shelf-jit-enabled")||mode!=F_OK)abort();return present?0:-1;}
#define getuid fixture_uid
#define access fixture_access
#include "jit-config-tested87.c"
#undef access
#undef getuid
const struct prop_info *__system_property_find(const char *name){(void)name;return (const struct prop_info*)1;}
int __system_property_read(const struct prop_info *p,char *name,char *value){(void)p;(void)name;strcpy(value,"stock");return 5;}
#define CHECK(x) do{if(!(x)){fprintf(stderr,"CHECK_FAILED line=%d\n",__LINE__);abort();}}while(0)
static void expect(const char *name,const char *value){char out[256];int n=__system_property_get(name,out);CHECK(n==(int)strlen(value)&&!strcmp(value,out));}
int main(void){
 uid=10007;present=0;expect("dalvik.vm.jitthreshold","stock");expect("dalvik.vm.jitinitialsize","256K");
 present=1;expect("dalvik.vm.jitthreshold","2000");expect("dalvik.vm.jitinitialsize","1M");expect("dalvik.vm.jitmaxsize","8M");expect("dalvik.vm.usejit","true");expect("unrelated.property","stock");
 uid=1000;expect("dalvik.vm.jitthreshold","stock");expect("dalvik.vm.jitinitialsize","256K");
 uid=10000;expect("dalvik.vm.jitthreshold","stock");expect("dalvik.vm.jitinitialsize","256K");
 uid=0;expect("dalvik.vm.usejit","stock");uid=10008;expect("dalvik.vm.jitthreshold","stock");
 puts("SHELF87_JIT_PROFILE_UID_MARKER_ISOLATION_DEFAULTS_AND_FALLBACK_OK");return 0;
}
