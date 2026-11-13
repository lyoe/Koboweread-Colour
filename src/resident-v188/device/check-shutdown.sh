#!/bin/sh
# Device-side, read-only: after a KOReader exit/restart, USB storage, power-off
# or reboot with the resident Android running, show whether it was shut down
# cleanly (deploy as resident/check-shutdown.sh).
#   ext2/ext4 s_state 1 = cleanly unmounted (cleared while mounted, set on umount).
b=/mnt/onboard/.adds/kobo-android-lab; img=$b/graphics-v188.img
u16(){ dd if=$img bs=1 skip=$1 count=2 2>/dev/null | od -An -tu2 | tr -d ' '; }
echo "IMAGE magic=$(dd if=$img bs=1 skip=1080 count=2 2>/dev/null | od -An -tx2 | tr -d ' ') state=$(u16 1082) mount_count=$(u16 1076)"
echo "NOW uptime=$(cut -d' ' -f1 /proc/uptime) resident=$(sh $b/resident-v188/resident-ctl.sh status) lock=$([ -d $b/weread-launch.lock ] && echo held || echo free)"
echo "CLEAN mounts=$(grep -c kobo-android /proc/mounts) loop_image=$(grep -c 'graphics.*\.img' /proc/mounts) modules=$(grep -cE '^(binder_linux|ashmem_linux|kobo_madvise|kobo_pagecache) ' /proc/modules) gov=$(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor)"
echo "## launch state"; cat $b/weread-launch.state $b/resident-state 2>/dev/null | tr '\n' ' '; echo
echo "## KOReader hook log"; grep -a 'WeReadLab resident\|exit code' /mnt/onboard/.adds/koreader/crash.log | tail -6
for f in $b/resident-v188-launch-private.log $b/resident-v188-launch-private.log.previous;do
 [ -f "$f" ] || continue
 echo "## ${f##*/} ($(date -r "$f" '+%m-%d %H:%M:%S' 2>/dev/null))"
 grep -a 'RESIDENT_SHUTDOWN\|RESIDENT_KOREADER_GONE\|WEREAD_RESIDENT_SESSION_ENDED\|RESTORE_BYTES_MATCH\|MODULE_UNLOADED\|GOVERNOR_RESTORED\|remains mounted\|UNBIND_FAILED\|UNLOAD_FAILED' "$f" | tail -8
done
