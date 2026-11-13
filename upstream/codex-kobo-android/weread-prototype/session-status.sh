#!/bin/sh
# Read-only live check; an old runner log is not proof a window is still open.
set -eu
root=/tmp/kobo-android-runtime
file=$root/lab/interactive-session-ready
inactive(){ echo WEREAD_INTERACTIVE_SESSION_NOT_ACTIVE;exit 0; }
[ -f "$file" ] && [ ! -L "$file" ] || inactive
[ "$(stat -c %u "$file")" = 0 ] && [ "$(stat -c %a "$file")" = 400 ] || inactive
field(){ awk -F= -v key="$1" '$1==key {print $2}' "$file"; }
[ "$(field boot_id)" = "$(cat /proc/sys/kernel/random/boot_id)" ] || inactive
duration=$(field duration_seconds)
case "$duration" in '') duration=180;;180|1800) :;;*) inactive;;esac
ready=$(field ready_uptime)
age=$(awk -v ready="$ready" '{printf "%d",$1-ready}' /proc/uptime)
[ "$age" -ge 0 ] && [ "$age" -lt "$duration" ] || inactive
for kind in display touch;do
 pid=$(field "${kind}_pid");start=$(field "${kind}_start")
 case "$pid" in ''|*[!0-9]*) inactive;;esac
 [ "$(awk '{print $22}' "/proc/$pid/stat" 2>/dev/null || :)" = "$start" ] || inactive
 if [ "$kind" = display ];then exe=live-display;else exe=touch-forward;fi
 [ "$(readlink "/proc/$pid/exe" 2>/dev/null || :)" = "$root/lab/$exe" ] || inactive
done
echo "WEREAD_INTERACTIVE_SESSION_ACTIVE remaining_seconds=$((duration-age))"
