#!/bin/sh
set -eu
root=/tmp/kobo-android-runtime
expected=$(awk '$1=="com.kobo.lab.installed" {print $2}' "$root/data/system/kobo-install-process-allowlist.txt")
[ -n "$expected" ] || exit 60
system_seen=0;app_seen=0
for process in /proc/[0-9]*;do
 [ "$(readlink "$process/root" 2>/dev/null || :)" = "$root" ] || continue
 [ "$(readlink "$process/exe" 2>/dev/null || :)" = "$root/system/bin/app_process32" ] || continue
 uid=$(awk '/^Uid:/{print $2}' "$process/status" 2>/dev/null || :)
 caps=$(awk '/^CapEff:/{print $2}' "$process/status" 2>/dev/null || :)
 echo "LAB_ART_PROCESS_IDENTITY pid=${process##*/} uid=$uid caps=$caps"
 if [ "$uid" = 1000 ];then
  [ "$caps" = 0000000001800020 ] || exit 61
  system_seen=1
 fi
 if [ "$uid" = "$expected" ];then
  [ "$caps" = 0000000000000000 ] || exit 62
  app_seen=1
 fi
done
[ "$system_seen" = 1 ] && [ "$app_seen" = 1 ] || exit 63
echo "LAB_SYSTEM_AND_INSTALLED_UID_AUDIT_OK system=1000 app=$expected"
