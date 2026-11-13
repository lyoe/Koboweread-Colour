#!/bin/sh
# Test the pinned CPU synchronous WebView output component; restore original APK-based loading on exit.
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
mounted=0;loopdev=;native_selected=0;created_lib=0;created_arm=0
cleanup() {
 code=$?
 trap - EXIT INT TERM
 if [ "$native_selected" = 1 ];then
  if mv "$root/system/app/webview/lib/arm/libwebviewchromium.so" "$root/lab/libwebviewchromium-software-surface.so";then
   echo ORIGINAL_APK_WEBVIEW_NATIVE_LOADING_RESTORED
  else
   echo WEBVIEW_NATIVE_COMPONENT_RESTORE_FAILED
   code=100
  fi
  [ "$created_arm" != 1 ] || rmdir "$root/system/app/webview/lib/arm" 2>/dev/null || :
  [ "$created_lib" != 1 ] || rmdir "$root/system/app/webview/lib" 2>/dev/null || :
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
[ "${KOBO_WEREAD_REUSE:-0}" = 1 ] && [ "${KOBO_WEREAD_HOST_NETWORK:-0}" = 1 ] && { [ "${KOBO_WEREAD_AGREEMENT_DIAG:-0}" = 1 ] || [ "${KOBO_WEREAD_INTERACTIVE:-0}" = 1 ]; } || { echo SOFTWARE_SURFACE_TEST_REQUIRES_AGREEMENT_OR_INTERACTIVE_SESSION;exit 101; }
component="$root/lab/libwebviewchromium-software-surface.so"
[ -f "$component" ] && [ ! -L "$component" ] || exit 102
[ "$(sha256sum "$component" | awk '{print $1}')" = d39f571b5c538bf20cca6f37637dfb5dd234da28d66ec7a99db19fa1bf9820b3 ] || { echo SOFTWARE_SURFACE_COMPONENT_HASH_MISMATCH;exit 103; }
[ ! -L "$root/system/app/webview/lib" ] && [ ! -L "$root/system/app/webview/lib/arm" ] && [ ! -e "$root/system/app/webview/lib/arm/libwebviewchromium.so" ] || { echo EXISTING_EXTERNAL_WEBVIEW_COMPONENT;exit 104; }
[ -d "$root/system/app/webview/lib" ] || created_lib=1
[ -d "$root/system/app/webview/lib/arm" ] || created_arm=1
mkdir -p "$root/system/app/webview/lib/arm"
# The private menu launcher uses umask 077 for its logs. Native library
# directories must remain traversable by the real application UID.
chmod 0755 "$root/system/app/webview/lib" "$root/system/app/webview/lib/arm"
mv "$component" "$root/system/app/webview/lib/arm/libwebviewchromium.so"
native_selected=1
chmod 0644 "$root/system/app/webview/lib/arm/libwebviewchromium.so"
echo PATCHED_WEBVIEW_CPU_SURFACE_COMPONENT_SELECTED
KOBO_WEBVIEW_SOFTWARE_SURFACE=1 "$root/lab/fd-clean-exec" /bin/sh "$root/lab/run-weread.sh"
