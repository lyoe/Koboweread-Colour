set -eu
base=/mnt/onboard/.adds/kobo-android-lab
root=/tmp/kobo-android-runtime
[ -x "$base/screen-probe" ] || exit 2
mountpoint -q "$root" && exit 3
mkdir -p "$root"
mount -t ext2 -o loop,ro,user_xattr "$base/graphics.img" "$root"
mounted=1;reader_pids=;stopped_pids=
cleanup(){
 code=$?
 trap - EXIT INT TERM
 for reader_pid in $stopped_pids;do kill -CONT "$reader_pid" 2>/dev/null || :;done
 [ "$mounted" != 1 ] || umount "$root"
 exit "$code"
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
# Only this experiment's generated image is copied; no original reader image saved.
cp "$root/lab/weread-input-reopened.png" "$base/weread-login.png"
umount "$root";mounted=0
for p in /proc/[0-9]*;do
 [ "$(readlink "$p/exe" 2>/dev/null || :)" = /mnt/onboard/.adds/koreader/luajit ] || continue
 [ "$(cat "$p/comm" 2>/dev/null || :)" = reader.lua ] || continue
 [ "$(awk '$1=="State:" {print $2}' "$p/status")" = S ] || { echo KOREADER_NOT_SLEEPING;exit 4; }
 reader_pids="$reader_pids ${p##*/}"
done
set -- $reader_pids
[ "$#" = 2 ] || { echo UNEXPECTED_KOREADER_PROCESS_COUNT;exit 5; }
parent_a=$(awk '$1=="PPid:" {print $2}' "/proc/$1/status")
parent_b=$(awk '$1=="PPid:" {print $2}' "/proc/$2/status")
[ "$parent_a" = "$2" ] || [ "$parent_b" = "$1" ] || { echo UNEXPECTED_KOREADER_PROCESS_TREE;exit 6; }
echo "VERIFIED_KOREADER_UI_PIDS=$reader_pids"
for reader_pid in $reader_pids;do
 [ "$(readlink "/proc/$reader_pid/exe")" = /mnt/onboard/.adds/koreader/luajit ] || exit 7
 kill -STOP "$reader_pid"
 stopped_pids="$stopped_pids $reader_pid"
done
"$base/screen-probe" "$base/weread-login.png" 5
for reader_pid in $stopped_pids;do kill -CONT "$reader_pid";done
stopped_pids=
echo KOREADER_RESUMED_AFTER_WEREAD_LOGIN_SCREEN
