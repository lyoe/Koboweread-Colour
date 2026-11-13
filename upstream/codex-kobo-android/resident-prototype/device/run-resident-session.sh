#!/bin/sh
# Resident launcher session: the fast session below plus the resident
# components (launcher HOME, focus kind, input helper, display/touch/network
# limits and the resident loop), all bound over the image only for this session.
# Native-resolution session with speed settings layered on top of the verified
# run-native-session.sh flow. Nothing in the image is replaced: overrides are
# bind mounts and sysfs values that are undone on exit.
#  - CPU governor "performance" for the session (restored afterwards).
#  - Drop the vfat-side page cache of graphics.img while mounted, so pages
#    read through ext2-on-loop are cached once instead of twice.
#  - JIT shim that lets WeRead (UID 10007) save its hot-method profile.
#  - Optional speed-profile odex compiled from that profile (lab/fast/), or
#    the lab/fast/oat-arm/ directory holding that odex plus an app image.
set -eu
base=/mnt/onboard/.adds/kobo-android-lab
fast=$base/fast
res=$base/resident
root=/tmp/kobo-android-runtime
image=$base/graphics.img
cd "$base"
[ "$(uname -r)" = '4.1.15-00838-g349fbf5767c' ] || { echo 'Kernel mismatch'; exit 2; }
[ -f "$image" ] || { echo 'Lab image is missing'; exit 3; }
if awk -v root="$root" '$2==root {found=1} END {exit !found}' /proc/mounts; then
 echo 'Lab image is already mounted; refusing a concurrent run'; exit 4
fi
inner=$root/lab/native-resolution/run-native-weread.sh
fblib=$root/lab/native-resolution/libkobo_fb.so
bridge=$root/lab/native-resolution/live-display
entry=$root/lab/native-resolution/weread-process-entry.dex
hwdir=$root/system/lib/hw
odex=$root/data/app/com.tencent.weread.eink-1/oat/arm/base.odex
gov=/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
mounted=0;loopdev=;native_selected=0;created_lib=0;created_arm=0
res_bound=;old_gov=;jit_bound=0;odex_bound=0;inner_bound=0;fblib_bound=0;bridge_bound=0;entry_bound=0;hwdir_bound=0;dropper=;pc_loaded=0;pc_node=/tmp/kobo_pagecache
cleanup() {
 code=$?
 trap - EXIT INT TERM
 if [ -n "$dropper" ];then kill "$dropper" 2>/dev/null || :;wait "$dropper" 2>/dev/null || :;fi
 if [ "$pc_loaded" = 1 ];then rm -f "$pc_node";rmmod kobo_pagecache && echo FAST_PAGECACHE_MODULE_UNLOADED || { echo FAST_PAGECACHE_MODULE_UNLOAD_FAILED;code=106; };fi
 if [ -n "$old_gov" ];then echo "$old_gov" > "$gov" 2>/dev/null && echo "FAST_GOVERNOR_RESTORED=$old_gov" || echo FAST_GOVERNOR_RESTORE_FAILED;fi
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
 for target in $res_bound;do umount "$target" || { echo "RESIDENT_UNBIND_FAILED target=$target";code=106; };done
 [ "$hwdir_bound" != 1 ] || umount "$hwdir" || { echo FAST_HWDIR_UNBIND_FAILED;code=106; }
 [ "$entry_bound" != 1 ] || umount "$entry" || { echo FAST_ENTRY_UNBIND_FAILED;code=106; }
 [ "$bridge_bound" != 1 ] || umount "$bridge" || { echo FAST_BRIDGE_UNBIND_FAILED;code=106; }
 [ "$fblib_bound" != 1 ] || umount "$fblib" || { echo FAST_FBLIB_UNBIND_FAILED;code=106; }
 [ "$inner_bound" != 1 ] || umount "$inner" || { echo FAST_INNER_UNBIND_FAILED;code=106; }
 [ "$odex_bound" != 1 ] || umount "$odex_mount" || { echo FAST_ODEX_UNBIND_FAILED;code=106; }
 [ "$jit_bound" != 1 ] || umount "$root/lab/libkobo_jit_config.so" || { echo FAST_JIT_UNBIND_FAILED;code=106; }
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

if [ "${KOBO_FAST_GOVERNOR:-1}" = 1 ] && [ -w "$gov" ];then
 old_gov=$(cat "$gov")
 echo performance > "$gov"
 echo "FAST_GOVERNOR=$(cat "$gov") previous=$old_gov"
fi
# Drop graphics.img's vfat-side page cache every 3 s: ext2-on-loop otherwise
# caches each page twice. kobo_pagecache only drops clean, unmapped pages.
if [ "${KOBO_FAST_DROP_LOOP_CACHE:-1}" = 1 ] && [ ! -e "$fast/pagecache-disabled" ] && ! grep -q '^kobo_pagecache ' /proc/modules;then
 [ "$(sha256sum "$fast/kobo_pagecache.ko" | awk '{print $1}')" = "$(cat "$fast/kobo_pagecache.ko.sha256")" ] || { echo FAST_PAGECACHE_HASH_MISMATCH;exit 105; }
 insmod "$fast/kobo_pagecache.ko";pc_loaded=1
 pc_dev=$(cat /sys/class/misc/kobo_pagecache/dev)
 rm -f "$pc_node";mknod "$pc_node" c "${pc_dev%:*}" "${pc_dev#*:}";chmod 0600 "$pc_node"
 "$fast/loop-cache" drop "$image" 3 2592000 $$ "$pc_node" > "$fast/loop-cache.log" 2>&1 &
 dropper=$!
 echo FAST_LOOP_CACHE_DROPPER_STARTED module=1
fi
if [ "${KOBO_FAST_JIT_PROFILES:-1}" = 1 ];then
 # libkobo_jit_config.so: profiles + heap headroom; -noheap: profiles only (A/B).
 shim=$fast/libkobo_jit_config.so
 [ ! -e "$fast/heap-disabled" ] || shim=$fast/libkobo_jit_config-noheap.so
 [ -f "$shim" ] && [ "$(sha256sum "$shim" | awk '{print $1}')" = "$(cat "$shim.sha256")" ] || { echo FAST_JIT_SHIM_HASH_MISMATCH;exit 105; }
 mount --bind "$shim" "$root/lab/libkobo_jit_config.so"
 echo "FAST_JIT_SHIM=${shim##*/}"
 jit_bound=1
 echo FAST_JIT_PROFILE_SHIM_BOUND
fi
candidate=$root/lab/fast/weread-speed-profile.odex
imagedir=$root/lab/fast/oat-arm
odex_mount=$odex
sha_ok(){ [ -f "$1" ] && [ -f "$1.sha256" ] && [ "$(sha256sum "$1" | awk '{print $1}')" = "$(cat "$1.sha256")" ]; }
if [ "${KOBO_FAST_ODEX:-1}" = 1 ] && [ ! -e "$fast/odex-disabled" ] && [ ! -e "$fast/appimage-disabled" ] && sha_ok "$imagedir/base.odex" && sha_ok "$imagedir/base.art";then
 odex_mount=${odex%/base.odex}
 mount --bind "$imagedir" "$odex_mount"
 odex_bound=1
 echo FAST_SPEED_PROFILE_APP_IMAGE_DIR_BOUND
elif [ "${KOBO_FAST_ODEX:-1}" = 1 ] && [ ! -e "$fast/odex-disabled" ] && [ -f "$candidate" ] && [ -f "$candidate.sha256" ];then
 if [ "$(sha256sum "$candidate" | awk '{print $1}')" = "$(cat "$candidate.sha256")" ];then
  mount --bind "$candidate" "$odex"
  odex_bound=1
  echo FAST_SPEED_PROFILE_ODEX_BOUND
 else
  echo FAST_SPEED_PROFILE_ODEX_HASH_MISMATCH_USING_ORIGINAL
 fi
fi
# Animation-scale helper dex, used by the fast inner script.
if [ -f "$fast/animation-scales.dex" ] && [ "$(sha256sum "$fast/animation-scales.dex" | awk '{print $1}')" = "$(cat "$fast/animation-scales.dex.sha256")" ];then
 mkdir -p "$root/lab/fast";cp "$fast/animation-scales.dex" "$root/lab/fast/animation-scales.dex";chmod 0644 "$root/lab/fast/animation-scales.dex"
fi
# Inner script copy: harness Java helpers without -Xint (ART interpret-only
# mode also interprets the AOT boot image) and readiness polls for sleeps.
[ "$(sha256sum "$fast/audit-runtime-fds" | awk '{print $1}')" = "$(cat "$fast/audit-runtime-fds.sha256")" ] || { echo FAST_AUDIT_HASH_MISMATCH;exit 105; }
res_sha_ok(){ [ -f "$res/$1" ] && [ ! -L "$res/$1" ] && [ "$(sha256sum "$res/$1" | awk '{print $1}')" = "$(cat "$res/$1.sha256")" ]; }
for f in resident-loop.sh resident-sched resident-dns-check dns-proxy-resident;do res_sha_ok "$f" || { echo "RESIDENT_HASH_MISMATCH file=$f";exit 105; };done
res_bind(){ # file target
 res_sha_ok "$1" || { echo "RESIDENT_HASH_MISMATCH file=$1";exit 105; }
 [ -f "$2" ] && [ ! -L "$2" ] || { echo "RESIDENT_TARGET_MISSING target=$2";exit 105; }
 mount --bind "$res/$1" "$2";res_bound="$2 $res_bound"
}
res_sha_ok run-native-weread-resident.sh || { echo RESIDENT_INNER_HASH_MISMATCH;exit 105; }
mount --bind "$res/run-native-weread-resident.sh" "$inner"
inner_bound=1
echo RESIDENT_INNER_SCRIPT_BOUND no_xint=1 readiness_polls=1
res_bind install-framework-probe-resident.dex "$root/lab/install-framework-probe.dex"
res_bind KoboProbe-resident.apk "$root/system/app/KoboProbe/KoboProbe.apk"
res_bind host-default-network-resident "$root/lab/host-default-network"
res_bind touch-forward-resident "$root/lab/native-resolution/touch-forward"
echo RESIDENT_COMPONENTS_BOUND

# Codex's display shim rebuilt to export only ioctl (the original also exports
# a slower memcpy that SurfaceFlinger ends up using). Same geometry and code.
# HWC (on unless fast/hwc-disabled exists): hwcomposer.default copies a lone full-screen layer straight into
# the virtual framebuffer instead of software GLES composition. It needs the
# shim variant that reports a single buffer to SurfaceFlinger (and is otherwise
# the slim shim); both are bound together or not at all.
if [ ! -e "$fast/hwc-disabled" ];then
 for f in libkobo_fb-hwc.so hwc-lib/hwcomposer.default.so;do
  [ "$(sha256sum "$fast/$f" | awk '{print $1}')" = "$(cat "$fast/$f.sha256")" ] || { echo FAST_HWC_HASH_MISMATCH file=$f;exit 105; }
 done
 for f in $(ls "$hwdir");do cmp -s "$hwdir/$f" "$fast/hwc-lib/$f" || { echo FAST_HWC_LIB_COPY_MISMATCH file=$f;exit 105; };done
 mount --bind "$fast/libkobo_fb-hwc.so" "$fblib"
 fblib_bound=1
 mount --bind "$fast/hwc-lib" "$hwdir"
 hwdir_bound=1
 echo FAST_HWC_BOUND
elif [ ! -e "$fast/fbslim-disabled" ];then
 [ "$(sha256sum "$fast/libkobo_fb-slim.so" | awk '{print $1}')" = 4a18b21a7c57817d43b251336d48c98cde5eeb3b7ec5c23548bb0188ae1bc18f ] || { echo FAST_FBLIB_HASH_MISMATCH;exit 105; }
 mount --bind "$fast/libkobo_fb-slim.so" "$fblib"
 fblib_bound=1
 echo FAST_SLIM_FB_LIBRARY_BOUND
fi

# Display bridge: mmap reads, NEON luma, changed-rectangle partial updates.
res_sha_ok live-display-resident || { echo RESIDENT_BRIDGE_HASH_MISMATCH;exit 105; }
mount --bind "$res/live-display-resident" "$bridge"
bridge_bound=1
echo RESIDENT_DISPLAY_BRIDGE_BOUND

# Input helper: page keys injected one at a time with WAIT_FOR_FINISH; keys and
# new gestures arriving while one is unfinished are dropped, not queued.
res_sha_ok weread-process-entry-resident.dex || { echo RESIDENT_ENTRY_HASH_MISMATCH;exit 105; }
mount --bind "$res/weread-process-entry-resident.dex" "$entry"
entry_bound=1
echo RESIDENT_INPUT_HELPER_BOUND

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
KOBO_RESIDENT_GOVERNOR="$old_gov" KOBO_WEBVIEW_SOFTWARE_SURFACE=1 "$root/lab/fd-clean-exec" /bin/sh "$root/lab/native-resolution/run-native-weread.sh"
