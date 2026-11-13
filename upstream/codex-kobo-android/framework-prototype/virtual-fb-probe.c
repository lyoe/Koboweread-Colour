#include <linux/fb.h>
#include "../libra2-prototype/device-profile.h"
extern int open(const char*,int,...);
extern int close(int);
extern int ioctl(int,unsigned long,...);
extern void *mmap(void*,unsigned,int,int,int,int);
extern int munmap(void*,unsigned);
extern int *__errno(void);
extern int printf(const char*,...);
extern void exit(int);
__attribute__((constructor)) static void test(void) {
 int fd=open("/dev/graphics/fb0",2);
 struct fb_fix_screeninfo fixed;struct fb_var_screeninfo variable;
 if(fd<0 || ioctl(fd,FBIOGET_FSCREENINFO,&fixed)<0 || ioctl(fd,FBIOGET_VSCREENINFO,&variable)<0)exit(1);
 if(variable.pixclock!=KOBO_FB_PIXCLOCK_30 && variable.pixclock!=(KOBO_FB_PIXCLOCK_30*2))exit(9);
 if(variable.xres!=KOBO_FB_WIDTH||variable.yres!=KOBO_FB_HEIGHT||variable.bits_per_pixel!=32||fixed.line_length!=(KOBO_FB_WIDTH*4))exit(2);
 unsigned *pixels=mmap(0,fixed.smem_len,3,1,fd,0);
 if(pixels==(void*)-1)exit(3);
 unsigned at=KOBO_FB_WIDTH*KOBO_FB_HEIGHT;pixels[at]=0xff123456;
 variable.yoffset=KOBO_FB_HEIGHT;
 if(ioctl(fd,FBIOPAN_DISPLAY,&variable)<0)exit(4);
 if(ioctl(fd,FBIOGET_VSCREENINFO,&variable)<0||variable.yoffset!=KOBO_FB_HEIGHT||pixels[at]!=0xff123456)exit(5);
 variable.yoffset=(KOBO_FB_HEIGHT+1);
 if(ioctl(fd,FBIOPAN_DISPLAY,&variable)!=-1||*__errno()!=22)exit(6);
 if(ioctl(fd,0xdead,&variable)!=-1||*__errno()!=25)exit(7);
 pixels[at]=0;variable.yoffset=0;
 if(ioctl(fd,FBIOPAN_DISPLAY,&variable)<0)exit(8);
 munmap(pixels,fixed.smem_len);close(fd);
 printf("VIRTUAL_FB_MEMORY_AND_PAN_OK fix=%u var=%u size=%u pixclock=%u\n",(unsigned)sizeof(fixed),(unsigned)sizeof(variable),fixed.smem_len,variable.pixclock);
 exit(0);
}
