#!/bin/sh
# Device-local, bounded launcher. No SSH, desktop service or remote rendering.
set -eu
umask 077
[ "$#" -le 1 ] && [ "$(id -u)" = 0 ] || exit 2
mode=${1:-180}
rounds=1
case "$mode" in
 180|1800) session_seconds=$mode;;
 startup-checks) session_seconds=180;rounds=10;;
 *) exit 2;;
esac
base=/mnt/onboard/.adds/kobo-android-lab
[ -d "$base" ] && [ ! -L "$base" ] || exit 3
cd "$base"
lock=$base/weread-launch.lock
state=$base/weread-launch.state
log=$base/weread-launch-private.log
summary=$base/startup-validation-result.txt
for file in "$state" "$state.new" "$log" "$log.previous" "$summary";do [ ! -L "$file" ] || exit 4;done
if ! mkdir "$lock" 2>/dev/null;then
 [ -d "$lock" ] && [ ! -L "$lock" ] && [ -f "$lock/owner" ] && [ ! -L "$lock/owner" ] || exit 5
 read old_pid old_start old_boot extra < "$lock/owner"
 case "$old_pid:$old_start" in *[!0-9:]*|:*|*:) exit 5;;esac
 [ -z "${extra:-}" ] || exit 5
 if [ "$old_boot" = "$(cat /proc/sys/kernel/random/boot_id)" ] && [ "$(awk '{print $22}' /proc/$old_pid/stat 2>/dev/null || :)" = "$old_start" ];then exit 5;fi
 # Only remove our known stale record; unknown entries prevent rmdir.
 rm "$lock/owner";rmdir "$lock";mkdir "$lock"
fi
printf '%s %s %s\n' "$$" "$(awk '{print $22}' /proc/$$/stat)" "$(cat /proc/sys/kernel/random/boot_id)" > "$lock/owner"
child=
write_state(){ printf 'stage=%s\nexit=%s\n' "$1" "$2" > "$state.new";mv "$state.new" "$state"; }
cleanup(){
 code=$?;trap - EXIT
 rm -f "$lock/owner";rmdir "$lock" 2>/dev/null || :
 exit "$code"
}
trap cleanup EXIT
trap '' HUP
for required in graphics.img run-weread-session.sh session-status.sh fd-clean-exec;do
 if [ ! -f "$base/$required" ] || [ -L "$base/$required" ];then write_state failed 3;exit 3;fi
done
if [ "$mode" = startup-checks ];then
 printf 'mode=startup-checks\nrequested_rounds=10\n' > "$summary"
fi
round=0
while [ "$round" -lt "$rounds" ];do
round=$((round+1))
# Keep at most two private run logs, never expose their contents in the menu.
[ ! -f "$log" ] || mv "$log" "$log.previous"
write_state starting pending
/usr/bin/env -i PATH=/sbin:/usr/sbin:/bin:/usr/bin \
 KOBO_WEREAD_STARTUP_PROFILE=reading KOBO_WEREAD_REUSE=1 KOBO_WEREAD_NATIVE_AUDIO=1 \
 KOBO_WEREAD_HOST_NETWORK=1 KOBO_WEREAD_HOST_NETWORK_STATE=1 \
 KOBO_WEREAD_JIT=1 KOBO_WEREAD_INTERACTIVE=1 KOBO_WEREAD_TOUCH_MODE=session KOBO_WEREAD_SESSION_SECONDS="$session_seconds" \
 KOBO_WEREAD_TOUCH_TEST=1 KOBO_WEREAD_LIVE_DISPLAY=1 KOBO_WEREAD_INPUT_TEST=1 \
 /bin/sh "$base/run-weread-session.sh" > "$log" 2>&1 &
child=$!
trap 'kill -TERM "$child" 2>/dev/null || :' TERM INT
shown=0;ready_polls=0;finish_requested=0
while kill -0 "$child" 2>/dev/null;do
 if /bin/sh "$base/session-status.sh" | grep -q 'SESSION_ACTIVE ';then
  [ "$shown" = 1 ] || write_state running pending
  shown=1
  if [ "$mode" = startup-checks ];then
   ready_polls=$((ready_polls+1))
   if [ "$ready_polls" -ge 6 ] && [ "$finish_requested" = 0 ];then
    # This test ends a verified live session; it never injects input.
    marker=/tmp/kobo-android-runtime/lab/interactive-session-finish
    if (set -C; printf 'reason=developer-startup-check\n' > "$marker") 2>/dev/null;then
     chmod 0400 "$marker";finish_requested=1
    fi
   fi
  fi
 elif [ "$shown" = 1 ];then write_state restoring pending;fi
 sleep 2
done
result=0;wait "$child" || result=$?
if [ "$mode" = startup-checks ];then
 if [ "$result" = 0 ] && { [ "$shown" != 1 ] || ! grep -q '^LIVE_DISPLAY_FRAMEBUFFER_RESTORE_BYTES_MATCH$' "$log"; };then result=127;fi
 printf 'round=%s result=%s ready_seen=%s developer_finish=%s\n' "$round" "$result" "$shown" "$finish_requested" >> "$summary"
 awk -v n="$round" '/^WEREAD_STARTUP_STAGE / {print "round=" n " " $0}' "$log" >> "$summary"
 if [ "$result" != 0 ];then printf 'completed_rounds=%s\nresult=%s\n' "$((round-1))" "$result" >> "$summary";fi
 if [ "$result" = 0 ] && grep -q '^HOST_EXIT_CORNER_HOLD_RELEASED$' "$log";then
  printf 'completed_rounds=%s\ncancelled_by_user=1\nresult=0\n' "$round" >> "$summary"
  write_state check_cancelled 0;exit 0
 fi
fi
if [ "$result" != 0 ];then write_state failed "$result";exit "$result";fi
if [ "$round" -lt "$rounds" ];then sleep 2;fi
done
[ "$mode" != startup-checks ] || printf 'completed_rounds=10\nresult=0\n' >> "$summary"
write_state complete 0
exit 0
