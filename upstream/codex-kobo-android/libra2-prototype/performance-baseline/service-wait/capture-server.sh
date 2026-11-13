#!/bin/sh
# Read-only application state except a standard SIGQUIT diagnostic append.
set -eu
[ "$#" = 1 ] && [ "$(id -u)" = 0 ] || exit 2
case "$1" in 1|2) :;; *) exit 2;;esac
r=/tmp/kobo-android-runtime
b=/mnt/onboard/.adds/kobo-android-lab/212-service-wait-private
[ -d "$b" ] && [ ! -L "$b" ] || exit 3
[ -f "$r/lab/interactive-session-ready" ] || exit 4
cmp -s "$b/session-ready" "$r/lab/interactive-session-ready" || exit 4
[ ! -e "$b/sample-$1-status.txt" ] && [ ! -L "$b/sample-$1-status.txt" ] || exit 3
pids=$r/lab/install-processes.pid
target=
for proc in /proc/[0-9]*;do
 p=${proc#/proc/}
 case "$p" in ''|*[!0-9]*) continue;;esac
 [ "$(readlink /proc/$p/root 2>/dev/null || :)" = "$r" ] || continue
 [ "$(readlink /proc/$p/exe 2>/dev/null || :)" = "$r/system/bin/app_process32" ] || continue
 [ "$(awk '/^Uid:/ {print $2}' /proc/$p/status 2>/dev/null || :)" = 1000 ] || continue
 tr '\000' ' ' < /proc/$p/cmdline | grep -q "/system/bin ServerLauncher reading-server unused" || continue
 [ -z "$target" ] || exit 5
 target=$p;start=$(awk '{print $22}' /proc/$p/stat)
done
[ -n "$target" ] || exit 6
file=$r/data/anr/traces.txt
[ -f "$file" ] && [ ! -L "$file" ] || exit 7
[ "$(stat -c %a "$file")" = 666 ] || exit 7
before=$(stat -c %s "$file")
[ "$(awk '{print $22}' /proc/$target/stat 2>/dev/null || :)" = "$start" ] || exit 8
umask 077
{
 echo "pid=$target start=$start before_bytes=$before"
 cat /proc/uptime
} > "$b/sample-$1-status.txt"
kill -QUIT "$target"
complete=0
for n in $(seq 1 40);do
 cmp -s "$b/session-ready" "$r/lab/interactive-session-ready" || break
 [ "$(awk '{print $22}' /proc/$target/stat 2>/dev/null || :)" = "$start" ] || break
 [ "$(stat -c %s "$file")" -ge "$before" ] || { echo TRACE_TRUNCATED_DURING_CAPTURE >> "$b/sample-$1-status.txt";exit 9; }
 if tail -c +$((before+1)) "$file" | grep -q -- "----- end $target -----";then complete=1;break;fi
 sleep 0.1
done
if [ "$complete" = 1 ];then
 tail -c +$((before+1)) "$file" > "$b/sample-$1-system-threads.txt"
 chmod 0600 "$b/sample-$1-system-threads.txt"
fi
{
 echo "complete=$complete"
 cat /proc/uptime
} >> "$b/sample-$1-status.txt"
[ "$complete" = 1 ]
