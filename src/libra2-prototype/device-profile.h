// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef KOBO_ANDROID_DEVICE_PROFILE_H
#define KOBO_ANDROID_DEVICE_PROFILE_H
#ifdef KOBO_TARGET_LIBRA2
#ifdef KOBO_LIBRA2_NATIVE
#define KOBO_FB_WIDTH 1264
#define KOBO_FB_HEIGHT 1680
#define KOBO_FB_SCALE 1
#define KOBO_EXIT_HOLD_SCALE 2
#define KOBO_FB_PIXCLOCK_30 15697
#else
#define KOBO_FB_WIDTH 632
#define KOBO_FB_HEIGHT 840
#define KOBO_FB_SCALE 2
#define KOBO_FB_PIXCLOCK_30 62789
#endif
#define KOBO_FB_WIDTH_MM 100
#define KOBO_FB_HEIGHT_MM 133
#define KOBO_TOUCH_NAME "Elan Touchscreen"
#define KOBO_TOUCH_MAX_X 1680
#define KOBO_TOUCH_MAX_Y 1264
#else
#define KOBO_FB_SCALE 2
#define KOBO_FB_WIDTH 720
#define KOBO_FB_HEIGHT 960
#define KOBO_FB_WIDTH_MM 114
#define KOBO_FB_HEIGHT_MM 152
#define KOBO_FB_PIXCLOCK_30 48225
#define KOBO_TOUCH_NAME "cyttsp5_mt"
#define KOBO_TOUCH_MAX_X 1919
#define KOBO_TOUCH_MAX_Y 1439
#endif
static inline int kobo_touch_map(int rx,int ry,int *x,int *y) {
 if(rx<0 || rx>KOBO_TOUCH_MAX_X || ry<0 || ry>KOBO_TOUCH_MAX_Y)return -1;
#ifdef KOBO_TARGET_LIBRA2
 *x=ry/KOBO_FB_SCALE;*y=rx/KOBO_FB_SCALE;
#else
 *x=(KOBO_TOUCH_MAX_Y-ry)/2;*y=rx/2;
#endif
 if(*x>=KOBO_FB_WIDTH)*x=KOBO_FB_WIDTH-1;
 if(*y>=KOBO_FB_HEIGHT)*y=KOBO_FB_HEIGHT-1;
 return 0;
}
#endif
