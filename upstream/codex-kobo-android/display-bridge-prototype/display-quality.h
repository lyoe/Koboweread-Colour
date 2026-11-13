// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef KOBO_DISPLAY_QUALITY_H
#define KOBO_DISPLAY_QUALITY_H
/* Mild grayscale emphasis: mix identity with a quadratic curve (3/8).
 * Preserve black/white endpoints and graded antialiasing, never threshold to B&W.
 * Applies to the displayed grayscale image, not the APK's font renderer. */
static unsigned char text_gray(unsigned v){
 return (unsigned char)((3u*v*v+5u*v*255u+1020u)/2040u);
}
static void display_lut(unsigned char lut[256],int text_emphasis){
 for(unsigned i=0;i<256;i++)lut[i]=text_emphasis?text_gray(i):(unsigned char)i;
}
#endif
