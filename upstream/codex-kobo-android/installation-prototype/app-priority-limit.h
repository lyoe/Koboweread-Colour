#ifndef KOBO_APP_PRIORITY_LIMIT_H
#define KOBO_APP_PRIORITY_LIMIT_H
#include <sys/resource.h>
/* Android 7 init.rc sets RLIMIT_NICE to 40/40 before zygote inheritance.
 * Our broker starts outside Android init, so supply the same per-child limit
 * before dropping UID. No application capability is retained.
 * https://android.googlesource.com/platform/system/core/+/android-7.0.0_r1/rootdir/init.rc
 */
static int android_app_priority_limit(void){
 struct rlimit wanted={40,40},actual;
 if(setrlimit(RLIMIT_NICE,&wanted)<0 || getrlimit(RLIMIT_NICE,&actual)<0)return -1;
 return actual.rlim_cur==40 && actual.rlim_max==40?0:-1;
}
#endif
