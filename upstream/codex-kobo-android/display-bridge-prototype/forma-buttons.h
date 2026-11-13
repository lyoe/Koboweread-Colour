// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef KOBO_FORMA_BUTTONS_H
#define KOBO_FORMA_BUTTONS_H
/* Only completed physical presses produce actions. No auto-repeat or timer taps.
 * Suppress keys already held at startup; a chord never becomes two page turns. */
enum { FORMA_NONE, FORMA_PREVIOUS, FORMA_NEXT, FORMA_REFRESH, FORMA_EXIT };
struct forma_buttons { unsigned pressed; int blocked,chord,power_armed,chord_action; double chord_began,single_began[2]; };
static void forma_buttons_init(struct forma_buttons *s,unsigned held){
 *s=(struct forma_buttons){.pressed=held,.blocked=held!=0};
}
static int forma_button_event(struct forma_buttons *s,int code,int value,double t){
 unsigned bit=code==193?1u:code==194?2u:code==116?4u:0u;
 if(!bit || (value!=0 && value!=1))return FORMA_NONE;
 if(value==1){
  if(s->pressed&bit)return FORMA_NONE;
  s->pressed|=bit;
  if(bit==1 || bit==2)s->single_began[bit==1?0:1]=t;
  if(bit==4){s->blocked=1;s->power_armed=1;return FORMA_NONE;}
  if((s->pressed&3)==3 && !s->chord){s->chord=1;s->chord_began=t;}
  return FORMA_NONE;
 }
 if(!(s->pressed&bit))return FORMA_NONE;
 s->pressed&=~bit;
 if(s->blocked){if(bit==4 && s->power_armed)s->chord_action=FORMA_EXIT;if(!s->pressed){int result=s->chord_action;s->blocked=0;s->chord=0;s->power_armed=0;s->chord_action=0;return result;}return FORMA_NONE;}
 if(s->chord){
  /* Decide at first release, act only after both buttons are released. */
  if(s->chord==1){s->chord=2;s->chord_action=t-s->chord_began>=2.0?FORMA_EXIT:FORMA_NONE;}
  if(!s->pressed){int result=s->chord_action;s->chord=0;s->chord_action=0;return result;}
  return FORMA_NONE;
 }
 if((bit==1 || bit==2) && t-s->single_began[bit==1?0:1]>=1.0)return FORMA_REFRESH;
 return bit==1?FORMA_NEXT:bit==2?FORMA_PREVIOUS:FORMA_NONE;
}
#endif
