#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
static uid_t uid;static int present,calls;
static uid_t fixture_uid(void){return uid;}
static int fixture_access(const char *p,int mode){if(strcmp(p,"/lab/weread-heap-headroom-enabled")||mode!=F_OK)abort();calls++;return present?0:-1;}
#define getuid fixture_uid
#define access fixture_access
#include "../../app-process-prototype/jit-config.c"
#undef access
#undef getuid
const struct prop_info *__system_property_find(const char *name){(void)name;return (const struct prop_info*)1;}
int __system_property_read(const struct prop_info *p,char *name,char *value){(void)p;(void)name;strcpy(value,"stock");return 5;}
static void expect(const char *name,const char *value){char out[256];int n=__system_property_get(name,out);if(n!=(int)strlen(value)||strcmp(value,out))abort();}
int main(void){
 uid=10007;present=0;expect("dalvik.vm.heapminfree","stock");expect("dalvik.vm.heapmaxfree","stock");
 present=1;expect("dalvik.vm.heapminfree","4m");expect("dalvik.vm.heapmaxfree","8m");
 expect("dalvik.vm.heapgrowthlimit","stock");expect("dalvik.vm.heapsize","stock");expect("dalvik.vm.heaptargetutilization","stock");expect("dalvik.vm.jitthreshold","stock");expect("dalvik.vm.jitinitialsize","256K");expect("dalvik.vm.jitmaxsize","8M");
 for(int i=0;i<4;i++){uid=(uid_t[]){0,1000,10000,10008}[i];int before=calls;expect("dalvik.vm.heapminfree","stock");expect("dalvik.vm.heapmaxfree","stock");if(before!=calls)abort();}
 uid=1000;expect("dalvik.vm.usejit","true");uid=10000;expect("dalvik.vm.jitmaxsize","8M");uid=10008;expect("dalvik.vm.usejit","stock");
 puts("HEAP92_UID_MARKER_ISOLATION_BASELINE_HEAP_LIMITS_AND_JIT_UNCHANGED_OK");return 0;
}
