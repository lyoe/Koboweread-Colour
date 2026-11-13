// Read-only capability probe: never reads input events or grabs the device.
#define _GNU_SOURCE
#include "../runtime-prototype/close-inherited-fds.h"
#include <linux/input.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
int main(void){
 if(close_inherited_fds()<0)return 2;
 int fd=open("/dev/input/event1",O_RDONLY|O_NONBLOCK|O_CLOEXEC|O_NOFOLLOW);
 if(fd<0)return 3;
 char name[128]={0};if(ioctl(fd,EVIOCGNAME(sizeof(name)),name)<0 || strcmp(name,"cyttsp5_mt")){close(fd);return 4;}
 printf("TOUCH_DEVICE_NAME=%s\n",name);
 const int axes[]={ABS_X,ABS_Y,ABS_MT_POSITION_X,ABS_MT_POSITION_Y,ABS_MT_TRACKING_ID,ABS_MT_SLOT,ABS_MT_PRESSURE,ABS_MT_TOUCH_MAJOR};
 for(unsigned i=0;i<sizeof(axes)/sizeof(axes[0]);i++){
  struct input_absinfo info;
  if(ioctl(fd,EVIOCGABS(axes[i]),&info)<0){printf("ABS_AXIS=%d unsupported\n",axes[i]);continue;}
  // Do not include 'value': it may describe the most recent touch.
  printf("ABS_AXIS=%d min=%d max=%d fuzz=%d flat=%d resolution=%d\n",axes[i],info.minimum,info.maximum,info.fuzz,info.flat,info.resolution);
 }
 close(fd);puts("INPUT_METADATA_ONLY_NO_EVENT_READS");return 0;
}
