#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include "forma-buttons-tested76.h"
#include "refresh-policy-tested76.h"
int main(void){
 struct forma_buttons s;forma_buttons_init(&s,0);
 assert(!forma_button_event(&s,193,0,0));
 assert(!forma_button_event(&s,193,1,1));
 for(int i=0;i<30;i++)assert(!forma_button_event(&s,193,2,1.1+i*.1));
 assert(forma_button_event(&s,193,0,5)==FORMA_PREVIOUS);
 assert(!forma_button_event(&s,194,1,6));assert(forma_button_event(&s,194,0,6.1)==FORMA_NEXT);
 for(int order=0;order<2;order++){
  int first=order?194:193,second=order?193:194;
  assert(!forma_button_event(&s,first,1,10));assert(!forma_button_event(&s,second,1,10.1));
  assert(!forma_button_event(&s,first,0,10.5));assert(forma_button_event(&s,second,0,10.6)==FORMA_REFRESH);
  assert(!forma_button_event(&s,first,1,20));assert(!forma_button_event(&s,second,1,20.1));
  assert(!forma_button_event(&s,first,0,22.1));assert(forma_button_event(&s,second,0,22.2)==FORMA_EXIT);
 }
 // Holding just one key for a long time remains a single page turn.
 assert(!forma_button_event(&s,194,1,30));assert(forma_button_event(&s,194,0,40)==FORMA_NEXT);
 // A nearly-long chord must refresh, never quit.
 assert(!forma_button_event(&s,193,1,50));assert(!forma_button_event(&s,194,1,51));
 assert(!forma_button_event(&s,194,0,52.99));assert(forma_button_event(&s,193,0,55)==FORMA_REFRESH);
 // Startup-held input, including power, is discarded until released.
 for(unsigned held=1;held<=7;held++){
  forma_buttons_init(&s,held);
  if(held&1)assert(!forma_button_event(&s,193,0,60));
  if(held&2)assert(!forma_button_event(&s,194,0,60));
  if(held&4)assert(!forma_button_event(&s,116,0,60));
 }
 forma_buttons_init(&s,0);
 assert(!forma_button_event(&s,116,1,70));assert(!forma_button_event(&s,116,2,71));assert(forma_button_event(&s,116,0,72)==FORMA_EXIT);
 assert(!forma_button_event(&s,59,1,80));assert(!forma_button_event(&s,999,0,81));
 assert(needs_full_refresh(0,0,0,0));assert(needs_full_refresh(3,0,0,1));
 assert(!needs_full_refresh(2,11,20,0));assert(!needs_full_refresh(2,12,14.99,0));assert(needs_full_refresh(2,12,15,0));
 puts("DAILY_CONTROLS_POLICY_OK page_release_only repeats_ignored chord_orders deadline startup_held power_release refresh_thresholds");
}
