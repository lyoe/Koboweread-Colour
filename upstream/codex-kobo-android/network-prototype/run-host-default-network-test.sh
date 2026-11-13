#!/bin/sh
# Bounded host observation/protocol test; does not start Android or touch the UI.
set -eu
base=/mnt/onboard/.adds/kobo-android-lab
root=/tmp/kobo-android-runtime
mountpoint -q "$root" && { echo LAB_ALREADY_MOUNTED;exit 2; }
mounted=0;daemon=;loopdev=
cleanup(){
 code=$?;trap - EXIT INT TERM;set +e
 if [ -n "$daemon" ];then kill "$daemon" 2>/dev/null || :;wait "$daemon" 2>/dev/null || :;fi
 if [ "$mounted" = 1 ];then
  rm -f "$root/dev/socket/fwmarkd" "$root/lab/network-state/host-default.properties" "$root/lab/network-state/pending"
  [ ! -d "$root/lab/network-state" ] || rmdir "$root/lab/network-state"
  sync
  if umount "$root";then [ -z "$loopdev" ] || losetup -d "$loopdev" 2>/dev/null || :;else code=9;fi
 fi
 exit "$code"
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
mkdir -p "$root"
mount -t ext2 -o loop,noatime,user_xattr "$base/graphics.img" "$root"
mounted=1
loopdev=$(awk -v root="$root" '$2==root {print $1}' /proc/mounts)
[ ! -e "$root/lab/network-state" ] && [ ! -L "$root/lab/network-state" ] || exit 3
[ ! -e "$root/dev/socket/fwmarkd" ] && [ ! -L "$root/dev/socket/fwmarkd" ] || exit 4
"$base/host-default-network" > "$root/lab/host-default-network.log" 2>&1 &
daemon=$!
attempt=0
while [ ! -f "$root/lab/network-state/host-default.properties" ];do
 kill -0 "$daemon" 2>/dev/null || { cat "$root/lab/host-default-network.log";exit 5; }
 attempt=$((attempt+1));[ "$attempt" -lt 5 ] || exit 6;sleep 1
done
echo ACTUAL_HOST_DEFAULT_NETWORK_OBSERVATION
cat "$root/lab/network-state/host-default.properties"
echo HOST_OBSERVER_PROCESS_IDENTITY
awk '/^(Uid|Gid|Groups|CapEff|NoNewPrivs):/' "/proc/$daemon/status"
"$base/host-default-network-probe"
cat "$root/lab/host-default-network.log"
echo HOST_DEFAULT_NETWORK_TEST_OK
