#!/bin/sh
# One-variable WebView compositor comparison; restores the exact private flag file on exit.
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
mounted=0;loopdev=;flags_saved=0
cleanup() {
 code=$?
 trap - EXIT INT TERM
 if [ "$flags_saved" = 1 ];then
  if mv "$root/lab/webview-command-line.before-comparison" "$root/data/local/tmp/webview-command-line";then
   echo COMPOSITOR_COMPARISON_ORIGINAL_FLAGS_RESTORED
  else
   echo COMPOSITOR_COMPARISON_FLAG_RESTORE_FAILED
   code=94
  fi
 fi
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
[ "${KOBO_WEREAD_REUSE:-0}" = 1 ] && [ "${KOBO_WEREAD_DRAW_DIAGNOSTIC:-0}" = 1 ] || { echo COMPOSITOR_COMPARE_REQUIRES_KNOWN_DRAW_DIAGNOSTIC;exit 92; }
flags="$root/data/local/tmp/webview-command-line"
baseline='webview --skip-gpu-data-loading --disable-gpu --disable-gpu-compositing --disable-gpu-rasterization --disable-accelerated-2d-canvas --disable-webgl'
[ -f "$flags" ] && [ ! -L "$flags" ] && [ "$(cat "$flags")" = "$baseline" ] || { echo UNEXPECTED_COMPOSITOR_BASELINE;exit 93; }
[ ! -e "$root/lab/webview-command-line.before-comparison" ] || { echo PREVIOUS_COMPOSITOR_BACKUP_EXISTS;exit 95; }
cp -p "$flags" "$root/lab/webview-command-line.before-comparison"
flags_saved=1
printf '%s\n' 'webview --skip-gpu-data-loading --disable-gpu --disable-gpu-rasterization --disable-accelerated-2d-canvas --disable-webgl' > "$flags"
echo COMPOSITOR_COMPARE_ONLY_GPU_COMPOSITING_DISABLE_REMOVED
"$root/lab/fd-clean-exec" /bin/sh "$root/lab/run-weread.sh"
