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
public_state=$base/resident-state
gov=/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
launch_pid=;dns_check_pid=;log_offset=0;log_tick=0;reader_missing=0;mode=starting;since=0
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
readers_stopped(){
 n=0;for p in /proc/[0-9]*;do [ "$(readlink "$p/exe" 2>/dev/null)" = /mnt/onboard/.adds/koreader/luajit ] || continue;n=$((n+1));[ "$(awk '{print $3}' "$p/stat" 2>/dev/null)" = T ] || return 1;done;[ "$n" -gt 0 ]
}
reader_alive(){
 for p in /proc/[0-9]*;do [ "$(readlink "$p/exe" 2>/dev/null)" = /mnt/onboard/.adds/koreader/luajit ] && return 0;done;return 1
}
# Only data written since the previous scan (plus a 256-byte overlap so a line
# split between two scans is still seen); the log grows for the whole residency.
scan_log(){
 size=$(stat -c %s "$root/lab/android.log")
 [ "$size" -gt "$log_offset" ] || return 0
 from=$((log_offset>256?log_offset-256:0))
 tail -c +"$((from+1))" "$root/lab/android.log" | head -c "$((size-from))" > "$rdir/log-chunk"
 log_offset=$size
 if grep -Fq "ANR in com.tencent.weread.eink" "$rdir/log-chunk";then echo RESIDENT_WEREAD_ANR;return 129;fi
 if grep -Fq "FATAL EXCEPTION IN SYSTEM PROCESS" "$rdir/log-chunk";then echo RESIDENT_SYSTEM_FATAL;return 117;fi
 if grep -q 'FATAL EXCEPTION' "$rdir/log-chunk";then echo RESIDENT_APP_FATAL_CONTINUING;fi
 if grep -q 'Displayed com.tencent.weread.eink/' "$rdir/log-chunk";then rm -f "$root/lab/weread-startup-service-grace";fi
 return 0
}
start_input_helper(){
 if [ -n "$touch_java_pid" ] && kill -0 "$touch_java_pid" 2>/dev/null;then return 0;fi
 rm -f "$root/dev/socket/kobo_touch_probe"
 CLASSPATH=/lab/native-resolution/weread-process-entry.dex:/lab/install-framework-probe.dex KOBO_VM_SHIM=1 "$root/lab/legacy-exec" "$root" /system/bin/app_process32 -Xnorelocate -Xnoimage-dex2oat -Xnodex2oat /system/bin WeReadInteractiveInput resident > "$root/lab/touch-probe.log" 2>&1 &
 touch_java_pid=$!
 poll_until 300 'grep -q ANDROID_TOUCH_PROBE_WINDOW_READY "$root/lab/touch-probe.log" || ! kill -0 "$touch_java_pid" 2>/dev/null' || :
 grep -q ANDROID_TOUCH_PROBE_WINDOW_READY "$root/lab/touch-probe.log" || { echo RESIDENT_INPUT_HELPER_NOT_READY;return 114; }
}
# DNS health check through the session's proxy, in the background so a switch
# never waits for it. Repeated failures also trip the proxy's public fallback.
dns_check(){
 if [ -n "$dns_check_pid" ] && kill -0 "$dns_check_pid" 2>/dev/null;then return 0;fi
 ( for attempt in 1 2 3;do "$base/resident/resident-dns-check" weread.qq.com && exit 0;sleep 1;done;echo RESIDENT_DNS_CHECK_FAILED attempts=3 ) &
 dns_check_pid=$!
}
foreground_enter(){
 if [ -n "${KOBO_RESIDENT_GOVERNOR:-}" ] && [ -w "$gov" ];then echo performance > "$gov";fi
 "$base/resident/resident-sched" normal
 rm -f "$root/lab/interactive-session-finish" "$root/lab/interactive-refresh-request"
 "$root/lab/native-resolution/live-display" 43260 "$display_profile" > "$root/lab/native-resolution/live-display.log" 2>&1 &
 bridge_pid=$!
 poll_until 80 'grep -q LIVE_DISPLAY_STARTED "$root/lab/native-resolution/live-display.log" || ! kill -0 "$bridge_pid" 2>/dev/null' || :
 kill -0 "$bridge_pid" 2>/dev/null || { cat "$root/lab/native-resolution/live-display.log";bridge_pid=;echo RESIDENT_DISPLAY_NOT_STARTED;return 115; }
 start_input_helper || return $?
 # SIGSTOP takes effect only once KOReader leaves an uninterruptible call (an
 # e-ink refresh it just requested); the forwarder requires both stopped.
 poll_until 50 'readers_stopped' || echo RESIDENT_KOREADER_STOP_SLOW
 for touch_attempt in 1 2;do
  "$root/lab/native-resolution/touch-forward" 43200 --resident > "$root/lab/native-resolution/touch-forward.log" 2>&1 &
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
 rm -f "$root/lab/interactive-session-finish" "$root/lab/interactive-refresh-request"
}
background_enter(){
 foreground_leave || return $?
 "$base/resident/resident-sched" idle
 if [ -n "${KOBO_RESIDENT_GOVERNOR:-}" ] && [ -w "$gov" ];then echo "$KOBO_RESIDENT_GOVERNOR" > "$gov";fi
 sync
 resident_state background
}
open_weread(){
 if [ -n "$launch_pid" ] && kill -0 "$launch_pid" 2>/dev/null;then echo RESIDENT_OPEN_ALREADY_RUNNING;return 0;fi
 # First opening of a residency: the verified launch path (identity cache, PMS
 # signer/UID, process registry). Later openings start directly from the launcher.
 printf 'Finite initial DB service grace\n' > "$root/lab/weread-startup-service-grace";chmod 0400 "$root/lab/weread-startup-service-grace"
 ( KOBO_VM_SHIM=1 timeout 120 "$root/lab/legacy-exec" "$root" /system/bin/app_process32 -Xnorelocate -Xnoimage-dex2oat -Xnodex2oat /system/bin FrameworkProbe launch-weread "$previous_weread_uid" \
   && { printf 'verified\n' > "$marker_dir/weread-verified";chmod 0644 "$marker_dir/weread-verified";echo RESIDENT_WEREAD_LAUNCH_VERIFIED; } ) > "$rdir/launch.log" 2>&1 &
 launch_pid=$!
 echo RESIDENT_WEREAD_LAUNCH_REQUESTED
}
resident_main(){
 [ -n "$previous_weread_uid" ] || { echo NO_PERSISTED_WEREAD_IDENTITY;return 56; }
 poll_until 600 'grep -q APK_RESIDENT_HOME_READY "$root/lab/android.log" || [ -e "$base/resident-shutdown-request" ]' || { echo RESIDENT_HOME_NOT_READY;return 25; }
 if [ -e "$base/resident-shutdown-request" ];then rm -f "$base/resident-shutdown-request";echo RESIDENT_SHUTDOWN_REQUESTED_DURING_START;return 0;fi
 startup_stage resident_home_ready
 foreground_enter || return $?
 startup_stage interactive_ready
 while :;do
  kill -0 "$dms_pid" 2>/dev/null || { echo RESIDENT_SYSTEM_SERVER_EXITED;return 120; }
  kill -0 "$dns_pid" 2>/dev/null || { echo RESIDENT_DNS_PROCESS_EXITED;return 118; }
  kill -0 "$host_network_pid" 2>/dev/null || { echo RESIDENT_HOST_NETWORK_OBSERVER_EXITED;return 118; }
  if [ -n "$launch_pid" ] && ! kill -0 "$launch_pid" 2>/dev/null;then cat "$rdir/launch.log";wait "$launch_pid" 2>/dev/null || echo RESIDENT_WEREAD_LAUNCH_FAILED;launch_pid=;fi
  log_tick=$((log_tick+1))
  if [ $((log_tick%4)) = 0 ];then
   scan_log || return $?
   [ "$(df -Pk "$root" | awk 'NR==2 {print $4}')" -ge 65536 ] || { echo RESIDENT_IMAGE_FREE_SPACE_BELOW_64_MIB;return 120; }
   if reader_alive;then reader_missing=0;else reader_missing=$((reader_missing+1));fi
   [ "$reader_missing" -lt 3 ] || { echo RESIDENT_KOREADER_GONE_SHUTDOWN;[ "$mode" != foreground ] || foreground_leave || :;return 0; }
  fi
  request_word=
  if take_request "$host_request" 0;then echo "RESIDENT_HOST_REQUEST $request_word";host_word=$request_word;else host_word=;fi
  if take_request "$app_request" 10000;then echo "RESIDENT_LAUNCHER_REQUEST $request_word";app_word=$request_word;else app_word=;fi
  if [ -e "$base/resident-shutdown-request" ];then rm -f "$base/resident-shutdown-request";host_word=shutdown;fi
  if [ "$host_word" = shutdown ] || [ "$app_word" = shutdown ];then
   [ "$mode" != foreground ] || foreground_leave || :
   echo RESIDENT_SHUTDOWN_REQUESTED;return 0
  fi
  if [ "$mode" = foreground ];then
   [ "$app_word" != open-weread ] || open_weread
   if [ "$app_word" = background ] || [ -e "$root/lab/interactive-session-finish" ];then background_enter || return $?
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
  fi
  sleep 0.5
 done
}
