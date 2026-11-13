// CPU framebuffer backed by a regular file inside the experimental root.
// Never opens the real Kobo display; unsupported ioctls return ENOTTY.
#include <linux/fb.h>
#include "../libra2-prototype/device-profile.h"
#include <stdarg.h>
extern long syscall(long,...);
extern int readlink(const char*,char*,unsigned);
extern int snprintf(char*,unsigned,const char*,...);
extern int strcmp(const char*,const char*);
extern void *memcpy(void*,const void*,unsigned);
extern void *memset(void*,int,unsigned);
extern int open(const char*,int,...);
extern int close(int);
extern int access(const char*,int);
extern int pwrite(int,const void*,unsigned,int);
extern int *__errno(void);
extern int pthread_mutex_lock(void*);
extern int pthread_mutex_unlock(void*);
static unsigned lock;
static struct fb_var_screeninfo variable;
static unsigned sequence;
static void initialize(void) {
 if(variable.xres)return;
 variable.xres=variable.xres_virtual=KOBO_FB_WIDTH;
 variable.yres=KOBO_FB_HEIGHT;variable.yres_virtual=(KOBO_FB_HEIGHT*2);variable.bits_per_pixel=32;
 variable.red.offset=0;variable.red.length=8;
 variable.green.offset=8;variable.green.length=8;
 variable.blue.offset=16;variable.blue.length=8;
 variable.transp.offset=24;variable.transp.length=8;
 variable.width=KOBO_FB_WIDTH_MM;variable.height=KOBO_FB_HEIGHT_MM;
 variable.pixclock=access("/lab/display-cadence15-enabled",0)==0?(KOBO_FB_PIXCLOCK_30*2):KOBO_FB_PIXCLOCK_30;
}
static int is_backend(int fd) {
 char path[48],target[256];
 snprintf(path,sizeof(path),"/proc/self/fd/%d",fd);
 int n=readlink(path,target,sizeof(target)-1);
 if(n<0)return 0;target[n]=0;
 return strcmp(target,"/dev/graphics/fb0")==0;
}
static void publish(void) {
 unsigned state[]={0x4b464230,KOBO_FB_WIDTH,KOBO_FB_HEIGHT,(KOBO_FB_WIDTH*4),variable.yoffset,++sequence};
 int fd=open("/lab/virtual-fb.state",1|64|02000000,0600);
 if(fd>=0){pwrite(fd,state,sizeof(state),0);close(fd);}
}
int ioctl(int fd,unsigned long request,...) {
 va_list args;va_start(args,request);void *argument=va_arg(args,void*);va_end(args);
 if(!is_backend(fd))return (int)syscall(54L,fd,request,argument);
 pthread_mutex_lock(&lock);initialize();int rc=0;
 if(request==FBIOGET_FSCREENINFO && argument) {
  struct fb_fix_screeninfo fixed;memset(&fixed,0,sizeof(fixed));
  memcpy(fixed.id,"kobo-soft-fb",12);fixed.smem_len=KOBO_FB_WIDTH*KOBO_FB_HEIGHT*4*2;
  fixed.type=FB_TYPE_PACKED_PIXELS;fixed.visual=FB_VISUAL_TRUECOLOR;
  fixed.ypanstep=1;fixed.line_length=KOBO_FB_WIDTH*4;
  memcpy(argument,&fixed,sizeof(fixed));
 } else if(request==FBIOGET_VSCREENINFO && argument) {
  memcpy(argument,&variable,sizeof(variable));
 } else if((request==FBIOPUT_VSCREENINFO || request==FBIOPAN_DISPLAY) && argument) {
  struct fb_var_screeninfo *wanted=argument;
  if(wanted->xres!=KOBO_FB_WIDTH || wanted->yres!=KOBO_FB_HEIGHT || wanted->xres_virtual!=KOBO_FB_WIDTH
     || wanted->yres_virtual>(KOBO_FB_HEIGHT*2) || wanted->yres_virtual<KOBO_FB_HEIGHT
     || wanted->bits_per_pixel!=32 || wanted->xoffset!=0 || wanted->yoffset>KOBO_FB_HEIGHT) {
   *__errno()=22;rc=-1;
  } else {
   variable.yoffset=wanted->yoffset;variable.activate=wanted->activate;
   publish();memcpy(argument,&variable,sizeof(variable));
  }
 } else if(request==FBIOBLANK) {
  // No physical backlight or power state; the memory-backed display stays available.
 } else { *__errno()=25;rc=-1; }
 pthread_mutex_unlock(&lock);return rc;
}
