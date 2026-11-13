#ifndef KOBO_CLOSE_INHERITED_FDS_H
#define KOBO_CLOSE_INHERITED_FDS_H
#include <dirent.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
/* Run before chroot/socket creation: SSH launched by KOReader can inherit
 * open host device descriptors, which chroot alone does not revoke. */
static int close_inherited_fds(void) {
 DIR *directory=opendir("/proc/self/fd");
 if(!directory)return -1;
 int own=dirfd(directory),count=0,result=0;
 for(;;){
  errno=0;struct dirent *entry=readdir(directory);
  if(!entry){if(errno)result=-1;break;}
  char *end;long descriptor=strtol(entry->d_name,&end,10);
  if(*entry->d_name && !*end && descriptor>2 && descriptor!=own){
   if(close((int)descriptor)<0){result=-1;break;}count++;
  }
 }
 int saved=errno;closedir(directory);errno=saved;
 return result<0?-1:count;
}
#endif
