#!/bin/sh
# Device-local launcher of the resident Android session (KOReader menu).
# Shares the lock and state file of the other entries, so only one Android
# session of any kind can exist. No SSH, desktop service or remote rendering.
set -eu
umask 077
[ "$#" = 0 ] && [ "$(id -u)" = 0 ] || exit 2
base=/mnt/onboard/.adds/kobo-android-lab
[ -d "$base" ] && [ ! -L "$base" ] || exit 3
cd "$base"
lock=$base/weread-launch.lock
state=$base/v197-launch.state
log=$base/resident-v197-launch-private.log
public=$base/resident-v197-state
display_profile=reagl-text
[ ! -L "$base/resident-v197/display-profile" ] || exit 4
if [ -f "$base/resident-v197/display-profile" ];then
 read -r display_profile < "$base/resident-v197/display-profile" || exit 4
 # fast4 (DU4 page turns, 2026-11-02) was removed on 2026-11-06: the Libra 2
 # panel does not visibly redraw DU4 updates. A leftover setting runs as default.
 case "$display_profile" in classic|reagl|reagl-text) :;; fast4) display_profile=reagl-text;; *) exit 4;;esac
fi
for file in "$state" "$state.new" "$log" "$log.previous" "$public" "$public.new";do [ ! -L "$file" ] || exit 4;done
if ! mkdir "$lock" 2>/dev/null;then
 [ -d "$lock" ] && [ ! -L "$lock" ] && [ -f "$lock/owner" ] && [ ! -L "$lock/owner" ] || exit 5
 read old_pid old_start old_boot extra < "$lock/owner"
 case "$old_pid:$old_start" in *[!0-9:]*|:*|*:) exit 5;;esac
 [ -z "${extra:-}" ] || exit 5
 if [ "$old_boot" = "$(cat /proc/sys/kernel/random/boot_id)" ] && [ "$(awk '{print $22}' /proc/$old_pid/stat 2>/dev/null || :)" = "$old_start" ];then exit 5;fi
 rm "$lock/owner";rmdir "$lock";mkdir "$lock"
fi
printf '%s %s %s\n' "$$" "$(awk '{print $22}' /proc/$$/stat)" "$(cat /proc/sys/kernel/random/boot_id)" > "$lock/owner"
child=;phase=runtime_begin;started_uptime=0
write_state(){
 printf 'stage=%s\nexit=%s\nphase=%s\nstarted_uptime=%s\nmode=resident\nround=1\nrounds=1\n' "$1" "$2" "$phase" "$started_uptime" > "$state.new"
 mv "$state.new" "$state"
}
cleanup(){
 code=$?;trap - EXIT
 printf 'state=stopped\nexit=%s\n' "$code" > "$public.new";mv "$public.new" "$public"
 rm -f "$lock/owner";rmdir "$lock" 2>/dev/null || :
 exit "$code"
}
trap cleanup EXIT
trap '' HUP
for required in graphics-v197.img resident-v197/run-resident-session.sh fd-clean-exec;do
 if [ ! -f "$base/$required" ] || [ -L "$base/$required" ];then write_state failed 3;exit 3;fi
done
[ ! -f "$log" ] || mv "$log" "$log.previous"
rm -f "$base/resident-v197-shutdown-request"
printf 'state=starting\n' > "$public.new";mv "$public.new" "$public"
started_uptime=$(awk '{print $1}' /proc/uptime)
write_state starting pending
/usr/bin/env -i PATH=/sbin:/usr/sbin:/bin:/usr/bin \
 KOBO_WEREAD_HEAP_HEADROOM=0 KOBO_WEREAD_DISPLAY_CADENCE=30 KOBO_WEREAD_DISPLAY_PROFILE="$display_profile" KOBO_WEREAD_STARTUP_PROFILE=reading KOBO_WEREAD_REUSE=1 KOBO_WEREAD_NATIVE_AUDIO=1 \
 KOBO_WEREAD_HOST_NETWORK=1 KOBO_WEREAD_HOST_NETWORK_STATE=1 \
 KOBO_WEREAD_JIT=1 KOBO_WEREAD_INTERACTIVE=1 KOBO_WEREAD_TOUCH_MODE=session KOBO_WEREAD_SESSION_SECONDS=1800 KOBO_WEREAD_RESIDENT=1 \
 KOBO_WEREAD_TOUCH_TEST=1 KOBO_WEREAD_LIVE_DISPLAY=1 KOBO_WEREAD_INPUT_TEST=1 \
 /bin/sh "$base/resident-v197/run-resident-session.sh" > "$log" 2>&1 &
child=$!
trap 'kill -TERM "$child" 2>/dev/null || :' TERM INT
# KEY=value fields read with the shell's read: $(sed …) forked twice per
# 2 s tick for the whole session (resident-v197/bgquiet-disabled: as before,
# apart from the reads).
field_of(){ v=;[ -f "$1" ] || return 0;while IFS= read -r line;do case "$line" in "$2="*) v=${line#*=};return 0;;esac;done < "$1"; }
while kill -0 "$child" 2>/dev/null;do
 field_of "$public" state;current=$v;field_of "$state" stage;stage_now=$v
 case "$current" in
  foreground) [ "$stage_now" = running ] || write_state running pending;;
  background) [ "$stage_now" = background ] || write_state background pending;;
  stopping) [ "$stage_now" = restoring ] || write_state restoring pending;;
  *)
   observed=$(awk '/^WEREAD_STARTUP_STAGE / {for(i=1;i<=NF;i++)if($i ~ /^stage=/){sub(/^stage=/,"",$i);p=$i}} END {print p}' "$log" 2>/dev/null || :)
   case "$observed" in runtime_begin|framework_begin|app_launch_begin|resident_home_ready)
    if [ "$phase" != "$observed" ];then phase=$observed;write_state starting pending;fi;;
   esac;;
 esac
 if [ "$current" = background ] && [ ! -e "$base/resident-v197/bgquiet-disabled" ];then sleep 5;else sleep 2;fi
done
result=0;wait "$child" || result=$?
if [ "$result" != 0 ];then write_state failed "$result";exit "$result";fi
write_state complete 0
exit 0
