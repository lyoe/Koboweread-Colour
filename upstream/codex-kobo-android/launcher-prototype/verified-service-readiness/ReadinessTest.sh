#!/bin/sh
# Linux /proc lifecycle and socket checks. Never starts Android or touches input/power.
set -eu
umask 077
. "${1:?helper source}"
worker=${2:?test service executable}
b=/tmp/kobo-readiness71-integration
[ ! -e "$b" ] || exit 90
mkdir "$b";test_pid=
cleanup(){ test -z "$test_pid" || { kill -CONT "$test_pid" 2>/dev/null || :;kill -TERM "$test_pid" 2>/dev/null || :;wait "$test_pid" 2>/dev/null || :; };rm -rf "$b"; }
trap cleanup EXIT
start(){
 rm -f "$b/socket" "$b/resource";: > "$b/log"
 "$worker" "$1" "$b/socket" "$b/resource" > "$b/log" & test_pid=$!
}
stop(){ kill -TERM "$test_pid";wait "$test_pid" 2>/dev/null || :;test_pid=; }
start socket-delayed
wait_log_ready delayed_socket "$test_pid" "$b/log" TEST_SERVICE_READY socket "$b/socket" > "$b/result"
grep -q 'STARTUP_COMPONENT_READY' "$b/result"
! grep -q 'attempts=0$' "$b/result"
stop
start marker-delayed
wait_log_ready delayed_marker "$test_pid" "$b/log" TEST_SERVICE_READY file "$b/resource" > "$b/result"
! grep -q 'attempts=0$' "$b/result"
kill -STOP "$test_pid";sleep 0.1
if wait_log_ready stopped "$test_pid" "$b/log" TEST_SERVICE_READY socket "$b/socket";then exit 91;fi
kill -CONT "$test_pid";dead_pid=$test_pid;stop
if wait_log_ready stale_dead "$dead_pid" "$b/log" TEST_SERVICE_READY socket "$b/socket";then exit 92;fi
start no-marker
if wait_log_ready no_marker "$test_pid" "$b/log" TEST_SERVICE_READY socket "$b/socket";then exit 93;fi
stop
if readiness_alive 0;then exit 94;fi
if wait_log_ready invalid 0 "$b/log" TEST_SERVICE_READY socket "$b/socket";then exit 95;fi
echo SERVICE_READINESS_OK delayed_socket delayed_marker stopped_dead_rejected stale_log_rejected timeout invalid_pid no_android_or_input
