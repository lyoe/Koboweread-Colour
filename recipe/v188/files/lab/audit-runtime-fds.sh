#!/bin/sh
set -eu
root=/tmp/kobo-android-runtime
count=0;app_seen=0
for process in /proc/[0-9]*;do
 [ "$(readlink "$process/root" 2>/dev/null || :)" = "$root" ] || continue
 count=$((count+1))
 uid=$(awk '/^Uid:/ {print $2}' "$process/status" 2>/dev/null || :)
 [ "$uid" != 10000 ] || app_seen=1
 for descriptor in "$process"/fd/*;do
  target=$(readlink "$descriptor" 2>/dev/null || :)
  [ -n "$target" ] || continue
  if [ -b "$descriptor" ];then echo "LAB_BLOCK_DESCRIPTOR_FOUND $process $target";exit 40;fi
  case "$target" in /dev/fb*|/dev/input/*|/dev/mmcblk*|/dev/mem|/dev/kmem)
   echo "LAB_HOST_DEVICE_DESCRIPTOR_FOUND $process $target";exit 41;;
  esac
 done
done
[ "$count" -ge 5 ] && [ "$app_seen" = 1 ] || { echo "LAB_DESCRIPTOR_AUDIT_MISSING_PROCESSES count=$count app=$app_seen";exit 42; }
echo "LAB_RUNTIME_DESCRIPTOR_AUDIT_OK processes=$count app_uid=10000"
