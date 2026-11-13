// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef KOBO_EDGE_LIGHT_H
#define KOBO_EDGE_LIGHT_H
/* Frontlight by edge swipe (resident), as on the stock firmware and KOReader:
 * a single-finger contact that starts within EDGE_WIDTH of the edge on the
 * page-key side (brightness) or the opposite edge (warmth) is held back from
 * Android until it is classified. The sides stay with the hardware: the keys
 * are on the right of the upright (Forma/Libra) screen, so upside down the
 * brightness edge is the user's left.
 *  - mostly vertical movement of EDGE_SLOP or more: a light gesture, never sent
 *    to Android; the level follows the finger (EDGE_SPAN pixels = full range);
 *  - horizontal movement, a release, or a still hold of EDGE_HOLD seconds:
 *    replayed to Android from where it began (a tap arrives on release).
 * The exit corner keeps its own gesture. The light stays as set when Android
 * is left. Off: resident/edgelight-disabled; warmth only:
 * resident/edgewarmth-disabled. Paths follow KOReader's Libra 2 discovery. */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#define EDGE_WIDTH 76
#define EDGE_SLOP 40
#define EDGE_SPAN 1200
#define EDGE_HOLD 0.4
#define EDGE_WRITE_INTERVAL 0.05
#define EDGE_SWITCHES "/mnt/onboard/.adds/kobo-android-lab/resident-v197/"
enum { EDGE_IDLE=0,EDGE_PENDING=1,EDGE_ACTIVE=2 };
enum { EDGE_BUFFER=1,EDGE_FLUSH=2 };
struct edge_light {
 const char *path[2];int max[2],inverted[2]; /* 0 brightness (key side), 1 warmth */
 int state,side,x0,y0,x,y,ymin,ymax,start,level,written;double began,last_write;
 unsigned gestures,writes,flushes;
};
static int edge_read(const char *path){
 char text[16]={0};int fd=open(path,O_RDONLY|O_CLOEXEC);if(fd<0)return -1;
 ssize_t n=read(fd,text,sizeof(text)-1);close(fd);return n>0?atoi(text):-1;
}
static void edge_init(struct edge_light *e,int enabled){
 static const char *const mixers[]={"/sys/class/leds/aw99703-bl_FL1/color","/sys/class/backlight/lm3630a_led/color","/sys/class/backlight/tlc5947_bl/color"};
 e->state=EDGE_IDLE;
 if(!enabled || !access(EDGE_SWITCHES "edgelight-disabled",F_OK))return;
 if(!access("/sys/class/backlight/mxc_msp430.0/brightness",W_OK)){e->path[0]="/sys/class/backlight/mxc_msp430.0/brightness";e->max[0]=100;}
 if(access(EDGE_SWITCHES "edgewarmth-disabled",F_OK))
  for(unsigned i=0;i<3;i++)if(!access(mixers[i],W_OK)){e->path[1]=mixers[i];e->max[1]=10;e->inverted[1]=1;break;}
}
/* User level (more light, warmer = higher) from the device value and back. */
static int edge_level(const struct edge_light *e,int side){
 int v=edge_read(e->path[side]);if(v<0)return -1;if(v>e->max[side])v=e->max[side];
 return e->inverted[side]?e->max[side]-v:v;
}
static void edge_write(struct edge_light *e,double t){
 char text[16];int v=e->inverted[e->side]?e->max[e->side]-e->level:e->level,n=snprintf(text,sizeof(text),"%d",v);
 int fd=open(e->path[e->side],O_WRONLY|O_CLOEXEC);
 if(fd<0 || write(fd,text,(size_t)n)!=n)fprintf(stderr,"HOST_EDGE_LIGHT_WRITE_FAILED path=%s value=%d\n",e->path[e->side],v);
 if(fd>=0)close(fd);e->written=e->level;e->last_write=t;e->writes++;
}
/* A new contact at (x,y), Android screen pixels: starts buffering if it is on
 * a light edge. keys_left: the page keys are on the screen's left (the device
 * is upside down). The exit corner (top left, corner pixels) is no edge. */
static int edge_begin(struct edge_light *e,int x,int y,int width,int corner,int keys_left,double t){
 int left=x<EDGE_WIDTH && !(y<corner),right=x>=width-EDGE_WIDTH;
 if(!left && !right)return 0;
 int side=left==keys_left?0:1;
 if(!e->path[side])return 0;
 e->state=EDGE_PENDING;e->side=side;e->x0=e->x=x;e->y0=e->y=e->ymin=e->ymax=y;e->began=t;return 1;
}
static void edge_follow(struct edge_light *e,double t){
 int level=e->start+(e->y0-e->y)*e->max[e->side]/EDGE_SPAN;
 if(level<0)level=0;if(level>e->max[e->side])level=e->max[e->side];
 e->level=level;if(level!=e->written && t-e->last_write>=EDGE_WRITE_INTERVAL)edge_write(e,t);
}
static int edge_move(struct edge_light *e,int x,int y,double t){
 e->x=x;e->y=y;if(y<e->ymin)e->ymin=y;if(y>e->ymax)e->ymax=y;
 if(e->state==EDGE_PENDING){
  int dx=abs(x-e->x0),dy=abs(y-e->y0);
  if(dy>=EDGE_SLOP && dy>=2*dx){
   e->start=edge_level(e,e->side);if(e->start<0){e->state=EDGE_IDLE;e->flushes++;return EDGE_FLUSH;}
   e->state=EDGE_ACTIVE;e->written=e->start;e->last_write=0;
  }else if(dx>=EDGE_SLOP || t-e->began>=EDGE_HOLD){e->state=EDGE_IDLE;e->flushes++;return EDGE_FLUSH;}
  else return EDGE_BUFFER;
 }
 edge_follow(e,t);return EDGE_BUFFER;
}
/* Still finger: no new frames arrive, so the hold is also checked by time. */
static int edge_tick(struct edge_light *e,double t){
 if(e->state!=EDGE_PENDING || t-e->began<EDGE_HOLD)return 0;
 e->state=EDGE_IDLE;e->flushes++;return EDGE_FLUSH;
}
/* Release or cancel: a pending contact is replayed (release) or dropped
 * (cancel); a light gesture writes its final level. */
static int edge_end(struct edge_light *e,double t){
 int state=e->state;e->state=EDGE_IDLE;
 if(state==EDGE_PENDING){e->flushes++;return EDGE_FLUSH;}
 if(state==EDGE_ACTIVE){
  if(e->level!=e->written)edge_write(e,t);e->gestures++;
  // Where the finger went (Android pixels, y down): diagnoses swipes that
  // change nothing, e.g. a downward swipe with the light already at 0.
  printf("HOST_EDGE_LIGHT kind=%s from=%d to=%d writes=%u x=%d y=%d->%d ymin=%d ymax=%d ms=%d\n",e->side?"warmth":"brightness",e->start,e->level,e->writes,e->x0,e->y0,e->y,e->ymin,e->ymax,(int)((t-e->began)*1000));
 }
 return 0;
}
#endif
