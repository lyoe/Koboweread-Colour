#!/bin/sh
# Input: prepared graphics-root tar.gz on stdin. Formats only a new lab image FILE.
set -eu
base=/mnt/onboard/.adds/kobo-android-lab
root=/tmp/kobo-android-runtime
image=$base/graphics.building.img
[ "$(uname -r)" = '4.1.15-00286-g2a6a4cb08c66' ] || exit 2
[ ! -e "$image" ] && [ ! -e "$base/graphics.img" ] || { echo 'Graphics image already exists; refusing overwrite';exit 3; }
if awk -v root="$root" '$2==root {found=1} END {exit !found}' /proc/mounts;then echo 'Lab is busy';exit 4;fi
free=$(df -k /mnt/onboard | awk 'NR==2 {print $4}')
[ "$free" -ge 655360 ] || { echo 'Insufficient storage for the graphics experiment';exit 5; }
mounted=0;loopdev=
cleanup(){
 code=$?
 trap - EXIT INT TERM
 if [ "$mounted" = 1 ];then
  if umount "$root";then losetup -d "$loopdev" 2>/dev/null || :;else echo 'Stage mount requires inspection';[ "$code" -ne 0 ] || code=6;fi
 fi
 exit "$code"
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
mkdir -p "$base" "$root"
dd if=/dev/zero of="$image" bs=1048576 count=384
busybox mke2fs -F -b 4096 -m 0 "$image"
mount -t ext2 -o loop,noatime "$image" "$root"
mounted=1
loopdev=$(awk -v root="$root" '$2==root {print $1}' /proc/mounts)
tar -xzf - -C "$root"
[ -f "$root/lab/canvas-probe.dex" ] && [ -x "$root/system/bin/app_process32" ]
sync
umount "$root"
mounted=0
losetup -d "$loopdev" 2>/dev/null || :
mv "$image" "$base/graphics.img"
echo GRAPHICS_IMAGE_STAGED
