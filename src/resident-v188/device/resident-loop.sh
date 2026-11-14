# Resident launcher loop, sourced by run-native-weread-resident.sh after the
# framework is ready. Android stays up between foreground periods:
#  foreground: display bridge (pauses KOReader) + touch/key forwarding;
#  background: KOReader runs, Android threads at SCHED_IDLE, CPU governor restored.
# Leaving foreground: exit gesture, power key, or the launcher's button.
# Shutdown: launcher button, KOReader menu/hooks, KOReader gone, or any failure.
# Every return falls through to the inner script's verified cleanup.
base=/mnt/onboard/.adds/kobo-android-lab
rdir=$root/lab/resident
marker_dir=$root/data/local/tmp/kobo-resident
app_request=$root/data/data/com.kobo.lab.smoke/files/resident-request
host_request=$rdir/request
public_state=$base/resident-v188-state
suspend_request=$base/resident-v188-suspend-request
gov=/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
launch_pid=;dns_check_pid=;readahead_pid=;seg_state=;seg_pid=;anr_times=;log_offset=0;log_tick=0;reader_missing=0;mode=starting;since=0;reader_cache=
resident_state(){
 mode=$1;since=$(awk '{print $1}' /proc/uptime)
 printf 'state=%s\nsince_uptime=%s\nboot_id=%s\nowner=%s\n' "$mode" "$since" "$(cat /proc/sys/kernel/random/boot_id)" "$$" > "$public_state.new"
 mv "$public_state.new" "$public_state"
 echo "RESIDENT_STATE state=$mode uptime=$since"
}
# One-word request from a regular, unlinked, fixed-owner file; removed once read.
take_request(){ # path owner_uid
 [ -e "$1" ] || [ -L "$1" ] || return 1
 if [ -L "$1" ] || [ ! -f "$1" ] || [ "$(stat -c %u "$1")" != "$2" ] || [ "$(stat -c %s "$1")" -gt 32 ];then rm -f "$1";echo RESIDENT_REQUEST_REJECTED;return 1;fi
 request_word=$(head -n 1 "$1" | tr -cd 'a-z-');rm -f "$1";return 0
}
# The bridge resumes KOReader itself; this is only for a bridge that died
# without its restore path. It signals KOReader's own processes only.
resume_reader(){
 for p in /proc/[0-9]*;do [ "$(readlink "$p/exe" 2>/dev/null)" = /mnt/onboard/.adds/koreader/luajit ] && kill -CONT "${p##*/}" 2>/dev/null;done;return 0
}
# KOReader's processes as "pid:starttime" entries. One /proc scan (a readlink
# fork per process) fills the cache; later checks read /proc/PID/stat with the
# shell's builtin read, so a background residency does not fork every 2 s.
# A vanished or reused PID (start time differs) triggers a fresh scan.
reader_scan(){
 reader_cache=
 for p in /proc/[0-9]*;do
  [ "$(readlink "$p/exe" 2>/dev/null)" = /mnt/onboard/.adds/koreader/luajit ] || continue
  read -r st 2>/dev/null < "$p/stat" || continue
  set -- ${st##*) };reader_cache="$reader_cache ${p##*/}:${20}"
 done
}
reader_cached_ok(){
 [ -n "$reader_cache" ] || return 1
 for e in $reader_cache;do
  read -r st 2>/dev/null < "/proc/${e%%:*}/stat" || return 1
  set -- ${st##*) };[ "${20}" = "${e#*:}" ] || return 1
 done
}
reader_alive(){ reader_cached_ok && return 0;reader_scan;[ -n "$reader_cache" ]; }
readers_stopped(){
 reader_alive || return 1
 for e in $reader_cache;do
  read -r st 2>/dev/null < "/proc/${e%%:*}/stat" || return 1
  set -- ${st##*) };[ "$1" = T ] || return 1
 done
}
# Only data written since the previous scan (plus a 256-byte overlap so a line
# split between two scans is still seen); the log grows for the whole residency.
scan_log(){
 size=$(stat -c %s "$root/lab/android.log")
 [ "$size" -gt "$log_offset" ] || return 0
 from=$((log_offset>256?log_offset-256:0))
 tail -c +"$((from+1))" "$root/lab/android.log" | head -c "$((size-from))" > "$rdir/log-chunk"
 log_offset=$size
 # Only WeRead's main process stops Android. A subprocess that stops
 # responding (e.g. com.tencent.weread.eink:seg) is logged and left to
 # ActivityManager, which handles that process on its own.
 # A main-process ANR does not stop Android either. Measured (tests/v188-anr.sh):
 # with no dialog to show, ActivityManager kills WeRead itself ("user request
 # after error") and the launcher comes back, from which WeRead opens again in
 # a running Android instead of after a full restart. Three main-process ANRs
 # within 10 minutes still stop Android (something is wrong beyond one stall).
 # Off (any main-process ANR stops Android): resident-v188/anrcontinue-disabled.
 if grep -Eq 'ANR in com[.]tencent[.]weread[.]eink([^:]|$)' "$rdir/log-chunk";then
  [ ! -e "$base/resident-v188/anrcontinue-disabled" ] || { echo RESIDENT_WEREAD_ANR;return 129; }
  now=$(awk '{print int($1)}' /proc/uptime);recent=
  for t in $anr_times;do [ $((now-t)) -lt 600 ] && recent="$recent $t";done
  anr_times="$recent $now";set -- $anr_times
  [ $# -lt 3 ] || { echo "RESIDENT_WEREAD_ANR_REPEATED count=$#";return 129; }
  echo "RESIDENT_WEREAD_ANR_CONTINUING count=$#"
 fi
 if grep -Fq "ANR in com.tencent.weread.eink:" "$rdir/log-chunk";then
  echo "RESIDENT_WEREAD_SUBPROCESS_ANR_CONTINUING name=$(grep -o 'ANR in com[.]tencent[.]weread[.]eink:[A-Za-z0-9_.]*' "$rdir/log-chunk" | head -n 1 | cut -d: -f2)"
 fi
 if grep -Fq "FATAL EXCEPTION IN SYSTEM PROCESS" "$rdir/log-chunk";then echo RESIDENT_SYSTEM_FATAL;return 117;fi
 if grep -q 'FATAL EXCEPTION' "$rdir/log-chunk";then echo RESIDENT_APP_FATAL_CONTINUING;fi
 if grep -q 'Displayed com.tencent.weread.eink/' "$rdir/log-chunk";then rm -f "$root/lab/weread-startup-service-grace" "$root/dev/socket/dnsproxyd.hold" "$marker_dir/weread-starting";trim_launcher;fi
 return 0
}
# 阅读优化 page in the launcher (2026-11-11, docs/1.9.7版本.md "启动器：阅读优化"):
# the launcher cannot see the SD card, so the current settings are written to
# $marker_dir/tuning (and, once per session, tuning-start: what this session
# runs with). Its one request, set-<13 x y|n>-<c|r|t>[-seconds], carries the whole wanted
# state in tune_all order plus the display profile (both versions' launchers
# share the first ten positions; switches this version does not have, i.e. not in
# tune_names, are neither listed nor touched); the files are switched
# at once and take effect at the next start. restart / compile end this
# session and start relaunch.sh, which compiles the odex first if asked and
# starts the session again (WeRead opens by itself, as at every start).
tune_all='syncthrottle markusers chapterprefetch bookprefetch fontcache skia segdict seglow poollow zram idle-suspend page-wipe aosp-pinyin'
tune_names=$tune_all
idle_timeout(){
 local seconds=300 file=$base/fast-v188/idle-suspend-seconds
 [ ! -f "$file" ] || read -r seconds < "$file" || :
 case "$seconds" in 60|180|300|600|900|1800) echo "$seconds";; *) echo 300;;esac
}
tune_state(){ # file
 tf=$base/fast-v188;tp=reagl-text
 [ ! -f "$base/resident-v188/display-profile" ] || read -r tp < "$base/resident-v188/display-profile" || :
 case "$tp" in classic|reagl|reagl-text) :;; *) tp=reagl-text;;esac
 {
  for n in $tune_names;do
   if [ "$n" = page-wipe ] || [ "$n" = aosp-pinyin ];then
    if [ -e "$tf/$n-enabled" ];then echo "$n 1";else echo "$n 0";fi
   elif [ -e "$tf/$n-disabled" ];then echo "$n 0"
   else echo "$n 1"
   fi
  done
  echo "display $tp"
  echo "idle-suspend-seconds $(idle_timeout)"
  cl=$base/resident-v188/compile-menu.log;cc=$(sed -n 's/^COMPILE_MENU_DONE code=\([0-9]*\).*/\1/p' "$cl" 2>/dev/null | tail -n 1)
  if [ -n "$cc" ];then
   echo "compile $cc $(sed -n 's/^FAST_COMPILE_OUTPUT_BYTES=\([0-9]*\).*/\1/p' "$cl" | tail -n 1 | awk '{print int($1/1048576)}') $(sed -n 's/^FAST_COMPILE_RESULT code=0 seconds=\([0-9]*\).*/\1/p' "$cl" | tail -n 1) $(date -r "$cl" '+%m-%d_%H:%M')"
  else echo "compile none";fi
 } > "$1.new";chmod 0644 "$1.new";mv "$1.new" "$1"
}
tune_set(){ # yynyy...-t[-seconds]
 local word=$1 seconds=
 case "$word" in
  *-*-*) seconds=${word##*-};word=${word%-*}
   case "$seconds" in 60|180|300|600|900|1800) :;; *) echo "RESIDENT_TUNE_REJECTED $1";return 0;;esac;;
 esac
 w=${word%-*};d=${word##*-};tf=$base/fast-v188;i=0
 ok=1;case ${#w} in 10|11|12|13) :;; *) ok=0;; esac
 case "$w" in *[!yn]*) ok=0;;esac
 case "$d" in c|r|t) :;; *) ok=0;;esac
 [ $ok = 1 ] || { echo "RESIDENT_TUNE_REJECTED $1";return 0; }
 for n in $tune_all;do
  v=$(echo "$w" | cut -c$((i+1)));i=$((i+1))
  [ -n "$v" ] || break
  case " $tune_names " in *" $n "*) :;; *) continue;;esac
  if [ "$n" = page-wipe ] || [ "$n" = aosp-pinyin ];then
   if [ "$v" = y ];then : > "$tf/$n-enabled";else rm -f "$tf/$n-enabled";fi
   continue
  fi
  if [ "$v" = y ];then [ ! -e "$tf/$n-disabled" ] || { rm -f "$tf/$n-disabled";echo "RESIDENT_TUNE $n=1"; }
  else [ -e "$tf/$n-disabled" ] || { : > "$tf/$n-disabled";echo "RESIDENT_TUNE $n=0"; };fi
 done
 case "$d" in c) dp=classic;; r) dp=reagl;; *) dp=reagl-text;;esac
 tp=;[ ! -f "$base/resident-v188/display-profile" ] || read -r tp < "$base/resident-v188/display-profile" || :
 [ "$tp" = "$dp" ] || { printf '%s\n' "$dp" > "$base/resident-v188/display-profile.new" && mv "$base/resident-v188/display-profile.new" "$base/resident-v188/display-profile";echo "RESIDENT_TUNE display=$dp"; }
 if [ -n "$seconds" ] && [ "$seconds" != "$(idle_timeout)" ];then
  printf '%s\n' "$seconds" > "$tf/idle-suspend-seconds.new" &&
   mv "$tf/idle-suspend-seconds.new" "$tf/idle-suspend-seconds"
 fi
 sync;tune_state "$marker_dir/tuning"
}
# The launcher opens its settings page once for each new open-tuning marker
# (contents differ each time): KOReader's 阅读优化 menu item.
open_tuning(){ awk '{print $1}' /proc/uptime > "$marker_dir/open-tuning.new";chmod 0644 "$marker_dir/open-tuning.new";mv "$marker_dir/open-tuning.new" "$marker_dir/open-tuning";echo RESIDENT_OPEN_TUNING; }
relaunch_start(){ # restart|compile
 [ -f "$base/resident-v188/relaunch.sh" ] || { echo RESIDENT_RELAUNCH_MISSING;return 1; }
 printf '%s\n' "$1" > "$base/resident-v188/relaunch.state.new" && mv "$base/resident-v188/relaunch.state.new" "$base/resident-v188/relaunch.state"
 /usr/bin/setsid "$base/fd-clean-exec" /bin/sh "$base/resident-v188/relaunch.sh" </dev/null >/dev/null 2>&1 &
 echo "RESIDENT_RELAUNCH kind=$1"
}
# Launcher trim (2026-11-08): the launcher is no longer persistent, so once
# WeRead is on screen its process (~9 MB) is ended; ActivityManager starts it
# again when HOME is shown (cold start ~2-3 s). It only sends requests from
# its own buttons, i.e. while it is on screen. Off: resident-v188/launchertrim-disabled.
trim_launcher(){
 [ ! -e "$base/resident-v188/launchertrim-disabled" ] || return 0
 for p in /proc/[0-9]*;do
  [ "$(readlink "$p/root" 2>/dev/null)" = "$root" ] || continue
  c=$(tr '\0' '\n' < "$p/cmdline" 2>/dev/null | head -n 1)
  [ "$c" = com.kobo.lab.smoke ] || continue
  kill -KILL "${p#/proc/}" 2>/dev/null && echo "RESIDENT_LAUNCHER_TRIMMED pid=${p#/proc/}"
 done
}
start_input_helper(){
 if [ -n "$touch_java_pid" ] && kill -0 "$touch_java_pid" 2>/dev/null;then return 0;fi
 rm -f "$root/dev/socket/kobo_touch_probe"
 CLASSPATH=/lab/native-resolution/weread-process-entry.dex:/lab/install-framework-probe.dex KOBO_VM_SHIM=1 "$root/lab/legacy-exec" "$root" /system/bin/app_process32 -Xnorelocate -Xnoimage-dex2oat -Xnodex2oat /system/bin WeReadInteractiveInput resident > "$root/lab/touch-probe.log" 2>&1 &
 touch_java_pid=$!
 poll_until 300 'grep -q ANDROID_TOUCH_PROBE_WINDOW_READY "$root/lab/touch-probe.log" || ! kill -0 "$touch_java_pid" 2>/dev/null' || :
 grep -q ANDROID_TOUCH_PROBE_WINDOW_READY "$root/lab/touch-probe.log" || { echo RESIDENT_INPUT_HELPER_NOT_READY;cat "$root/lab/touch-probe.log";return 114; }
}
# DNS health check through the session's proxy, in the background so a switch
# never waits for it. Repeated failures also trip the proxy's public fallback.
# The API host: weread.qq.com itself is blocked below and never resolves.
dns_check(){
 if [ -n "$dns_check_pid" ] && kill -0 "$dns_check_pid" 2>/dev/null;then return 0;fi
 ( for attempt in 1 2 3;do "$base/resident-v188/resident-dns-check" i.weread.qq.com && exit 0;sleep 1;done;echo RESIDENT_DNS_CHECK_FAILED attempts=3 ) &
 dns_check_pid=$!
}
foreground_enter(){
 rm -f "$suspend_request" "$root/lab/resident-page-wipe"
 android_thaw
 if [ -n "${KOBO_RESIDENT_GOVERNOR:-}" ] && [ -w "$gov" ];then echo performance > "$gov";fi
 "$base/resident-v188/resident-sched" normal
 rm -f "$root/lab/interactive-session-finish" "$root/lab/interactive-refresh-request" "$root/lab/interactive-screenshot-request"
 KOBO_PAGE_WIPE=$page_wipe "$root/lab/native-resolution/live-display" 43260 "$display_profile" > "$root/lab/native-resolution/live-display.log" 2>&1 &
 bridge_pid=$!
 poll_until 80 'grep -q LIVE_DISPLAY_STARTED "$root/lab/native-resolution/live-display.log" || ! kill -0 "$bridge_pid" 2>/dev/null' || :
 kill -0 "$bridge_pid" 2>/dev/null || { cat "$root/lab/native-resolution/live-display.log";bridge_pid=;echo RESIDENT_DISPLAY_NOT_STARTED;return 115; }
 start_input_helper || return $?
 # SIGSTOP takes effect only once KOReader leaves an uninterruptible call (an
 # e-ink refresh it just requested); the forwarder requires both stopped.
 poll_until 50 'readers_stopped' || echo RESIDENT_KOREADER_STOP_SLOW
 for touch_attempt in 1 2;do
  KOBO_IDLE_SUSPEND_SECONDS=$idle_seconds KOBO_PAGE_WIPE=$page_wipe "$root/lab/native-resolution/touch-forward" 43200 --resident > "$root/lab/native-resolution/touch-forward.log" 2>&1 &
  touch_pid=$!
  poll_until 30 'grep -q HOST_TOUCH_FORWARD_READY "$root/lab/native-resolution/touch-forward.log" || ! kill -0 "$touch_pid" 2>/dev/null' || :
  kill -0 "$touch_pid" 2>/dev/null && break
  cat "$root/lab/native-resolution/touch-forward.log";wait "$touch_pid" 2>/dev/null || :;touch_pid=
  [ "$touch_attempt" = 1 ] || { echo RESIDENT_TOUCH_NOT_STARTED;return 116; }
  sleep 0.5
 done
 resident_state foreground
 dns_check
}
foreground_leave(){
 if [ -n "$touch_pid" ];then kill -TERM "$touch_pid" 2>/dev/null || :;wait "$touch_pid" 2>/dev/null || :;touch_pid=;cat "$root/lab/native-resolution/touch-forward.log";fi
 if [ -n "$bridge_pid" ];then
  kill -TERM "$bridge_pid" 2>/dev/null || :;bridge_result=0;wait "$bridge_pid" || bridge_result=$?;bridge_pid=
  cat "$root/lab/native-resolution/live-display.log"
  grep -q LIVE_DISPLAY_FRAMEBUFFER_RESTORE_BYTES_MATCH "$root/lab/native-resolution/live-display.log" || { echo RESIDENT_DISPLAY_RESTORE_UNVERIFIED result=$bridge_result;resume_reader;return 73; }
 fi
 rm -f "$root/lab/interactive-session-finish" "$root/lab/interactive-refresh-request" "$root/lab/interactive-screenshot-request"
}
# Background freeze: while KOReader is on screen, every process of the
# Android chroot is stopped (apps first) and continued on the way back
# (system first). Measured: whole-system CPU in the background 7.2% -> 3.1%;
# system_server survived a 5 min freeze (its watchdog's 30 s checks finish
# within milliseconds, so they are done before a stop lands), switching back
# took 0.8 s, the next book open was normal. Every way out of the loop
# continues them first (run-native-weread.sh). Off with
# resident-v188/bgfreeze-disabled.
frozen_apps=;frozen_rest=
android_freeze(){
 [ -z "$frozen_apps$frozen_rest" ] && [ ! -e "$base/resident-v188/bgfreeze-disabled" ] || return 0
 for p in /proc/[0-9]*;do
  [ "$(readlink "$p/root" 2>/dev/null)" = "$root" ] && [ -r "$p/status" ] || continue
  uid=;while read -r key a rest;do [ "$key" = Uid: ] && { uid=$a;break; };done < "$p/status"
  case "$uid" in ''|*[!0-9]*) continue;;esac
  if [ "$uid" -ge 10000 ];then frozen_apps="$frozen_apps ${p#/proc/}";else frozen_rest="$frozen_rest ${p#/proc/}";fi
 done
 for p in $frozen_apps $frozen_rest;do kill -STOP "$p" 2>/dev/null || :;done
 set -- $frozen_apps;na=$#;set -- $frozen_rest
 echo "RESIDENT_ANDROID_FROZEN apps=$na system=$#"
}
android_thaw(){
 [ -n "$frozen_apps$frozen_rest" ] || return 0
 for p in $frozen_rest $frozen_apps;do kill -CONT "$p" 2>/dev/null || :;done
 frozen_apps=;frozen_rest=;echo RESIDENT_ANDROID_THAWED
}
background_enter(){
 foreground_leave || return $?
 "$base/resident-v188/resident-sched" idle
 if [ -n "${KOBO_RESIDENT_GOVERNOR:-}" ] && [ -w "$gov" ];then echo "$KOBO_RESIDENT_GOVERNOR" > "$gov";fi
 sync
 android_freeze
 if [ "${1:-}" = reason=idle-suspend ];then
  printf '%s\n' "$$" > "$suspend_request.new" && mv "$suspend_request.new" "$suspend_request"
  echo RESIDENT_IDLE_SUSPEND_HANDOFF
 fi
 resident_state background
}
# Ask the input helper (WeReadInteractiveInput, already running with a system
# context) to do something a separate JVM would otherwise start for, ~1.5 s
# of CPU each: helper_request NAME [TEXT] -> 0 when it answered "ok" within
# 10 s. Off with resident-v188/helperlaunch-disabled.
helper_request(){
 [ ! -e "$base/resident-v188/helperlaunch-disabled" ] && grep -q WEREAD_HELPER_REQUESTS_READY "$root/lab/touch-probe.log" 2>/dev/null || return 1
 rm -f "$rdir/$1.result"
 printf '%s\n' "${2:-}" > "$rdir/$1.tmp";mv "$rdir/$1.tmp" "$rdir/$1"
 n=0;while [ ! -f "$rdir/$1.result" ] && [ $n -lt 50 ];do sleep 0.2;n=$((n+1));done
 answer=$(cat "$rdir/$1.result" 2>/dev/null);rm -f "$rdir/$1" "$rdir/$1.result"
 echo "RESIDENT_HELPER_REQUEST name=$1 answer=${answer:-none}"
 [ "$answer" = ok ]
}
open_weread(){
 if [ -n "$launch_pid" ] && kill -0 "$launch_pid" 2>/dev/null;then echo RESIDENT_OPEN_ALREADY_RUNNING;return 0;fi
 # First opening of a residency: the verified launch path (identity cache, PMS
 # signer/UID, process registry). Later openings start directly from the launcher.
 printf 'Finite initial DB service grace\n' > "$root/lab/weread-startup-service-grace";chmod 0400 "$root/lab/weread-startup-service-grace"
 # The launcher shows "starting" while this marker exists: from the launch
 # until WeRead is displayed (scan_log), or until the launch fails. A press in
 # that time opens WeRead once it is gone and weread-verified exists.
 printf 'starting\n' > "$marker_dir/weread-starting";chmod 0644 "$marker_dir/weread-starting"
 # One-shot install of WeRead 1.8.8 over the package in the image (copied
 # from 1.9.8's): a root-owned lab/v188-install-once inside the image, removed
 # as it is taken. The probe checks /lab/weread.apk against the 1.8.8 digest,
 # installs it with downgrade allowed (app data kept), then launches it.
 launch_args="launch-weread $previous_weread_uid";launch_limit=120
 if [ -f "$root/lab/v188-install-once" ] && [ ! -L "$root/lab/v188-install-once" ];then
  rm -f "$root/lab/v188-install-once";launch_args="install-apk /lab/weread.apk";launch_limit=250;echo V188_INSTALL_ONCE
 fi
 ( { [ "$launch_args" = "launch-weread $previous_weread_uid" ] && helper_request helper-launch "$previous_weread_uid"; } \
   || KOBO_VM_SHIM=1 timeout "$launch_limit" "$root/lab/legacy-exec" "$root" /system/bin/app_process32 -Xnorelocate -Xnoimage-dex2oat -Xnodex2oat /system/bin FrameworkProbe $launch_args \
   && { printf 'verified\n' > "$marker_dir/weread-verified";chmod 0644 "$marker_dir/weread-verified";echo RESIDENT_WEREAD_LAUNCH_VERIFIED; } ) > "$rdir/launch.log" 2>&1 &
 launch_pid=$!
 echo RESIDENT_WEREAD_LAUNCH_REQUESTED
}
# WeRead 1.8.8 starts its word-segmentation service (process :seg, used by
# typesetting, selection and link detection) only when a book is opened: a
# cold ART process (~2 s CPU on this single core, plus its pages) inside the
# first open. Start it once per session instead, after the prewarmed launch
# is verified and the odex readahead is over (SegPrestart in the entry dex:
# this runtime has no am.jar). Root may start the unexported service; it then
# stays up, as it would after the first open anyway.
# Off with resident-v188/segprestart-disabled.
seg_prestart(){
 seg_state=started
 [ ! -e "$base/resident-v188/segprestart-disabled" ] || { seg_state=off;echo RESIDENT_SEG_PRESTART_DISABLED;return 0; }
 ( helper_request helper-seg || CLASSPATH=/lab/native-resolution/weread-process-entry.dex KOBO_VM_SHIM=1 timeout 60 "$root/lab/legacy-exec" "$root" /system/bin/app_process32 -Xnorelocate -Xnoimage-dex2oat -Xnodex2oat /system/bin SegPrestart ) > "$rdir/seg-prestart.log" 2>&1 &
 seg_pid=$!;echo RESIDENT_SEG_PRESTART
}
resident_main(){
 [ -n "$previous_weread_uid" ] || { echo NO_PERSISTED_WEREAD_IDENTITY;return 56; }
 # KOReader's 阅读优化 menu item on a fresh start: no prewarm this once (it
 # takes input focus from the launcher at once), the launcher shows its
 # settings page and WeRead opens from its button. The launcher is already on
 # screen before this loop runs, so it is brought to the front once more
 # (helper-home, as for a running session) to read the marker.
 tune_once=0;if [ -e "$base/resident-v188/tuning-once" ];then rm -f "$base/resident-v188/tuning-once";tune_once=1;open_tuning;fi
 poll_until 600 'grep -q APK_RESIDENT_HOME_READY "$root/lab/android.log" || [ -e "$base/resident-v188-shutdown-request" ]' || { echo RESIDENT_HOME_NOT_READY;return 25; }
 if [ -e "$base/resident-v188-shutdown-request" ];then rm -f "$base/resident-v188-shutdown-request";echo RESIDENT_SHUTDOWN_REQUESTED_DURING_START;return 0;fi
 startup_stage resident_home_ready
 idle_seconds=$(idle_timeout);[ ! -e "$base/fast-v188/idle-suspend-disabled" ] || idle_seconds=0
 page_wipe=0;[ ! -e "$base/fast-v188/page-wipe-enabled" ] || page_wipe=1
 tune_state "$marker_dir/tuning";cp "$marker_dir/tuning" "$marker_dir/tuning-start"
 # Background quiet mode: while KOReader is on screen the loop wakes every
 # 10 s instead of every 1 s (each tick forks a sleep; together with the
 # memory watchdog and launch-resident.sh the polling cost ~5% of the CPU).
 # resident-ctl.sh signals USR1 after writing a request, which ends the
 # wait at once, so switching back is as quick as before.
 # Off with resident-v188/bgquiet-disabled.
 trap ':' USR1
 # Read WeRead's odex (84 MB) sequentially while the bridge and input helper
 # start (CPU-bound), so the prewarmed launch below finds it in the page cache
 # instead of faulting it in from the SD card page by page. The inner cleanup
 # stops it before the image is unmounted. Off with resident/readahead-disabled.
 if [ ! -e "$base/resident-v188/prewarm-disabled" ] && [ ! -e "$base/resident-v188/readahead-disabled" ];then
  cat "$root"/data/app/com.tencent.weread.eink-*/oat/arm/base.odex > /dev/null 2>&1 &
  readahead_pid=$!;echo RESIDENT_READAHEAD_ODEX
 fi
 foreground_enter || return $?
 # Prewarm: start WeRead's verified launch as soon as the launcher is on
 # screen instead of waiting for its button. Started before the bridge it
 # delayed the launcher by ~5 s for the same time to WeRead.
 # Off with resident/prewarm-disabled (back to the launcher-first flow).
 # WeRead 1.8.8, as 1.9.8: weread.qq.com answers its
 # test-flight query with a forced update to 2.1.2 (a dialog with no way
 # past it, plus a 121 MB download), so the DNS proxy reports that one name
 # as unknown. The API host i.weread.qq.com is a different name.
 echo weread.qq.com > "$root/dev/socket/dnsproxyd.block";echo RESIDENT_DNS_BLOCK host=weread.qq.com
 if [ $tune_once = 1 ];then
  n=0;while ! grep -q WEREAD_HELPER_REQUESTS_READY "$root/lab/touch-probe.log" 2>/dev/null && [ $n -lt 40 ];do sleep 0.5;n=$((n+1));done
  helper_request helper-home || echo RESIDENT_OPEN_TUNING_HOME_FAILED
 fi
 if [ ! -e "$base/resident-v188/prewarm-disabled" ] && [ $tune_once = 0 ];then
  # DNS hold: WeRead's first draw is 10-17 s slower when network responses
  # (follow/account sync) come back before it and run on its main path. The
  # proxy answers nothing while this file exists; scan_log removes it once
  # WeRead is displayed, the proxy ignores it after 30 s. Off with
  # resident/dnshold-disabled.
  if [ ! -e "$base/resident-v188/dnshold-disabled" ];then : > "$root/dev/socket/dnsproxyd.hold";echo RESIDENT_DNS_HOLD;fi
  echo RESIDENT_PREWARM_WEREAD;open_weread
 fi
 startup_stage interactive_ready
 while :;do
  kill -0 "$dms_pid" 2>/dev/null || { echo RESIDENT_SYSTEM_SERVER_EXITED;return 120; }
  kill -0 "$dns_pid" 2>/dev/null || { echo RESIDENT_DNS_PROCESS_EXITED;return 118; }
  kill -0 "$host_network_pid" 2>/dev/null || { echo RESIDENT_HOST_NETWORK_OBSERVER_EXITED;return 118; }
  if [ -n "$readahead_pid" ] && ! kill -0 "$readahead_pid" 2>/dev/null;then wait "$readahead_pid" 2>/dev/null || :;readahead_pid=;echo RESIDENT_READAHEAD_DONE;fi
  if [ -n "$launch_pid" ] && ! kill -0 "$launch_pid" 2>/dev/null;then cat "$rdir/launch.log";wait "$launch_pid" 2>/dev/null || { echo RESIDENT_WEREAD_LAUNCH_FAILED;rm -f "$marker_dir/weread-starting"; };launch_pid=;fi
  # Not before WeRead's home is on screen (scan_log removes weread-starting
  # at its Displayed line): started with the launch, the service's JVM and
  # :seg's start competed with the home page for the single core.
  # Off (start as soon as the launch is verified): resident-v188/segafterhome-disabled.
  seg_wait_home=0;[ -e "$base/resident-v188/segafterhome-disabled" ] || [ ! -e "$marker_dir/weread-starting" ] || seg_wait_home=1
  [ -n "$seg_state" ] || [ -n "$launch_pid" ] || [ -n "$readahead_pid" ] || [ ! -e "$marker_dir/weread-verified" ] || [ "$seg_wait_home" = 1 ] || seg_prestart
  if [ -n "$seg_pid" ] && ! kill -0 "$seg_pid" 2>/dev/null;then wait "$seg_pid" 2>/dev/null && echo RESIDENT_SEG_PRESTART_DONE || echo RESIDENT_SEG_PRESTART_FAILED;tr '\n' ' ' < "$rdir/seg-prestart.log";echo;seg_pid=;fi
  log_tick=$((log_tick+1))
  # Health checks every 2 s in front (0.5 s ticks), every 10 s in the
  # background (1 s ticks): the log scan and df still fork.
  check_every=4;[ "$mode" != background ] || check_every=10
  [ "$mode" != background ] || [ -e "$base/resident-v188/bgquiet-disabled" ] || check_every=1
  if [ $((log_tick%check_every)) = 0 ];then
   scan_log || return $?
   [ "$(df -Pk "$root" | awk 'NR==2 {print $4}')" -ge 65536 ] || { echo RESIDENT_IMAGE_FREE_SPACE_BELOW_64_MIB;return 120; }
   if reader_alive;then reader_missing=0;else reader_missing=$((reader_missing+1));fi
   [ "$reader_missing" -lt 3 ] || { echo RESIDENT_KOREADER_GONE_SHUTDOWN;[ "$mode" != foreground ] || foreground_leave || :;return 0; }
  fi
  request_word=
  if take_request "$host_request" 0;then echo "RESIDENT_HOST_REQUEST $request_word";host_word=$request_word;else host_word=;fi
  if take_request "$app_request" 10000;then echo "RESIDENT_LAUNCHER_REQUEST $request_word";app_word=$request_word;else app_word=;fi
  if [ -e "$base/resident-v188-shutdown-request" ];then rm -f "$base/resident-v188-shutdown-request";host_word=shutdown;fi
  case "$app_word" in
   set-*) tune_set "${app_word#set-}";app_word=;;
   restart|compile) relaunch_start "$app_word" && app_word=shutdown || app_word=;;
  esac
  if [ "$host_word" = shutdown ] || [ "$app_word" = shutdown ];then
   [ "$mode" != foreground ] || foreground_leave || :
   echo RESIDENT_SHUTDOWN_REQUESTED;return 0
  fi
  if [ "$mode" = foreground ];then
   [ "$app_word" != open-weread ] || open_weread
   if [ "$app_word" = background ] || [ -e "$root/lab/interactive-session-finish" ];then
    finish_reason=
    if [ -f "$root/lab/interactive-session-finish" ] && [ ! -L "$root/lab/interactive-session-finish" ];then
     read -r finish_reason < "$root/lab/interactive-session-finish" || :
    fi
    background_enter "$finish_reason" || return $?
   else
    kill -0 "$bridge_pid" 2>/dev/null || { echo RESIDENT_DISPLAY_PROCESS_EXITED;wait "$bridge_pid" 2>/dev/null || :;bridge_pid=;resume_reader;return 118; }
    kill -0 "$touch_java_pid" 2>/dev/null || { echo RESIDENT_INPUT_PROCESS_EXITED;return 118; }
    if ! kill -0 "$touch_pid" 2>/dev/null;then
     # The forwarder ends by itself only at its 12 h limit or on an input error.
     wait "$touch_pid" 2>/dev/null || :;touch_pid=;cat "$root/lab/native-resolution/touch-forward.log";background_enter || return $?
    fi
   fi
  elif [ "$host_word" = foreground ];then
   foreground_enter || return $?
  elif [ "$host_word" = tuning ];then
   foreground_enter || return $?
   open_tuning;helper_request helper-home || echo RESIDENT_OPEN_TUNING_HOME_FAILED
  fi
  if [ "$mode" != background ];then sleep 0.5
  elif [ -e "$base/resident-v188/bgquiet-disabled" ];then sleep 1
  else
   sleep 10 & bg_sleep=$!
   [ -e "$host_request" ] || [ -e "$base/resident-v188-shutdown-request" ] || wait "$bg_sleep" 2>/dev/null || :
   kill "$bg_sleep" 2>/dev/null || :;wait "$bg_sleep" 2>/dev/null || :
  fi
 done
}
