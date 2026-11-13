#!/bin/sh
set -eu
. "$1"
present=1;calls=0;waits=0
lab_module_present(){ [ "$present" = 1 ]; }
rmmod(){ calls=$((calls+1));if [ "$calls" = 3 ];then present=0;return 0;fi;return 1; }
sleep(){ [ "$1" = 0.1 ];waits=$((waits+1)); }
lab_unload_module binder_linux
[ "$calls" = 3 ] && [ "$waits" = 2 ] && [ "$present" = 0 ]
calls=0;waits=0
lab_unload_module binder_linux
[ "$calls" = 0 ] && [ "$waits" = 0 ]
present=1
rmmod(){ calls=$((calls+1));return 1; }
result=0;lab_unload_module ashmem_linux || result=$?
[ "$result" = 1 ] && [ "$calls" = 20 ] && [ "$waits" = 19 ]
calls=0;result=0;lab_unload_module unknown || result=$?
[ "$result" = 2 ] && [ "$calls" = 0 ]
echo MODULE90_OWNED_UNLOAD_RETRY_ABSENT_BUSY_BOUND_AND_UNKNOWN_REJECTION_OK
