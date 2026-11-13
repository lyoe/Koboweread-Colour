#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
extern int __system_property_get(const char*,char*);
__attribute__((constructor)) static void probe(void){
 char min[128],max[128],jit[128];__system_property_get("dalvik.vm.heapminfree",min);__system_property_get("dalvik.vm.heapmaxfree",max);__system_property_get("dalvik.vm.jitinitialsize",jit);
 printf("HEAP92_REAL_BIONIC uid=%u min=%s max=%s jitinitial=%s\n",(unsigned)getuid(),min,max,jit);
 const char *expected=getenv("EXPECT_HEAP");if(!expected)_exit(2);
 if(!strcmp(expected,"enabled") && (getuid()!=10007||strcmp(min,"4m")||strcmp(max,"8m")||strcmp(jit,"256K")))exit(3);
 if(!strcmp(expected,"baseline") && (!strcmp(min,"4m")||!strcmp(max,"8m")))exit(4);
 puts("HEAP92_REAL_BIONIC_PROPERTIES_OK");exit(0);
}
