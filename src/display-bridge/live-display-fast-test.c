/* Device self-test for live-display-fast.c: NEON luma + LUT + hash must equal
 * the original scalar code byte for byte; changed_rect must cover exactly the
 * differing pixels (horizontally widened to multiples of 8). */
#define main live_display_main
#include "live-display-fast.c"
#undef main
#include <stdlib.h>
static uint32_t rnd=12345;static unsigned char r8(void){rnd=rnd*1103515245u+12345u;return (unsigned char)(rnd>>16);}
int main(void){
 unsigned char *rgba=malloc(PIXELS*4),*g1=malloc(PIXELS),*g2=malloc(PIXELS),lut[256];
 int fails=0;
 for(int mode=0;mode<2;mode++){
  display_lut(lut,mode);
  for(int round=0;round<3;round++){
   for(size_t i=0;i<(size_t)PIXELS*4;i++)rgba[i]=round==0?r8():round==1?(i%3?255:0):(unsigned char)(i*7);
   rgba[0]=rgba[1]=rgba[2]=255;rgba[4]=rgba[5]=rgba[6]=0;
   double t=now();uint32_t h2=convert(rgba,g2,lut);double fast=now()-t;
   t=now();uint32_t h1=2166136261u;
   for(size_t p=0;p<PIXELS;p++){g1[p]=lut[(77u*rgba[p*4]+150u*rgba[p*4+1]+29u*rgba[p*4+2]+128u)>>8];h1=(h1^g1[p])*16777619u;}
   double slow=now()-t;
   int same=!memcmp(g1,g2,PIXELS)&&h1==h2;fails+=!same;
   printf("CONVERT mode=%d round=%d equal=%d scalar_ms=%.1f fast_ms=%.1f\n",mode,round,same,slow*1000,fast*1000);
  }
 }
 for(int k=0;k<200;k++){
  memcpy(g2,g1,PIXELS);
  int n=k%5;int bx0=WIDTH,by0=HEIGHT,bx1=-1,by1=-1;
  for(int j=0;j<n;j++){int x=r8()*WIDTH/256,y=(r8()<<8|r8())%HEIGHT;g2[(size_t)y*WIDTH+x]^=1;
   if(x<bx0)bx0=x;if(x>bx1)bx1=x;if(y<by0)by0=y;if(y>by1)by1=y;}
  int x,y,w,h,c=changed_rect(g2,g1,&x,&y,&w,&h);
  /* identical pixels flipped twice cancel out: recompute brute force */
  bx0=WIDTH;by0=HEIGHT;bx1=-1;by1=-1;
  for(int yy=0;yy<HEIGHT;yy++)for(int xx=0;xx<WIDTH;xx++)if(g1[(size_t)yy*WIDTH+xx]!=g2[(size_t)yy*WIDTH+xx]){if(xx<bx0)bx0=xx;if(xx>bx1)bx1=xx;if(yy<by0)by0=yy;if(yy>by1)by1=yy;}
  int expect=bx1>=0;
  int ok=c==expect && (!c || (y==by0 && y+h-1==by1 && x==(bx0&~7) && x<=bx0 && x+w-1>=bx1 && x%8==0 && x+w<=WIDTH));
  if(!ok){fails++;printf("RECT_FAIL k=%d c=%d %d,%d %dx%d expect %d %d-%d %d-%d\n",k,c,x,y,w,h,expect,bx0,bx1,by0,by1);}
 }
 printf("SELFTEST %s fails=%d\n",fails?"FAILED":"PASSED",fails);return fails!=0;
}
