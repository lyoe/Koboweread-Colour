#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include "../../display-bridge-prototype/forma-buttons.h"
int main(void){
 struct forma_buttons s;
 forma_buttons_init(&s,0);
 for(int code=193;code<=194;code++){
  int short_action=code==193?FORMA_NEXT:FORMA_PREVIOUS;
  assert(!forma_button_event(&s,code,0,0));
  assert(!forma_button_event(&s,code,1,10));
  assert(forma_button_event(&s,code,0,10.999)==short_action);
  assert(!forma_button_event(&s,code,1,12));
  assert(forma_button_event(&s,code,0,13)==FORMA_REFRESH);
  assert(!forma_button_event(&s,code,1,20));
  for(int n=0;n<30;n++)assert(!forma_button_event(&s,code,2,20.1+n*.1));
  assert(forma_button_event(&s,code,0,25)==FORMA_REFRESH);
  assert(!forma_button_event(&s,code,0,26));
  // Previous long hold does not contaminate the following short press.
  assert(!forma_button_event(&s,code,1,30));assert(forma_button_event(&s,code,0,30.1)==short_action);
 }
 for(int order=0;order<2;order++){
  int a=order?194:193,b=order?193:194;
  assert(!forma_button_event(&s,a,1,40));assert(!forma_button_event(&s,b,1,40.2));
  assert(!forma_button_event(&s,a,0,40.8));assert(!forma_button_event(&s,b,0,41.5));
  assert(!forma_button_event(&s,a,1,50));assert(!forma_button_event(&s,b,1,50.5));
  assert(!forma_button_event(&s,a,0,52.5));assert(forma_button_event(&s,b,0,53)==FORMA_EXIT);
 }
 for(unsigned held=1;held<=7;held++){
  forma_buttons_init(&s,held);
  if(held&1)assert(!forma_button_event(&s,193,0,70));
  if(held&2)assert(!forma_button_event(&s,194,0,70));
  if(held&4)assert(!forma_button_event(&s,116,0,70));
 }
 forma_buttons_init(&s,0);
 assert(!forma_button_event(&s,116,1,80));assert(forma_button_event(&s,116,0,81)==FORMA_EXIT);
 assert(!forma_button_event(&s,59,1,82));
 puts("FORMA77_POLICY_OK upper_next lower_previous one_second_refresh_no_page repeats startup_held chord_exit_priority power_exit");
}
