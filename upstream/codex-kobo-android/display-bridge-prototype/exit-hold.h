// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef KOBO_EXIT_HOLD_H
#define KOBO_EXIT_HOLD_H
/* Buffer a real single-finger contact in the top-left 60x60 virtual pixels.
 * Short taps are replayed as the same physical gesture; exit occurs on release
 * after a stationary two-second hold. No gesture is synthesized to request exit. */
#ifndef KOBO_EXIT_HOLD_SCALE
#define KOBO_EXIT_HOLD_SCALE 1
#endif
struct exit_hold { int state,x,y;double began; };
enum { HOLD_NORMAL=0,HOLD_BUFFER=1,HOLD_FLUSH=2,HOLD_EXIT=3 };
static void exit_hold_cancel(struct exit_hold *h){h->state=0;}
static void exit_hold_tick(struct exit_hold *h,double t){if(h->state==1 && t-h->began>=2.0)h->state=2;}
static int exit_hold_contact(struct exit_hold *h,int x,int y,double t){
 if(!h->state){if(x<0||y<0||x>=60*KOBO_EXIT_HOLD_SCALE||y>=60*KOBO_EXIT_HOLD_SCALE)return HOLD_NORMAL;h->state=1;h->x=x;h->y=y;h->began=t;return HOLD_BUFFER;}
 if(h->state==2)return HOLD_BUFFER;
 if(x<0||y<0||x>=60*KOBO_EXIT_HOLD_SCALE||y>=60*KOBO_EXIT_HOLD_SCALE||x-h->x>18*KOBO_EXIT_HOLD_SCALE||h->x-x>18*KOBO_EXIT_HOLD_SCALE||y-h->y>18*KOBO_EXIT_HOLD_SCALE||h->y-y>18*KOBO_EXIT_HOLD_SCALE){h->state=0;return HOLD_FLUSH;}
 exit_hold_tick(h,t);return HOLD_BUFFER;
}
static int exit_hold_release(struct exit_hold *h,double t){
 if(!h->state)return HOLD_NORMAL;exit_hold_tick(h,t);int result=h->state==2?HOLD_EXIT:HOLD_FLUSH;h->state=0;return result;
}
#endif
