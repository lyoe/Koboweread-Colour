// Android software GLES1 pbuffer: no real framebuffer device is opened.
extern void *eglGetDisplay(void*);
extern unsigned eglInitialize(void*,int*,int*);
extern unsigned eglChooseConfig(void*,const int*,void**,int,int*);
extern void *eglCreatePbufferSurface(void*,void*,const int*);
extern void *eglCreateContext(void*,void*,void*,const int*);
extern unsigned eglMakeCurrent(void*,void*,void*,void*);
extern unsigned eglDestroyContext(void*,void*);
extern unsigned eglDestroySurface(void*,void*);
extern unsigned eglTerminate(void*);
extern int eglGetError(void);
extern const char *eglQueryString(void*,int);
extern const char *glGetString(unsigned);
extern void glClearColorx(int,int,int,int);
extern void glViewport(int,int,int,int);
extern void glScissor(int,int,int,int);
extern void glEnable(unsigned);
extern void glFinish(void);
extern void glGetFixedv(unsigned,int*);
extern void glGetIntegerv(unsigned,int*);
extern void glClear(unsigned);
extern void glReadPixels(int,int,int,int,unsigned,unsigned,void*);
extern unsigned glGetError(void);
extern int printf(const char*,...);
extern void exit(int);
extern void *dlopen(const char*,int);
extern void *dlsym(void*,const char*);
__attribute__((constructor)) static void test(void) {
 void *display=eglGetDisplay((void*)0);int major=0,minor=0;
 if(!display || !eglInitialize(display,&major,&minor)){printf("EGL_INIT_FAILED error=%x\n",eglGetError());exit(1);}
 printf("EGL_INITIALIZED %d.%d %s\n",major,minor,eglQueryString(display,0x3053));
 int attributes[]={0x3033,1,0x3040,1,0x3024,8,0x3023,8,0x3022,8,0x3038};
 void *config=0;int count=0;
 if(!eglChooseConfig(display,attributes,&config,1,&count)||!count){printf("EGL_CONFIG_FAILED %x\n",eglGetError());exit(2);}
 int surfaceAttributes[]={0x3057,4,0x3056,4,0x3038};
 int contextAttributes[]={0x3098,1,0x3038};
 void *surface=eglCreatePbufferSurface(display,config,surfaceAttributes);
 void *context=eglCreateContext(display,config,0,contextAttributes);
 if(!surface||!context||!eglMakeCurrent(display,surface,surface,context)){printf("EGL_CONTEXT_FAILED %x\n",eglGetError());exit(3);}
 printf("GL_RENDERER %s\nGL_VERSION %s\n",glGetString(0x1f01),glGetString(0x1f02));
 unsigned char pixels[4*4*4]={0};
 glViewport(0,0,4,4);glScissor(0,0,4,4);glEnable(0x0c11);
 glClearColorx(65536,0,0,65536);glClear(0x4000);glFinish();
 glReadPixels(0,0,4,4,0x1908,0x1401,pixels);
 unsigned error=glGetError();
 for(int i=0;i<16;i++)if(error||pixels[4*i]<248||pixels[4*i+1]>7||pixels[4*i+2]>7){printf("GL_PIXEL_FAILED %x %u,%u,%u\n",error,pixels[4*i],pixels[4*i+1],pixels[4*i+2]);exit(4);}
 eglMakeCurrent(display,0,0,0);eglDestroyContext(display,context);eglDestroySurface(display,surface);eglTerminate(display);
 printf("ANDROID_SOFTWARE_EGL_PIXELS_OK\n");exit(0);
}
