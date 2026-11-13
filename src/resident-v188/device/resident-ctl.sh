#!/bin/sh
# Control a running resident Android session from KOReader:
#   resident-ctl.sh status            -> prints starting|foreground|background|stopping|none
#   resident-ctl.sh foreground        -> bring the launcher/WeRead back on screen
#   resident-ctl.sh tuning            -> the same, with the launcher's 阅读优化 page on top
#   resident-ctl.sh shutdown [SECONDS] -> stop Android; optionally wait for full cleanup
set -eu
[ "$(id -u)" = 0 ] || exit 2
base=/mnt/onboard/.adds/kobo-android-lab
public=$base/resident-v188-state
rdir=/tmp/kobo-android-runtime/lab/resident
field(){ sed -n "s/^$1=//p" "$public" 2>/dev/null | head -n 1; }
current(){
 [ -f "$public" ] && [ ! -L "$public" ] || { echo none;return; }
 state=$(field state)
 case "$state" in foreground|background|stopping)
  owner=$(field owner)
  case "$owner" in ''|*[!0-9]*) echo none;return;;esac
  [ "$(field boot_id)" = "$(cat /proc/sys/kernel/random/boot_id)" ] && kill -0 "$owner" 2>/dev/null || { echo none;return; }
  echo "$state";;
 starting) [ -d "$base/weread-launch.lock" ] && echo starting || echo none;;
 *) echo none;;
 esac
}
request(){
 [ -d "$rdir" ] && [ ! -L "$rdir" ] && [ "$(stat -c %u:%a "$rdir")" = 0:700 ] || exit 4
 printf '%s\n' "$1" > "$rdir/request.$$";mv "$rdir/request.$$" "$rdir/request"
 # Wake the loop if it waits out a quiet background tick (resident-loop.sh).
 owner=$(field owner);case "$owner" in ''|*[!0-9]*) :;; *) kill -USR1 "$owner" 2>/dev/null || :;;esac
}
case "${1:-}" in
 status) current;;
 foreground) [ "$(current)" = background ] || exit 3;request foreground;;
 tuning) [ "$(current)" = background ] || exit 3;request tuning;;
 shutdown)
  # While starting, the loop takes this marker as soon as it runs.
  case "$(current)" in foreground|background) request shutdown;; starting) : > "$base/resident-v188-shutdown-request";; *) exit 0;;esac
  limit=${2:-0};case "$limit" in *[!0-9]*) exit 2;;esac
  [ "$limit" != 0 ] || exit 0
  waited=0
  while [ "$waited" -lt "$((limit*2))" ] && [ -d "$base/weread-launch.lock" ];do sleep 0.5;waited=$((waited+1));done
  [ ! -d "$base/weread-launch.lock" ] || exit 6;;
 *) exit 2;;
esac
