/* Write a version-1 Linux swap header on a block device (busybox on the
 * Kobo has swapon/swapoff but no mkswap):  kobo-mkswap /dev/zram0
 * First page: zeros, version 1 and the last usable page at offset 1024,
 * "SWAPSPACE2" in the page's last 10 bytes. No bad-page list, no label. */
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <linux/fs.h>
int main(int argc,char **argv){
 if(argc!=2)return 2;
 int fd=open(argv[1],O_WRONLY|O_CLOEXEC);if(fd<0){perror("open");return 3;}
 uint64_t bytes=0;if(ioctl(fd,BLKGETSIZE64,&bytes)<0){perror("size");return 4;}
 long page=sysconf(_SC_PAGESIZE);uint64_t pages=bytes/(uint64_t)page;
 if(pages<10){fprintf(stderr,"too small\n");return 5;}
 unsigned char h[65536];if((size_t)page>sizeof h)return 6;memset(h,0,(size_t)page);
 uint32_t v[3]={1,(uint32_t)(pages-1),0};memcpy(h+1024,v,sizeof v);
 memcpy(h+page-10,"SWAPSPACE2",10);
 if(pwrite(fd,h,(size_t)page,0)!=page||fsync(fd)<0){perror("write");return 7;}
 printf("KOBO_MKSWAP pages=%llu\n",(unsigned long long)pages);return 0;
}
