// Production touch entry regression. Stops before opening/grabbing real input.
#define _GNU_SOURCE
#include <linux/input.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "../runtime-prototype/close-inherited-fds.h"
#include "../launcher-prototype/handoff-prototype/reading-lease.h"
#define READER_EXE "/mnt/onboard/.adds/koreader/luajit"
#include "reader-identity.h"
static int lease_present,lease_ok,readers_ok,lease_calls,reader_calls,input_calls;
static int fixture_access(const char *p,int mode){(void)mode;if(strcmp(p,HANDOFF_LEASE))abort();return lease_present?0:-1;}
static int fixture_lease(void){lease_calls++;return lease_ok;}
static int fixture_readers(struct reader *r,char state){(void)r;if(state!='T')abort();reader_calls++;return readers_ok?0:-1;}
static int fixture_open(const char *p,int flags,...){(void)flags;if(strcmp(p,"/dev/input/event1"))abort();input_calls++;errno=EACCES;return -1;}
#define access fixture_access
#define reading_valid fixture_lease
#define find_readers fixture_readers
#define open fixture_open
#define main touch_entry
#include "touch-forward.c"
#undef main
#undef open
#undef find_readers
#undef reading_valid
#undef access
#define CHECK(x) do{if(!(x)){fprintf(stderr,"CHECK_FAILED line=%d\n",__LINE__);abort();}}while(0)
static void one(int present,int valid,int legacy,int result){
 lease_present=present;lease_ok=valid;readers_ok=legacy;lease_calls=reader_calls=input_calls=0;
 char *args[]={"touch-forward","180","--interactive",NULL};
 CHECK(touch_entry(3,args)==result);
 CHECK(lease_calls==(present?1:0));CHECK(reader_calls==(present?0:1));CHECK(input_calls==(result==5?1:0));
}
int main(void){
 one(1,1,0,5); /* A valid independent lease reaches input even with no readers. */
 one(1,0,1,4); /* Invalid independent lease cannot fall back to legacy readers. */
 one(0,0,1,5);one(0,1,0,4); /* Legacy still requires exactly paused readers. */
 puts("TOUCH86_PRODUCTION_ENTRY_VALID_INDEPENDENT_INVALID_LEASE_AND_LEGACY_OWNER_BRANCHES_OK");return 0;
}
