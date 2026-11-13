#!/bin/sh
# Fast-entry copy of Codex's native-memory-watchdog.sh: same thresholds, actions
# and log lines, but /proc is read with the shell's builtin read instead of awk,
# so the 0.5 s loop forks only its sleep (it runs for the whole session, and in
# the resident launcher also while Android waits in the background).
set -eu
r=/tmp/kobo-android-runtime
owner=$1
case "$owner" in ''|*[!0-9]*) exit 2;;esac
start_of(){ read -r st 2>/dev/null < "/proc/$1/stat" || return 1;set -- ${st##*) };echo "${20}"; }
# Not via $(…): a command substitution would fork again.
owner_start(){ owner_now=;read -r st 2>/dev/null < "/proc/$owner/stat" || return 0;set -- ${st##*) };owner_now=${20}; }
mem_available(){ available=;while read -r key value unit;do [ "$key" = MemAvailable: ] && { available=$value;return 0; };done < /proc/meminfo; }
# Every 0.5 s in front; every 5 s while the resident session waits in the
# background (memory hardly moves then), unless resident/bgquiet-disabled.
public=/mnt/onboard/.adds/kobo-android-lab/resident-state
quiet_flag=/mnt/onboard/.adds/kobo-android-lab/resident/bgquiet-disabled
in_background(){ [ ! -e "$quiet_flag" ] && [ -f "$public" ] || return 1;while IFS= read -r line;do [ "$line" = state=background ] && return 0;done < "$public";return 1; }
started=$(start_of "$owner")
minimum=999999999;low=0;samples=0
trap 'echo "NATIVE_MEMORY_MONITOR_STOP minimum_available_kib=$minimum samples=$samples";exit 0' TERM INT
owner_start
while [ "$owner_now" = "$started" ];do
 mem_available
 case "$available" in ''|*[!0-9]*) exit 3;;esac
 [ "$available" -ge "$minimum" ] || minimum=$available
 samples=$((samples+1))
 if [ $((samples%10)) = 1 ];then echo "NATIVE_MEMORY_SAMPLE available_kib=$available minimum_kib=$minimum";fi
 if [ "$available" -lt 49152 ];then low=$((low+1));else low=0;fi
 if [ "$low" -ge 2 ];then
  echo "NATIVE_MEMORY_ABORT available_kib=$available threshold_kib=49152"
  # No stock process, OOM policy, kernel setting or unrelated application is changed.
  for p in /proc/[0-9]*;do
   [ "$(readlink "$p/root" 2>/dev/null || true)" = "$r" ] || continue
   [ "$(readlink "$p/exe" 2>/dev/null || true)" = "$r/system/bin/app_process32" ] || continue
   kill -TERM "${p##*/}" 2>/dev/null || true
  done
  exit 4
 fi
 if in_background;then sleep 5;else sleep 0.5;fi
 owner_start
done
echo "NATIVE_MEMORY_MONITOR_STOP minimum_available_kib=$minimum samples=$samples"
