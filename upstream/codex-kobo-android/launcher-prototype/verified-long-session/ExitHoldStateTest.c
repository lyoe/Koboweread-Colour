#include <stdio.h>
#include <assert.h>
#include "../../display-bridge-prototype/exit-hold.h"
int main(void){
 struct exit_hold h={0};
 assert(exit_hold_contact(&h,10,10,0)==HOLD_BUFFER);assert(exit_hold_release(&h,0.2)==HOLD_FLUSH);
 assert(exit_hold_contact(&h,10,10,1)==HOLD_BUFFER);assert(exit_hold_contact(&h,40,10,1.1)==HOLD_FLUSH);assert(exit_hold_release(&h,4)==HOLD_NORMAL);
 assert(exit_hold_contact(&h,10,10,5)==HOLD_BUFFER);exit_hold_tick(&h,7);assert(h.state==2);assert(exit_hold_contact(&h,100,100,7.2)==HOLD_BUFFER);assert(exit_hold_release(&h,8)==HOLD_EXIT);
 assert(exit_hold_contact(&h,59,59,10)==HOLD_BUFFER);assert(exit_hold_release(&h,11.99)==HOLD_FLUSH);
 assert(exit_hold_contact(&h,59,59,12)==HOLD_BUFFER);assert(exit_hold_release(&h,14)==HOLD_EXIT);
 assert(exit_hold_contact(&h,60,10,15)==HOLD_NORMAL);
 assert(exit_hold_contact(&h,10,10,20)==HOLD_BUFFER);exit_hold_cancel(&h);assert(exit_hold_release(&h,25)==HOLD_NORMAL);
 puts("EXIT_HOLD_STATE_CHECKS_OK short_tap motion_cancel hold_release boundary multitouch_cancel");
}
