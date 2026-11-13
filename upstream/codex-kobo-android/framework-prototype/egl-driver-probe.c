// Diagnose the software driver directly, independently of libEGL's dispatch layer.
extern void *dlopen(const char*,int);
extern void *dlsym(void*,const char*);
extern const char *dlerror(void);
extern int printf(const char*,...);
extern void exit(int);
#define LOAD(ret,name,args) ret (*name)args=(ret(*)args)dlsym(driver,#name);if(!name){printf("MISSING %s\n",#name);exit(8);}
__attribute__((constructor)) static void test(void) {
 void *driver=dlopen("/system/lib/egl/libGLES_android.so",2);
 if(!driver){printf("DRIVER_LOAD_FAILED %s\n",dlerror());exit(7);}
 LOAD(void*,eglGetDisplay,(void*)); LOAD(unsigned,eglInitialize,(void*,int*,int*));
 LOAD(unsigned,eglChooseConfig,(void*,const int*,void**,int,int*));
 LOAD(void*,eglCreatePbufferSurface,(void*,void*,const int*));
 LOAD(void*,eglCreateContext,(void*,void*,void*,const int*));
 LOAD(unsigned,eglMakeCurrent,(void*,void*,void*,void*));
 LOAD(int,eglGetError,(void)); LOAD(unsigned,glGetError,(void));
 LOAD(void,glViewport,(int,int,int,int)); LOAD(void,glScissor,(int,int,int,int));
 LOAD(void,glEnable,(unsigned)); LOAD(void,glClearColorx,(int,int,int,int));
 LOAD(void,glClear,(unsigned)); LOAD(void,glReadPixels,(int,int,int,int,unsigned,unsigned,void*));
 void *display=eglGetDisplay(0);int major,minor,count;
 if(!eglInitialize(display,&major,&minor)){printf("DIRECT_INIT_FAILED %x\n",eglGetError());exit(1);}
 int attributes[]={0x3033,1,0x3040,1,0x3024,8,0x3023,8,0x3022,8,0x3038};void *config=0;
 if(!eglChooseConfig(display,attributes,&config,1,&count)||!count)exit(2);
 int surfaceAttributes[]={0x3057,4,0x3056,4,0x3038};int contextAttributes[]={0x3098,1,0x3038};
 void *surface=eglCreatePbufferSurface(display,config,surfaceAttributes);
 void *context=eglCreateContext(display,config,0,contextAttributes);
 if(!surface||!context||!eglMakeCurrent(display,surface,surface,context))exit(3);
 glViewport(0,0,4,4);glScissor(0,0,4,4);glEnable(0x0c11);
 glClearColorx(65536,0,0,65536);glClear(0x4000);
 // Read-only diagnostic for this specific libagl C++ object ABI, never used by the display backend.
 struct ggl_surface { unsigned version,width,height;int stride;unsigned *data;unsigned char format,pad[3];int compressed;void *reserved; };
 struct pbuffer_object { void *vtable;unsigned magic;void *display,*config,*context;unsigned zombie;struct ggl_surface depth,pixels; };
 struct pbuffer_object *object=(struct pbuffer_object*)surface;
 if(object->magic==0x31415265 && object->pixels.version==sizeof(struct ggl_surface)
    && object->pixels.width==4 && object->pixels.height==4 && object->pixels.data
    && (object->pixels.format==1 || object->pixels.format==2)) {
  printf("DIRECT_PBUFFER_RAW format=%u first=%08x last=%08x\n",object->pixels.format,object->pixels.data[0],object->pixels.data[15]);
 } else printf("DIRECT_PBUFFER_ABI_UNRECOGNIZED\n");
 unsigned char pixel[64];for(unsigned i=0;i<64;i++)pixel[i]=0xcd;
 glReadPixels(0,0,4,4,0x1908,0x1401,pixel);
 unsigned error=glGetError();
 printf("DIRECT_DRIVER_PIXEL error=%x rgba=%u,%u,%u,%u last=%u,%u,%u,%u\n",error,pixel[0],pixel[1],pixel[2],pixel[3],pixel[60],pixel[61],pixel[62],pixel[63]);
 for(unsigned i=0;i<16;i++)if(error||pixel[4*i]<248||pixel[4*i+1]>7||pixel[4*i+2]>7)exit(4);
 printf("DIRECT_SOFTWARE_EGL_PIXELS_OK\n");exit(0);
}
