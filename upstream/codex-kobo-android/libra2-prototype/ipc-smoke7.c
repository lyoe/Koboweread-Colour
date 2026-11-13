#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>
#define BINDER_IPC_32BIT 1
#include "../module-prototype/binder/binder.h"
_Static_assert(sizeof(struct binder_write_read)==24,"Protocol7 layout");
#define ASHMEM_SET_NAME _IOW(0x77,1,char[256])
#define ASHMEM_SET_SIZE _IOW(0x77,3,size_t)
#define ASHMEM_GET_SIZE _IO(0x77,4)
static void check(int yes,const char *msg) { if (!yes) { perror(msg); exit(1); } }
int main(void) {
 int fd=open("/tmp/kobo-libra2-module-test/ashmem",O_RDWR); check(fd>=0,"open ashmem");
 char name[256]="kobo-android-smoke";
 check(ioctl(fd,ASHMEM_SET_NAME,name)==0,"ashmem name");
 check(ioctl(fd,ASHMEM_SET_SIZE,4096)==0,"ashmem size");
 check(ioctl(fd,ASHMEM_GET_SIZE)==4096,"ashmem get size");
 char *p=mmap(NULL,4096,PROT_READ|PROT_WRITE,MAP_SHARED,fd,0);check(p!=MAP_FAILED,"ashmem mmap");
 strcpy(p,"parent");pid_t child=fork();check(child>=0,"fork");
 if (!child) { strcpy(p,"child-shared-ok"); _exit(0); }
 int status=0;check(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==0,"wait child");
 check(strcmp(p,"child-shared-ok")==0,"shared data");munmap(p,4096);close(fd);
 puts("PASS: ashmem name, size, mmap, cross-process shared writes");
 fd=open("/tmp/kobo-libra2-module-test/binder",O_RDWR);check(fd>=0,"open binder");struct binder_version v={0};
 check(ioctl(fd,BINDER_VERSION,&v)==0,"binder version");
 printf("Binder protocol: %d\n",v.protocol_version);check(v.protocol_version==7,"expected Binder protocol 7");
 void *buf=mmap(NULL,1024*1024,PROT_READ,MAP_PRIVATE,fd,0);check(buf!=MAP_FAILED,"binder mmap");
 uint32_t threads=1;check(ioctl(fd,BINDER_SET_MAX_THREADS,&threads)==0,"binder set threads");
 struct binder_write_read wr={0};check(ioctl(fd,BINDER_WRITE_READ,&wr)==0,"binder empty write/read");
 munmap(buf,1024*1024);close(fd);
 puts("PASS: binder protocol 7, mmap, thread configuration, empty ioctl");
 puts("These tests do not verify Binder transactions or Android startup.");return 0;
}
