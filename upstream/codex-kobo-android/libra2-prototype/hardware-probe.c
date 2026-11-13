#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <linux/input.h>
int main(void){
 if(getuid()!=0)return 2;
 int fb=open("/dev/fb0",O_RDONLY|O_CLOEXEC|O_NOFOLLOW);if(fb<0)return 3;
 struct fb_var_screeninfo v;struct fb_fix_screeninfo f;
 if(ioctl(fb,FBIOGET_VSCREENINFO,&v)||ioctl(fb,FBIOGET_FSCREENINFO,&f))return 4;close(fb);
 printf("FB_GEOMETRY x=%u y=%u virtual_x=%u virtual_y=%u xoffset=%u yoffset=%u bpp=%u rotate=%u grayscale=%u line_length=%u memory_bytes=%u\n",v.xres,v.yres,v.xres_virtual,v.yres_virtual,v.xoffset,v.yoffset,v.bits_per_pixel,v.rotate,v.grayscale,f.line_length,f.smem_len);
 for(int i=0;i<16;i++){
  char path[64],name[128]={0};snprintf(path,sizeof(path),"/dev/input/event%d",i);int fd=open(path,O_RDONLY|O_NONBLOCK|O_CLOEXEC|O_NOFOLLOW);if(fd<0)continue;
  if(ioctl(fd,EVIOCGNAME(sizeof(name)),name)<0){close(fd);continue;}printf("INPUT path=%s name=%s\n",path,name);
  const int axes[]={ABS_X,ABS_Y,ABS_MT_SLOT,ABS_MT_TRACKING_ID,ABS_MT_POSITION_X,ABS_MT_POSITION_Y};
  for(unsigned n=0;n<sizeof(axes)/sizeof(axes[0]);n++){struct input_absinfo a;if(ioctl(fd,EVIOCGABS(axes[n]),&a)==0)printf("INPUT_AXIS code=%d min=%d max=%d resolution=%d\n",axes[n],a.minimum,a.maximum,a.resolution);}
  unsigned long bits[(KEY_MAX+8*sizeof(long))/(8*sizeof(long))];memset(bits,0,sizeof(bits));
  if(ioctl(fd,EVIOCGBIT(EV_KEY,sizeof(bits)),bits)>=0){const int keys[]={KEY_POWER,KEY_PAGEUP,KEY_PAGEDOWN,193,194};for(unsigned n=0;n<sizeof(keys)/sizeof(keys[0]);n++)if(bits[keys[n]/(8*sizeof(long))]&(1UL<<(keys[n]%(8*sizeof(long)))))printf("INPUT_KEY code=%d\n",keys[n]);}
  close(fd);
 }
 return 0;
}
