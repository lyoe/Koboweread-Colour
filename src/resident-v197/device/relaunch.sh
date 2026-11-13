#!/bin/sh
# Restart of the 1.9.7 session from the launcher's 阅读优化 page (2026-11-11,
# docs/1.9.7版本.md "启动器：阅读优化"). Started detached by resident-loop.sh
# (relaunch_start) just before the session ends, with resident-v197/
# relaunch.state = restart | compile. Waits until the session is fully gone
# (status none, launch lock released, session script ended), runs
# compile-menu.sh first for "compile" (WeRead's speed-profile odex, Android
# closed, about a minute; result in compile-menu.log), then starts the session
# again as the KOReader menu does. relaunch.state stays while this runs (the
# KOReader plugin shows a message and keeps the Kobo awake meanwhile) and is
# removed once the new session is starting. Log: resident-v197/relaunch.log.
b=/mnt/onboard/.adds/kobo-android-lab;res=$b/resident-v197;st=$res/relaunch.state;log=$res/relaunch.log
cd /
exec > "$log" 2>&1
kind=;read -r kind < "$st" 2>/dev/null || :
echo "RELAUNCH_BEGIN kind=$kind uptime=$(awk '{print $1}' /proc/uptime)"
case "$kind" in restart|compile) :;; *) echo RELAUNCH_BAD_KIND;rm -f "$st";exit 2;;esac
n=0
while [ $n -lt 300 ];do
 [ "$(sh "$res/resident-ctl.sh" status)" = none ] && [ ! -d "$b/weread-launch.lock" ] && ! pgrep -f run-resident-session.sh >/dev/null && break
 sleep 1;n=$((n+1))
done
[ $n -lt 300 ] || { echo RELAUNCH_SESSION_STILL_RUNNING;rm -f "$st";exit 3; }
echo "RELAUNCH_SESSION_ENDED after_s=$n"
sleep 2
if [ "$kind" = compile ];then
 printf 'compiling\n' > "$st.new" && mv "$st.new" "$st"
 /bin/sh "$res/compile-menu.sh"
 echo "RELAUNCH_COMPILED $(tail -n 1 "$res/compile-menu.log")"
fi
printf 'launching\n' > "$st.new" && mv "$st.new" "$st"
/usr/bin/setsid "$b/fd-clean-exec" /bin/sh "$res/launch-resident.sh" </dev/null >/dev/null 2>&1 &
n=0;while [ "$(sh "$res/resident-ctl.sh" status)" = none ] && [ $n -lt 60 ];do sleep 1;n=$((n+1));done
rm -f "$st"
echo "RELAUNCH_STARTED status=$(sh "$res/resident-ctl.sh" status) after_s=$n"
