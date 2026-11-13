#!/bin/sh
# Observe only this bounded experiment; stop its private Java actors on low RAM.
set -eu
r=/tmp/kobo-android-runtime
owner=$1
case "$owner" in ''|*[!0-9]*) exit 2;;esac
started=$(awk '{print $22}' /proc/$owner/stat)
minimum=999999999;low=0;samples=0
trap 'echo "NATIVE_MEMORY_MONITOR_STOP minimum_available_kib=$minimum samples=$samples";exit 0' TERM INT
while [ "$(awk '{print $22}' /proc/$owner/stat 2>/dev/null || true)" = "$started" ];do
 available=$(awk '/^MemAvailable:/ {print $2}' /proc/meminfo)
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
 sleep 0.5
done
echo "NATIVE_MEMORY_MONITOR_STOP minimum_available_kib=$minimum samples=$samples"
