#include <string.h>
#include <unistd.h>
extern int access(const char *,int);
struct prop_info;
extern const struct prop_info *__system_property_find(const char *);
extern int __system_property_read(const struct prop_info *,char *,char *);
/* Per-process ART configuration. The private emulator base property disables
 * JIT globally; AndroidRuntime appends that property after command-line flags.
 * Load only in the fixed UID probe, enabled WeRead process, or root-marked system server.
 * All other properties use original Bionic lookup/read. Avoid dlsym here:
 * property reads occur during the dynamic linker's initialization.
 */
int __system_property_get(const char *name,char *value){
 const char *setting=0;uid_t uid=getuid();
 if(uid==1000||uid==10000||uid==10007){
  if(!strcmp(name,"dalvik.vm.usejit"))setting="true";
  /* WeRead saves its JIT hot-method profile so a later offline speed-profile
   * compile can replace the interpret-only odex; other UIDs keep it off. */
  else if(!strcmp(name,"dalvik.vm.usejitprofiles"))setting=uid==10007?"true":"false";
  else if(!strcmp(name,"dalvik.vm.jitinitialsize"))setting="256K";
  else if(!strcmp(name,"dalvik.vm.jitmaxsize"))setting="8M";
 }
 /* The emulator image leaves ART's defaults (512K-2M free, 0.5 target), so
  * WeRead's ~60M live heap is collected after every ~2M allocated. Use
  * typical phone headroom for WeRead only. */
 /* WeRead's memory class (ActivityManager.getMemoryClass(), which Glide uses to
  * size its bitmap memory cache) comes from heapgrowthlimit, 64m in this
  * runtime: about 8.5 MB of cache, so shelf covers are decoded again after
  * every book. 96m unless the session left kobo-memclass-disabled. */
 if(uid==10007 && !strcmp(name,"dalvik.vm.heapgrowthlimit") && access("/data/local/tmp/kobo-memclass-disabled",0))setting="96m";
 /* System services nothing here uses, skipped by SystemServer's own
  * config.disable_* switches (system_server only): network time polling,
  * OTA dexopt, Wi-Fi RTT, the sampling profiler, serial ports, media
  * projection, trust agents. Location, text services and search stay (apps
  * call them and would get a null service). Off: kobo-svcoff-disabled. */
 if(uid==1000 && !strncmp(name,"config.disable_",15) && access("/data/local/tmp/kobo-svcoff-disabled",0)){
  const char *n=name+15;
  if(!strcmp(n,"networktime")||!strcmp(n,"otadexopt")||!strcmp(n,"rtt")||!strcmp(n,"samplingprof")||
     !strcmp(n,"serial")||!strcmp(n,"mediaproj")||!strcmp(n,"trustmanager"))setting="true";
 }
 /* Collector for WeRead: ART's default foreground collector is concurrent
  * mark sweep, whose background marking only competes with the main thread
  * on this single core and adds barrier and bookkeeping work. "MS" is the
  * same mark sweep without the concurrent phase (1.9.7 experiment).
  * Off: kobo-gcms-disabled. */
 if(uid==10007 && !strcmp(name,"dalvik.vm.gctype") && access("/data/local/tmp/kobo-gcms-disabled",0))setting="MS";
 if(uid==10007){
  if(!strcmp(name,"dalvik.vm.heapminfree"))setting="4m";
  /* At most this much free heap is kept after a collection. 16m (the 1.9.8
   * speed setting) keeps up to 16 MB of WeRead's heap idle; 8m keeps half
   * of that (1.9.7 memory work). Off: kobo-heapmax8-disabled (16m). */
  else if(!strcmp(name,"dalvik.vm.heapmaxfree"))setting=access("/data/local/tmp/kobo-heapmax8-disabled",0)?"8m":"16m";
  else if(!strcmp(name,"dalvik.vm.heaptargetutilization"))setting="0.75";
 }
 if(setting){size_t n=strlen(setting);memcpy(value,setting,n+1);return (int)n;}
 const struct prop_info *property=__system_property_find(name);
 if(!property){value[0]=0;return 0;}
 return __system_property_read(property,0,value);
}

/* Skia glyph cache for WeRead. Software rendering draws every glyph through
 * Skia's glyph cache, which this runtime limits to 768 KB: WeRead's large CJK
 * body text was evicted and rasterized again several times a minute. A
 * thread waits until this process is WeRead (UID 10007), then finds the
 * already loaded libskia with dlopen/dlsym (never during linker
 * initialization) and raises the cache to 16 MB / 16384 glyphs. The session
 * scripts mirror fast/skia-disabled and fast/skia-probe into /data/local/tmp:
 *   kobo-skia-disabled  leave Skia's limit alone
 *   kobo-skia-mb        size in MB instead of 16 (1-16, digits; 2026-11-08:
 *                       new pages draw ~0.23 s faster at 16 MB than at
 *                       768 KB, but 16 MB costs ~12-14 MB after a few books)
 *   kobo-skia-probe     log limit and use every 2 s for 4 min (tag KoboSkia) */
typedef long kobo_pthread_t;
extern int pthread_create(kobo_pthread_t *,const void *,void *(*)(void *),void *);
extern int pthread_detach(kobo_pthread_t);
extern void *dlopen(const char *,int);
extern void *dlsym(void *,const char *);
extern int usleep(unsigned);
extern int open(const char *,int,...);
#define KOBO_SKIA_DISABLED "/data/local/tmp/kobo-skia-disabled"
#define KOBO_SKIA_PROBE "/data/local/tmp/kobo-skia-probe"
#define KOBO_SKIA_MB "/data/local/tmp/kobo-skia-mb"
#define KOBO_NEONBLIT_DISABLED "/data/local/tmp/kobo-neonblit-disabled"
#define KOBO_NEONCOLOR_DISABLED "/data/local/tmp/kobo-neoncolor-disabled"

/* NEON glyph blending. This runtime's libskia is built without NEON, so
 * SkOpts::blit_mask_d32_a8 (every anti-aliased glyph mask blended into the
 * 32-bit window) runs Skia's portable code, emulating 16-lane vectors one byte
 * at a time: about half of WeRead's page-draw time. The same arithmetic, 8
 * pixels per step, as Skia's Sk4px (x.approxMulDiv255(y) = (x*y + x) >> 8):
 *   black   d*(1-aa) + aa in alpha
 *   opaque  s*aa + d*(1-aa)
 *   other   s*aa + d*(1-(s*aa).alpha)
 * Installed only after it matches the original byte for byte on test data
 * (switch kobo-neonblit-disabled). */
#include <arm_neon.h>
typedef void (*kobo_blit_mask)(uint32_t *,size_t,const uint8_t *,size_t,uint32_t,int,int);
static kobo_blit_mask original_blit;
static uint32_t (*premultiply)(uint32_t);
static int alpha_lane;
static inline uint8x8_t mul255(uint8x8_t x,uint8x8_t y){return vshrn_n_u16(vaddw_u8(vmull_u8(x,y),x),8);}
static inline uint8_t mul255s(uint8_t x,uint8_t y){return (uint8_t)((x*y+x)>>8);}
static void neon_blit_mask(uint32_t *dst,size_t dstRB,const uint8_t *mask,size_t maskRB,uint32_t color,int w,int h){
 int mode=color==0xFF000000u?0:(color>>24)==0xFF?1:2;
 uint32_t pm=premultiply(color);uint8_t s[4];int k;
 for(k=0;k<4;k++)s[k]=(uint8_t)(pm>>(8*k));
 const uint8x8_t ff=vdup_n_u8(255);
 for(;h>0;h--,dst=(uint32_t *)((char *)dst+dstRB),mask+=maskRB){
  int x=0;
  for(;x+8<=w;x+=8){
   uint8x8x4_t d=vld4_u8((const uint8_t *)(dst+x));uint8x8_t aa=vld1_u8(mask+x);
   if(mode==0){
    uint8x8_t inv=vsub_u8(ff,aa);
    for(k=0;k<4;k++)d.val[k]=mul255(d.val[k],inv);
    d.val[alpha_lane]=vadd_u8(d.val[alpha_lane],aa);
   }else{
    uint8x8_t left[4],inv;
    for(k=0;k<4;k++)left[k]=mul255(vdup_n_u8(s[k]),aa);
    inv=vsub_u8(ff,mode==1?aa:left[alpha_lane]);
    for(k=0;k<4;k++)d.val[k]=vadd_u8(left[k],mul255(d.val[k],inv));
   }
   vst4_u8((uint8_t *)(dst+x),d);
  }
  for(;x<w;x++){
   uint8_t *p=(uint8_t *)(dst+x),aa=mask[x],inv,left[4];
   if(mode==0){inv=255-aa;for(k=0;k<4;k++)p[k]=mul255s(p[k],inv);p[alpha_lane]+=aa;continue;}
   for(k=0;k<4;k++)left[k]=mul255s(s[k],aa);
   inv=255-(mode==1?aa:left[alpha_lane]);
   for(k=0;k<4;k++)p[k]=left[k]+mul255s(p[k],inv);
  }
 }
}
/* Compare with the original on 37x3 pixels (8-wide steps plus a tail, row
 * padding) for black, opaque and translucent colours. */
static int neon_blit_matches(void){
 static uint32_t a[3*40],b[3*40];static uint8_t m[3*41];
 static const uint32_t colors[]={0xFF000000u,0xFF333333u,0xFF8A5A2Bu,0x80405060u,0x20FFFFFFu,0xFFFFFFFFu};
 uint32_t seed=12345;unsigned c,i;
 for(c=0;c<sizeof colors/sizeof *colors;c++){
  for(i=0;i<3*41;i++){seed=seed*1103515245u+12345u;m[i]=(uint8_t)(i%5==0?0:i%7==0?255:seed>>16);}
  for(i=0;i<3*40;i++){seed=seed*1103515245u+12345u;uint8_t al=(uint8_t)(i%3?255:seed>>24),r=(uint8_t)((((seed>>8)&255)*(al+1u))>>8),g=(uint8_t)((((seed>>16)&255)*(al+1u))>>8),bl=(uint8_t)(((seed&255)*(al+1u))>>8);
   uint8_t px[4]={r,g,bl,r};px[alpha_lane]=al;a[i]=b[i]=(uint32_t)px[0]|(uint32_t)px[1]<<8|(uint32_t)px[2]<<16|(uint32_t)px[3]<<24;}
  original_blit(a,40*4,m,41,colors[c],37,3);
  neon_blit_mask(b,40*4,m,41,colors[c],37,3);
  for(i=0;i<3*40;i++)if(a[i]!=b[i])return -(int)(c+1);
 }
 return 1;
}
static void install_neon_blit(void *skia,int (*say)(int,const char *,const char *,...)){
 kobo_blit_mask *slot=(kobo_blit_mask *)dlsym(skia,"_ZN6SkOpts16blit_mask_d32_a8E");
 premultiply=(uint32_t (*)(uint32_t))dlsym(skia,"_Z18SkPreMultiplyColorj");
 if(!slot||!*slot||!premultiply){say(4,"KoboSkia","KOBO_NEON_BLIT missing_symbols");return;}
 uint32_t black=premultiply(0xFF000000u);
 for(alpha_lane=0;alpha_lane<4&&(black>>(8*alpha_lane)&0xFF)!=0xFF;alpha_lane++);
 if(alpha_lane==4){say(4,"KoboSkia","KOBO_NEON_BLIT no_alpha_lane");return;}
 original_blit=*slot;
 int ok=neon_blit_matches();
 if(ok==1)*slot=neon_blit_mask;
 say(4,"KoboSkia","KOBO_NEON_BLIT %s alpha_lane=%d",ok==1?"installed":"mismatch_kept_original",alpha_lane);
 if(ok!=1)say(4,"KoboSkia","KOBO_NEON_BLIT mismatch_color=%d",-ok);
}
/* NEON solid-colour row blending (WeRead 1.9.7). SkBlitRow::Color32 hands
 * every translucent drawColor/drawRect row to SkOpts::blit_row_color32, which
 * this libskia also runs as portable code: about 20 ms of each page turn. Same
 * arithmetic as Skia's Sk4px version, on every byte (all four channels):
 *   invA = 255 - A(color); invA += invA >> 7; kept as 8 bits (Sk16b)
 *   out  = (src*invA + (c << 8) + 128) >> 8, in 16-bit lanes (Sk16h)
 * 16 bytes per step. Installed only after it matches the original byte for
 * byte on test data (switch kobo-neoncolor-disabled). */
typedef void (*kobo_row_color)(uint32_t *,const uint32_t *,int,uint32_t);
static kobo_row_color original_color;
static void neon_row_color(uint32_t *dst,const uint32_t *src,int count,uint32_t color){
 unsigned inv=255-(color>>24);inv+=inv>>7;
 const uint8_t a=(uint8_t)inv;
 uint8_t c[16];int k;for(k=0;k<16;k++)c[k]=(uint8_t)(color>>(8*(k&3)));
 const uint8x16_t cv=vld1q_u8(c);const uint8x8_t av=vdup_n_u8(a);const uint16x8_t round=vdupq_n_u16(128);
 const uint16x8_t add_lo=vaddq_u16(vshll_n_u8(vget_low_u8(cv),8),round),add_hi=vaddq_u16(vshll_n_u8(vget_high_u8(cv),8),round);
 const uint8_t *s8=(const uint8_t *)src;uint8_t *d8=(uint8_t *)dst;int n=count*4,i=0;
 for(;i+16<=n;i+=16){
  uint8x16_t x=vld1q_u8(s8+i);
  uint16x8_t lo=vaddq_u16(vmull_u8(vget_low_u8(x),av),add_lo),hi=vaddq_u16(vmull_u8(vget_high_u8(x),av),add_hi);
  vst1q_u8(d8+i,vcombine_u8(vshrn_n_u16(lo,8),vshrn_n_u16(hi,8)));
 }
 for(;i<n;i++)d8[i]=(uint8_t)((uint16_t)(s8[i]*a+(c[i&3]<<8)+128)>>8);
}
/* Compare with the original on 37 pixels (16-byte steps plus a tail), in place
 * and between buffers, over alphas 0-255 and arbitrary colour bytes. */
static int neon_color_matches(void){
 static uint32_t s[40],a[40],b[40];
 uint32_t seed=54321;unsigned al,i;
 for(al=0;al<256;al++){
  seed=seed*1103515245u+12345u;
  uint32_t color=al<<24|(seed>>8&0xFFFFFFu);
  for(i=0;i<40;i++){seed=seed*1103515245u+12345u;s[i]=seed^(seed<<7);a[i]=b[i]=s[i];}
  original_color(a,s,37,color);neon_row_color(b,s,37,color);
  for(i=0;i<40;i++)if(a[i]!=b[i])return -(int)(al+1);
  original_color(a,a,37,color);neon_row_color(b,b,37,color);
  for(i=0;i<40;i++)if(a[i]!=b[i])return -(int)(al+1)-1000;
 }
 return 1;
}
static void install_neon_color(void *skia,int (*say)(int,const char *,const char *,...)){
 kobo_row_color *slot=(kobo_row_color *)dlsym(skia,"_ZN6SkOpts16blit_row_color32E");
 if(!slot||!*slot){say(4,"KoboSkia","KOBO_NEON_COLOR missing_symbols");return;}
 original_color=*slot;
 int ok=neon_color_matches();
 if(ok==1)*slot=neon_row_color;
 say(4,"KoboSkia","KOBO_NEON_COLOR %s",ok==1?"installed":"mismatch_kept_original");
 if(ok!=1)say(4,"KoboSkia","KOBO_NEON_COLOR mismatch_case=%d",-ok);
}
static int read_small(const char *path,char *buf,int size){
 int fd=open(path,0);if(fd<0)return -1;long n=(long)read(fd,buf,(size_t)size-1);close(fd);if(n<0)n=0;buf[n]=0;return (int)n;
}
static int is_weread(void){
 char cmd[64];if(getuid()!=10007||read_small("/proc/self/cmdline",cmd,sizeof cmd)<0)return 0;
 /* Byte loop: a constant-string strcmp is turned into bcmp, absent from bionic. */
 const char *want="com.tencent.weread.eink";int k=0;
 while(want[k]&&cmd[k]==want[k])k++;
 return !want[k]&&!cmd[k];
}
static void *skia_thread(void *unused){
 (void)unused;int i;
 for(i=0;i<240 && !is_weread();i++)usleep(500000);
 if(i==240)return 0;
 void *skia=dlopen("libskia.so",0),*log=dlopen("liblog.so",0);
 int (*say)(int,const char *,const char *,...)=log?(int (*)(int,const char *,const char *,...))dlsym(log,"__android_log_print"):0;
 unsigned (*limit)(void)=skia?(unsigned (*)(void))dlsym(skia,"_ZN10SkGraphics17GetFontCacheLimitEv"):0;
 unsigned (*used)(void)=skia?(unsigned (*)(void))dlsym(skia,"_ZN10SkGraphics16GetFontCacheUsedEv"):0;
 int (*count_limit)(void)=skia?(int (*)(void))dlsym(skia,"_ZN10SkGraphics22GetFontCacheCountLimitEv"):0;
 int (*count_used)(void)=skia?(int (*)(void))dlsym(skia,"_ZN10SkGraphics21GetFontCacheCountUsedEv"):0;
 unsigned (*set_limit)(unsigned)=skia?(unsigned (*)(unsigned))dlsym(skia,"_ZN10SkGraphics17SetFontCacheLimitEj"):0;
 int (*set_count)(int)=skia?(int (*)(int))dlsym(skia,"_ZN10SkGraphics22SetFontCacheCountLimitEi"):0;
 if(skia&&say&&access(KOBO_NEONBLIT_DISABLED,0))install_neon_blit(skia,say);
 if(skia&&say&&access(KOBO_NEONCOLOR_DISABLED,0))install_neon_color(skia,say);
 if(!access(KOBO_SKIA_DISABLED,0)&&access(KOBO_SKIA_PROBE,0))return 0;
 if(!say||!limit||!used||!count_limit||!count_used||!set_limit||!set_count)return 0;
 int mb=0;
 if(access(KOBO_SKIA_DISABLED,0)){
  char v[8];mb=16;
  if(read_small(KOBO_SKIA_MB,v,sizeof v)>0){int n=0,k=0;while(v[k]>='0'&&v[k]<='9'&&n<100)n=n*10+(v[k++]-'0');if(k>0&&n>=1&&n<=16)mb=n;}
  set_limit((unsigned)mb<<20);set_count(mb*1024);
 }
 say(4,"KoboSkia","KOBO_SKIA_FONT_CACHE set_mb=%d limit=%u count_limit=%d",mb,limit(),count_limit());
 if(access(KOBO_SKIA_PROBE,0))return 0;
 for(i=0;i<120;i++){
  say(4,"KoboSkia","KOBO_SKIA_FONT_CACHE_USE used=%u limit=%u count_used=%d count_limit=%d",used(),limit(),count_used(),count_limit());
  usleep(2000000);
 }
 return 0;
}
/* Bundled reader font without inflating it. WeRead 1.9.7 loads
 * assets/fonts/SourceHanSerifCN-Medium_FDK_motify_strip.otf (11,373,656
 * bytes, stored deflated in the APK) with Typeface.createFromAsset: Android
 * inflates the whole file into an 11 MB anonymous buffer at the first book
 * open and keeps it for the process lifetime (0.37 s of CPU, 11 MB that can
 * never be dropped). The session extracts the identical file once into
 * /data/local/tmp/kobo-fonts/. Once WeRead's Application exists, this
 * thread creates the Typeface from that file (mmap: clean pages the kernel
 * can drop and read back) and puts it in Typeface.sDynamicTypefaceCache
 * under the key createFromAsset looks up first, holding the cache's lock,
 * following the key while it changes (see font_map). Same font bytes, same
 * Typeface construction as createFromAsset. Log tag KoboFont.
 *   kobo-fontmap-disabled  leave the asset path alone */
#include <jni.h>
#define KOBO_FONTMAP_DISABLED "/data/local/tmp/kobo-fontmap-disabled"
#define KOBO_FONT_ASSET "fonts/SourceHanSerifCN-Medium_FDK_motify_strip.otf"
#define KOBO_FONT_FILE "/data/local/tmp/kobo-fonts/SourceHanSerifCN-Medium_FDK_motify_strip.otf"
#define KOBO_FONT_BYTES 11373656L
/* Bionic (32-bit) lseek takes a 32-bit offset; musl's header says 64. */
extern long kobo_lseek(int,long,int) __asm__("lseek");
static void font_say(int (*say)(int,const char *,const char *,...),JNIEnv *e,const char *what,jstring key){
 const char *k=key?(*e)->GetStringUTFChars(e,key,0):0;
 say(4,"KoboFont","KOBO_FONT_MAP %s key=%s",what,k?k:"-");
 if(k)(*e)->ReleaseStringUTFChars(e,key,k);
}
static const char *font_map(JNIEnv *e,jobject app,int (*say)(int,const char *,const char *,...)){
 jclass ctx=(*e)->FindClass(e,"android/content/Context");
 jmethodID assets=ctx?(*e)->GetMethodID(e,ctx,"getAssets","()Landroid/content/res/AssetManager;"):0;if(!assets)return "no_get_assets";
 jobject mgr=(*e)->CallObjectMethod(e,app,assets);if((*e)->ExceptionCheck(e)||!mgr)return "get_assets_failed";
 jclass tf=(*e)->FindClass(e,"android/graphics/Typeface");if(!tf)return "no_typeface";
 jfieldID cf=(*e)->GetStaticFieldID(e,tf,"sDynamicTypefaceCache","Landroid/util/LruCache;");if(!cf)return "no_dynamic_cache";
 jmethodID uid=(*e)->GetStaticMethodID(e,tf,"createAssetUid","(Landroid/content/res/AssetManager;Ljava/lang/String;)Ljava/lang/String;");if(!uid)return "no_asset_uid";
 jmethodID from_file=(*e)->GetStaticMethodID(e,tf,"createFromFile","(Ljava/lang/String;)Landroid/graphics/Typeface;");if(!from_file)return "no_from_file";
 jclass lru=(*e)->FindClass(e,"android/util/LruCache");
 jmethodID get=lru?(*e)->GetMethodID(e,lru,"get","(Ljava/lang/Object;)Ljava/lang/Object;"):0;
 jmethodID put=lru?(*e)->GetMethodID(e,lru,"put","(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;"):0;if(!get||!put)return "no_lru_methods";
 jobject cache=(*e)->GetStaticObjectField(e,tf,cf);if(!cache)return "cache_null";
 jstring asset=(*e)->NewStringUTF(e,KOBO_FONT_ASSET),file=(*e)->NewStringUTF(e,KOBO_FONT_FILE);if(!asset||!file)return "no_strings";
 jobject face=(*e)->CallStaticObjectMethod(e,tf,from_file,file);
 if((*e)->ExceptionCheck(e)||!face)return "create_from_file_failed";
 /* The key lists the packages whose resources the AssetManager holds; it
  * changes when WebView's are added (WeRead loads WebView on its home page).
  * Every 2 s for 30 min, put the mapped face under the current key when
  * the cache has nothing there (also after an LRU eviction); stop when
  * another face is found there (the asset was loaded the usual way). */
 for(int tick=0;tick<900;tick++,usleep(2000000)){
  jstring key=(jstring)(*e)->CallStaticObjectMethod(e,tf,uid,mgr,asset);
  if((*e)->ExceptionCheck(e)||!key)return "asset_uid_failed";
  if((*e)->MonitorEnter(e,cache))return "lock_failed";
  jobject old=(*e)->CallObjectMethod(e,cache,get,key);
  const char *result=0;
  if((*e)->ExceptionCheck(e))result="cache_get_threw";
  else if(!old){(*e)->CallObjectMethod(e,cache,put,key,face);if((*e)->ExceptionCheck(e))result="cache_put_threw";else font_say(say,e,"mapped",key);}
  else if(!(*e)->IsSameObject(e,old,face)){result="loaded_from_asset";font_say(say,e,result,key);}
  (*e)->MonitorExit(e,cache);
  if(old)(*e)->DeleteLocalRef(e,old);
  (*e)->DeleteLocalRef(e,key);
  if(result)return result;
 }
 return "watch_ended";
}
/* WebView pre-warm. WeReadFragmentActivity.onCreate calls
 * WRWebViewPool.preInit, which creates a WRWebView, loads about:blank and
 * parks it in the pool. That loads and starts Chromium (libwebviewchromium,
 * ~27 MB of pages, its native-library load alone ~0.5 s) on the main thread
 * while the home page is being built. The pool's only user is the
 * external-document header of a story-feed detail page, and acquireWebView
 * creates a WebView itself when the pool is empty. Setting the pool's
 * isInited flag first (through WeRead's class loader, once its Application
 * exists, before the home activity) makes preInit return at once; WebView
 * then loads when a page that needs one opens.
 *   kobo-webviewpool-disabled  leave the pre-warm alone */
#define KOBO_WEBVIEWPOOL_DISABLED "/data/local/tmp/kobo-webviewpool-disabled"
static const char *defer_webview_pool(JNIEnv *e,jobject app){
 jclass ctx=(*e)->FindClass(e,"android/content/Context");
 jmethodID loader=ctx?(*e)->GetMethodID(e,ctx,"getClassLoader","()Ljava/lang/ClassLoader;"):0;if(!loader)return "no_get_class_loader";
 jobject cl=(*e)->CallObjectMethod(e,app,loader);if((*e)->ExceptionCheck(e)||!cl)return "class_loader_failed";
 jclass clc=(*e)->FindClass(e,"java/lang/ClassLoader");
 jmethodID load=clc?(*e)->GetMethodID(e,clc,"loadClass","(Ljava/lang/String;)Ljava/lang/Class;"):0;if(!load)return "no_load_class";
 jstring name=(*e)->NewStringUTF(e,"com.tencent.weread.WRWebViewPool");if(!name)return "no_string";
 jclass pool=(jclass)(*e)->CallObjectMethod(e,cl,load,name);if((*e)->ExceptionCheck(e)||!pool)return "pool_class_missing";
 jfieldID inited=(*e)->GetStaticFieldID(e,pool,"isInited","Z");if(!inited)return "no_is_inited";
 if((*e)->GetStaticBooleanField(e,pool,inited))return "too_late";
 (*e)->SetStaticBooleanField(e,pool,inited,JNI_TRUE);
 return "deferred";
}
/* Attaching to the VM before the runtime has finished starting aborts the
 * process ("Check failed: main_thread_group_ != nullptr"): JNI_GetCreatedJavaVMs
 * already returns the VM while Runtime::Start is still running. Its last step
 * starts the daemon threads (HeapTaskDaemon, FinalizerDaemon, ...), after the
 * thread groups exist, so wait until one of them is listed. */
#include <dirent.h>
static int runtime_started(void){
 DIR *d=opendir("/proc/self/task");if(!d)return 0;
 struct dirent *t;int found=0;char path[64],comm[32];
 while(!found&&(t=readdir(d))){
  if(t->d_name[0]<'0'||t->d_name[0]>'9')continue;
  int k=0;const char *a="/proc/self/task/";while(*a)path[k++]=*a++;
  for(const char *n=t->d_name;*n&&k<48;)path[k++]=*n++;
  a="/comm";while(*a)path[k++]=*a++;path[k]=0;
  if(read_small(path,comm,sizeof comm)<=0)continue;
  const char *want[2]={"HeapTaskDaemon","FinalizerDaemon"};
  for(int w=0;w<2&&!found;w++){int j=0;while(want[w][j]&&comm[j]==want[w][j])j++;found=!want[w][j];}
 }
 closedir(d);return found;
}
/* One thread for both: attaches to WeRead's VM once its Application exists. */
static void *java_thread(void *unused){
 (void)unused;int i;
 for(i=0;i<240 && !is_weread();i++)usleep(500000);
 if(i==240)return 0;
 for(i=0;i<1200 && !runtime_started();i++)usleep(50000);
 void *log=dlopen("liblog.so",0),*art=dlopen("libart.so",0);
 int (*say)(int,const char *,const char *,...)=log?(int (*)(int,const char *,const char *,...))dlsym(log,"__android_log_print"):0;
 if(!say)return 0;
 if(i==1200){say(4,"KoboFont","KOBO_JAVA_THREAD skipped=runtime_not_started");return 0;}
 jint (*vms)(JavaVM **,jsize,jsize *)=art?(jint (*)(JavaVM **,jsize,jsize *))dlsym(art,"JNI_GetCreatedJavaVMs"):0;
 JavaVM *vm=0;jsize n=0;JNIEnv *e=0;
 if(!vms||vms(&vm,1,&n)!=JNI_OK||n<1||(*vm)->AttachCurrentThreadAsDaemon(vm,(void **)&e,0)!=JNI_OK){say(4,"KoboFont","KOBO_JAVA_THREAD skipped=jni");return 0;}
 jclass at=(*e)->FindClass(e,"android/app/ActivityThread");
 jmethodID current=at?(*e)->GetStaticMethodID(e,at,"currentApplication","()Landroid/app/Application;"):0;
 jobject app=0;
 /* Poll every 20 ms: the pool flag must be set before the home activity. */
 for(i=0;current && i<3000 && !app;i++){app=(*e)->CallStaticObjectMethod(e,at,current);if((*e)->ExceptionCheck(e))break;if(!app)usleep(20000);}
 if((*e)->ExceptionCheck(e))(*e)->ExceptionClear(e);
 if(!app){say(4,"KoboFont","KOBO_JAVA_THREAD skipped=no_application");(*vm)->DetachCurrentThread(vm);return 0;}
 if(access(KOBO_WEBVIEWPOOL_DISABLED,0)){
  const char *pool=defer_webview_pool(e,app);
  if((*e)->ExceptionCheck(e))(*e)->ExceptionClear(e);
  say(4,"KoboFont","KOBO_WEBVIEW_POOL result=%s",pool);
 }
 if(access(KOBO_FONTMAP_DISABLED,0)){
  int fd=open(KOBO_FONT_FILE,0);long size=fd<0?-1:kobo_lseek(fd,0,2);if(fd>=0)close(fd);
  if(size!=KOBO_FONT_BYTES)say(4,"KoboFont","KOBO_FONT_MAP skipped=file size=%ld",size);
  else{
   const char *result=font_map(e,app,say);
   if((*e)->ExceptionCheck(e))(*e)->ExceptionClear(e);
   say(4,"KoboFont","KOBO_FONT_MAP result=%s",result);
  }
 }
 (*vm)->DetachCurrentThread(vm);
 return 0;
}
/* WeRead's own log files without fsync. moai.log.FileHandler.flush ends in
 * FileChannel.force for every handler (wrlog, perf, report, oss, ... in the
 * main process and :seg), and the "autoflush" threads flush them every few
 * seconds. Each force is a synchronous write through the ext4 image on the
 * vfat partition: going back to the shelf soon after opening a book waited
 * 2-4 s on them (submit_bio_wait), and the main thread stalled behind the
 * I/O. For WeRead (UID 10007) only, an fsync/fdatasync of a file in a log
 * directory or named wr-*.log / wr.log.* returns success without syncing;
 * the data still reaches the file through the page cache (lost only if
 * the device loses power within the writeback delay). Everything else
 * (SQLite, LevelDB, preferences) is synced as before. The first 48
 * distinct synced paths are logged with the decision (tag KoboSync).
 * Off: kobo-logsync-disabled. */
#define KOBO_LOGSYNC_DISABLED "/data/local/tmp/kobo-logsync-disabled"
extern long syscall(long,...);
extern int snprintf(char *,size_t,const char *,...);
static int logsync_state=-1;
static unsigned logsync_seen[48];static int logsync_count;
static int has(const char *s,const char *w){
 for(;*s;s++){int k=0;while(w[k]&&s[k]==w[k])k++;if(!w[k])return 1;}
 return 0;
}
static int skip_sync(int fd){
 if(getuid()!=10007)return 0;
 if(logsync_state<0)logsync_state=access(KOBO_LOGSYNC_DISABLED,0)?1:0;
 char p[40],l[256];snprintf(p,sizeof p,"/proc/self/fd/%d",fd);
 long n=(long)readlink(p,l,sizeof l-1);if(n<=0)return 0;l[n]=0;
 const char *base=l;for(const char *c=l;*c;c++)if(*c=='/')base=c+1;
 int is_log=has(l,"/log/")||has(l,"/logs/")||(base[0]=='w'&&base[1]=='r'&&(base[2]=='-'||has(base,"wr.log.")));
 int skip=logsync_state==1&&is_log;
 unsigned h=2166136261u;for(const char *c=l;*c;c++)h=(h^(unsigned char)*c)*16777619u;
 int known=0;for(int i=0;i<logsync_count;i++)if(logsync_seen[i]==h){known=1;break;}
 if(!known&&logsync_count<48){
  logsync_seen[logsync_count++]=h;
  void *log=dlopen("liblog.so",0);
  int (*say)(int,const char *,const char *,...)=log?(int (*)(int,const char *,const char *,...))dlsym(log,"__android_log_print"):0;
  if(say)say(4,"KoboSync","KOBO_LOG_SYNC %s path=%s",skip?"skipped":"synced",l);
 }
 return skip;
}
int fsync(int fd){return skip_sync(fd)?0:(int)syscall(118,fd);}
int fdatasync(int fd){return skip_sync(fd)?0:(int)syscall(148,fd);}
__attribute__((constructor)) static void skia_font_cache(void){
 /* Only WeRead's process (already UID 10007 when it loads this library). */
 if(getuid()!=10007)return;
 kobo_pthread_t t;
 if((access(KOBO_FONTMAP_DISABLED,0)||access(KOBO_WEBVIEWPOOL_DISABLED,0))&&!pthread_create(&t,0,java_thread,0))pthread_detach(t);
 if(!access(KOBO_SKIA_DISABLED,0)&&access(KOBO_SKIA_PROBE,0)&&!access(KOBO_NEONBLIT_DISABLED,0)&&!access(KOBO_NEONCOLOR_DISABLED,0))return;
 if(!pthread_create(&t,0,skia_thread,0))pthread_detach(t);
}
