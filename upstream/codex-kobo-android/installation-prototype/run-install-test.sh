#!/bin/sh
# Runs the existing staged lab image; installs only the fixed local test APK in the private Android image.
set -eu
base=/mnt/onboard/.adds/kobo-android-lab
root=/tmp/kobo-android-runtime
image=$base/graphics.img
cd "$base"
[ "$(uname -r)" = '4.1.15-00286-g2a6a4cb08c66' ] || { echo 'Kernel mismatch'; exit 2; }
[ -f "$image" ] || { echo 'Lab image is missing'; exit 3; }
if awk -v root="$root" '$2==root {found=1} END {exit !found}' /proc/mounts; then
 echo 'Lab image is already mounted; refusing a concurrent run'; exit 4
fi
mounted=0;loopdev=
cleanup() {
 code=$?
 trap - EXIT INT TERM
 if [ "$mounted" = 1 ]; then
  attempt=0
  sync
  while ! umount "$root" 2>/dev/null; do
   attempt=$((attempt + 1))
   [ "$attempt" -lt 3 ] || break
   sleep 1
  done
  if ! awk -v root="$root" '$2==root {found=1} END {exit !found}' /proc/mounts; then
   if [ -n "$loopdev" ];then losetup -d "$loopdev" 2>/dev/null || :;fi
  else
   echo 'Lab image remains mounted; inspect running lab processes before unmounting.'
   [ "$code" -ne 0 ] || code=5
  fi
 fi
 exit "$code"
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
mkdir -p "$root"
mount -t ext2 -o loop,noatime,user_xattr "$image" "$root"
mounted=1
loopdev=$(awk -v root="$root" '$2==root {print $1}' /proc/mounts)
"$root/lab/fd-clean-exec" /bin/sh "$root/lab/run-install.sh"
