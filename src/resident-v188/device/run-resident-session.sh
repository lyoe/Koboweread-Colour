#!/bin/sh
# Resident launcher session: the fast session below plus the resident
# components (launcher HOME, focus kind, input helper, display/touch/network
# limits and the resident loop), all bound over the image only for this session.
# Native-resolution session with speed settings layered on top of the verified
# run-native-session.sh flow. Nothing in the image is replaced: overrides are
# bind mounts and sysfs values that are undone on exit.
#  - CPU governor "performance" for the session (restored afterwards).
#  - Drop the vfat-side page cache of graphics.img while mounted, so pages
#    read through the image on loop are cached once instead of twice.
#  - JIT shim that lets WeRead (UID 10007) save its hot-method profile.
#  - Optional speed-profile odex compiled from that profile (lab/fast/), or
#    the lab/fast/oat-arm/ directory holding that odex plus an app image.
set -eu
base=/mnt/onboard/.adds/kobo-android-lab
fast=$base/fast-v188
res=$base/resident-v188
root=/tmp/kobo-android-runtime
# WeRead 1.8.8 launcher, kept apart from the 1.9.8 one (resident/, fast/,
# graphics-v198.img): its own image graphics-v188.img, framework probe (that
# APK's digest, downgrade allowed), files under resident-v188/ and fast-v188/,
# state files resident-v188-state and v188-launch.state. Only the session
# lock weread-launch.lock is shared: one Android at a time.
image=$base/graphics-v188.img
image_tag=v188
probe=install-framework-probe-resident${image_tag:+-$image_tag}.dex
echo "RESIDENT_IMAGE ${image##*/}"
cd "$base"
# Kernel modules per kernel release: $base/kernels/<uname -r>/ holds the six
# modules this session may load, SHA256SUMS ("hash  name.ko") and status
# (device-verified, or untested: vermagic-patched, symbol CRCs checked on the
# Mac only; docs/固件兼容.md). No set for this kernel: refuse as before.
krel=$(uname -r);kmods=$base/kernels/$krel
[ -f "$kmods/SHA256SUMS" ] && [ ! -L "$kmods" ] || { echo "Kernel mismatch: no module set for $krel"; exit 2; }
kmod_ok(){ [ -f "$kmods/$1.ko" ] && [ ! -L "$kmods/$1.ko" ] && [ "$(sha256sum "$kmods/$1.ko" | awk '{print $1}')" = "$(awk -v n="$1.ko" '$2==n {print $1}' "$kmods/SHA256SUMS")" ]; }
echo "RESIDENT_KERNEL release=$krel modules=$(cat "$kmods/status" 2>/dev/null || echo unknown)"
[ -f "$image" ] || { echo 'Lab image is missing'; exit 3; }
if awk -v root="$root" '$2==root {found=1} END {exit !found}' /proc/mounts; then
 echo 'Lab image is already mounted; refusing a concurrent run'; exit 4
fi
inner=$root/lab/native-resolution/run-native-weread.sh
fblib=$root/lab/native-resolution/libkobo_fb.so
bridge=$root/lab/native-resolution/live-display
entry=$root/lab/native-resolution/weread-process-entry.dex
watchdog=$root/lab/native-resolution/native-memory-watchdog.sh
hwdir=$root/system/lib/hw
gov=/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
mounted=0;loopdev=;native_selected=0;created_lib=0;created_arm=0
res_bound=;boot_bound=;old_gov=;zram_loaded=0;zram_on=0;old_swappiness=;zram_node=/tmp/kobo-zram0;jit_bound=0;odex_bound=0;inner_bound=0;fblib_bound=0;bridge_bound=0;entry_bound=0;hwdir_bound=0;watchdog_bound=0;dropper=;pc_loaded=0;pc_node=/tmp/kobo_pagecache
# PackageManagerService treats a system package as changed when its directory's
# mtime differs from the ft it recorded, and then digests the whole 44.8 MB
# WebView APK again (signature scheme v2, 2.8 s of every start). Adding and
# removing lib/ below changes that mtime; set it back to the recorded value
# (resident/webviewtime-disabled: leave it).
webview_time(){
 [ ! -e "$res/webviewtime-disabled" ] || return 0
 wv_ft=$(sed -n 's/.*<package name="com.android.webview" [^>]*ft="\([0-9a-f]*\)".*/\1/p' "$root/data/system/packages.xml" 2>/dev/null | head -n 1)
 [ -n "$wv_ft" ] && touch -d "@$((0x$wv_ft / 1000))" "$root/system/app/webview" && echo "RESIDENT_WEBVIEW_TIME ft=$wv_ft"
}
cleanup() {
 code=$?
 trap - EXIT INT TERM
 if [ -n "${prefetch:-}" ];then kill "$prefetch" 2>/dev/null || :;wait "$prefetch" 2>/dev/null || :;fi
 if [ -n "$dropper" ];then kill "$dropper" 2>/dev/null || :;wait "$dropper" 2>/dev/null || :;fi
 if [ -n "${io_old:-}" ];then set -- $io_old;echo "$1" > /sys/block/mmcblk0/queue/scheduler;echo "$2" > /sys/block/mmcblk0/queue/read_ahead_kb;echo "FAST_IO_TUNE_RESTORED scheduler=$1 sd_ra=$2";fi
 if [ "$zram_on" = 1 ];then swapoff "$zram_node" && echo FAST_ZRAM_SWAP_OFF || { echo FAST_ZRAM_SWAPOFF_FAILED;code=106; };fi
 [ -z "$old_swappiness" ] || echo "$old_swappiness" > /proc/sys/vm/swappiness
 if [ "$zram_loaded" = 1 ];then
  echo 1 > /sys/block/zram0/reset 2>/dev/null || :;rm -f "$zram_node"
  rmmod kobo_zram && rmmod kobo_zsmalloc && echo FAST_ZRAM_MODULES_UNLOADED || { echo FAST_ZRAM_UNLOAD_FAILED;code=106; }
 fi
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
  webview_time
 fi
 for target in $res_bound;do umount "$target" || { echo "RESIDENT_UNBIND_FAILED target=$target";code=106; };done
 [ "$watchdog_bound" != 1 ] || umount "$watchdog" || { echo FAST_WATCHDOG_UNBIND_FAILED;code=106; }
 [ "$hwdir_bound" != 1 ] || umount "$hwdir" || { echo FAST_HWDIR_UNBIND_FAILED;code=106; }
 [ "$entry_bound" != 1 ] || umount "$entry" || { echo FAST_ENTRY_UNBIND_FAILED;code=106; }
 [ "$bridge_bound" != 1 ] || umount "$bridge" || { echo FAST_BRIDGE_UNBIND_FAILED;code=106; }
 [ "$fblib_bound" != 1 ] || umount "$fblib" || { echo FAST_FBLIB_UNBIND_FAILED;code=106; }
 [ "$inner_bound" != 1 ] || umount "$inner" || { echo FAST_INNER_UNBIND_FAILED;code=106; }
 [ "$odex_bound" != 1 ] || umount "$odex_mount" || { echo FAST_ODEX_UNBIND_FAILED;code=106; }
 for target in $boot_bound;do umount "$target" || { echo FAST_BOOT_NODEBUG_UNBIND_FAILED;code=106; };done
 [ "$jit_bound" != 1 ] || umount "$root/lab/libkobo_jit_config.so" || { echo FAST_JIT_UNBIND_FAILED;code=106; }
 if [ "$mounted" = 1 ]; then
  attempt=0
  sync
  df -Pk "$root" | awk 'NR==2 {print $4}' > "$res/image-free-kb" 2>/dev/null || :
  while ! umount "$root" 2>/dev/null; do
   attempt=$((attempt + 1))
   [ "$attempt" -lt 3 ] || break
   sleep 1
  done
  if ! awk -v root="$root" '$2==root {found=1} END {exit !found}' /proc/mounts; then
   if [ -n "$loopdev" ];then losetup -d "$loopdev" 2>/dev/null || :;fi
   rm -f "$res/session-dirty"
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
# Unclean end of the previous session (2026-11-08: the reader restarted while
# Android ran, so cleanup never ran; the image file system was left inconsistent, and run
# markers, sockets and the WebView component stayed in the image, so the next
# start failed with exit 102/121). resident-v188/session-dirty is created
# right after the mount and removed only after a clean unmount. When it is
# found here: check the image (e2fsck -fp, about 5 s) and, once mounted,
# remove what the old session left. Off with resident-v188/precheck-disabled.
recover=0
if [ -e "$res/session-dirty" ] && [ ! -e "$res/precheck-disabled" ];then
 echo RESIDENT_PREVIOUS_SESSION_UNCLEAN
 if [ ! -e "$res/e2fsck" ];then echo RESIDENT_IMAGE_FSCK_SKIPPED_NO_TOOL
 else
  [ -f "$res/e2fsck" ] && [ ! -L "$res/e2fsck" ] && [ "$(sha256sum "$res/e2fsck" | awk '{print $1}')" = "$(cat "$res/e2fsck.sha256")" ] || { echo "RESIDENT_HASH_MISMATCH file=e2fsck";exit 105; }
  fsck_code=0;"$res/e2fsck" -fp "$image" > "$res/fsck-last.log" 2>&1 || fsck_code=$?
  sed 's/^/RESIDENT_FSCK_OUTPUT /' "$res/fsck-last.log"
  echo "RESIDENT_IMAGE_FSCK code=$fsck_code"
  # 0 clean, 1/2 errors fixed; 4 and above need a manual e2fsck -fy (keep a copy first).
  [ "$fsck_code" -lt 4 ] || { echo RESIDENT_IMAGE_FSCK_NEEDS_MANUAL_REPAIR;exit 150; }
 fi
 recover=1
fi
# Image file system (2026-11-10): images are built as ext4 with a journal;
# older images without one (has_journal, bit 2 of the compat word at byte
# 1116) are still mounted as ext2.
img_fstype(){ j_=$(dd if="$1" bs=1 skip=1116 count=1 2>/dev/null | od -An -tu1 | tr -d ' ');[ $(( ${j_:-0} & 4 )) = 0 ] && echo ext2 || echo ext4; }
mkdir -p "$root"
fstype=$(img_fstype "$image");echo "RESIDENT_IMAGE_FS type=$fstype"
mount -t $fstype -o loop,noatime,user_xattr "$image" "$root"
mounted=1
: > "$res/session-dirty"
if [ "$recover" = 1 ];then
 # Everything the session scripts create in the image and remove on exit;
 # the inner script refuses to start while any of them exists.
 left=
 for f in lab/weread-heap-headroom-enabled lab/display-cadence15-enabled lab/framework-jit-enabled lab/weread-jit-enabled lab/weread-startup-service-grace lab/interactive-session-ready lab/interactive-refresh-request lab/interactive-session-finish lab/webview-read-diagnostics-enabled lab/webview-software-draw-enabled lab/webview-trace-started lab/webview-trace-complete lab/host-default-network-enabled lab/network-state lab/install-processes.pid lab/app-probe.pid lab/resident data/local/tmp/kobo-resident dev/socket/kobo_touch_probe dev/socket/zygote dev/socket/dnsproxyd dev/socket/fwmarkd dev/socket/vold dev/socket/cryptd dev/socket/installd dev/socket/property_service dev/socket/lmkd;do
  if [ -e "$root/$f" ] || [ -L "$root/$f" ];then rm -rf "$root/$f";left="$left $f";fi
 done
 wv=$root/system/app/webview/lib/arm/libwebviewchromium.so
 if [ -f "$wv" ] && [ ! -L "$wv" ] && [ ! -e "$root/lab/libwebviewchromium-software-surface.so" ];then
  mv "$wv" "$root/lab/libwebviewchromium-software-surface.so";left="$left webview-component"
 fi
 if [ -d "$root/system/app/webview/lib" ];then
  rmdir "$root/system/app/webview/lib/arm" 2>/dev/null || :;rmdir "$root/system/app/webview/lib" 2>/dev/null && left="$left webview-lib-dir" || :
  webview_time || :
 fi
 echo "RESIDENT_LEFTOVERS_REMOVED items=${left:- none}"
fi
# Free space in the image (KiB) for the KOReader plugin's low-space warning;
# written again just before the image is unmounted.
df -Pk "$root" | awk 'NR==2 {print $4}' > "$res/image-free-kb" 2>/dev/null || :
loopdev=$(awk -v root="$root" '$2==root {print $1}' /proc/mounts)
# Test hooks left in the image by a session that ended (or froze) before the
# touch forwarder took them would fire as soon as the next forwarder starts,
# on whatever is on screen then (2026-11-01: a stale "140 520" pressed the
# launcher's back-to-KOReader button). Never meaningful at session start.
rm -f "$root/lab/test-tap" "$root/lab/test-page-key" "$root/lab/test-page-key-previous"
# I/O tuning for this session (test switch resident-v188/io-tune, one line
# "SCHEDULER READAHEAD_KB", e.g. "deadline 512"): the SD card's I/O scheduler
# and the SD and loop readahead. The SD card's previous values are restored
# in cleanup (the loop device goes away with the image).
io_old=
if [ -f "$res/io-tune" ] && [ ! -L "$res/io-tune" ] && [ -n "$loopdev" ];then
 read -r io_sched io_ra < "$res/io-tune" || :
 io_mb=/sys/block/mmcblk0/queue;io_lb=/sys/block/${loopdev#/dev/}/queue
 io_old="$(sed 's/.*\[\(.*\)\].*/\1/' $io_mb/scheduler) $(cat $io_mb/read_ahead_kb)"
 case "$io_sched" in noop|deadline|cfq) echo "$io_sched" > $io_mb/scheduler;;esac
 case "$io_ra" in ''|*[!0-9]*) :;; *) echo "$io_ra" > $io_mb/read_ahead_kb;echo "$io_ra" > $io_lb/read_ahead_kb;;esac
 echo "FAST_IO_TUNE scheduler=$(sed 's/.*\[\(.*\)\].*/\1/' $io_mb/scheduler) sd_ra=$(cat $io_mb/read_ahead_kb) loop_ra=$(cat $io_lb/read_ahead_kb) previous=$io_old"
fi
# WeRead's install directory differs per image (-1 in graphics.img, -2 after
# the 1.9.8 downgrade install).
app_dir=$(ls -d "$root"/data/app/com.tencent.weread.eink-* 2>/dev/null | head -n 1)
[ -n "$app_dir" ] || { echo RESIDENT_WEREAD_NOT_INSTALLED;exit 105; }
odex=$app_dir/oat/arm/base.odex
# WeRead tuning switches, read inside the image by libkobo_jit_config.so:
# fast/skia-disabled (Skia glyph cache stays 768 KB), fast/skia-probe (log
# its use), fast/memclass-disabled (memory class stays 64m),
# fast/neonblit-disabled (Skia keeps its portable glyph blending),
# fast/neoncolor-disabled (portable solid-colour row blending, 1.8.8 only),
# fast/fontmap-disabled (bundled reader font inflated from the APK again),
# fast/webviewpool-disabled (WebView pre-warmed on the home page again),
# fast/gcms-disabled (WeRead keeps the concurrent collector),
# fast/svcoff-disabled (system_server starts every service again),
# fast/heapmax8-disabled (WeRead keeps up to 16 MB of free heap again),
# fast/logsync-disabled (WeRead's log files are fsynced again),
# fast/gesturewait-disabled (the input helper queues keys behind unfinished taps again),
# fast/pagefix-disabled (off-screen reader pages stay registered with their chapter, the 1.8.8 leak),
# fast/memtrim-disabled (no heap trim / onTrimMemory when MemAvailable runs low).
# fast/segdict-disabled (:seg keeps the IK dictionary after a book is closed),
# fast/seglow-disabled (:seg threads keep their normal CPU priority),
# fast/fontcache-disabled (WeRead reads the font size from LevelDB for every view on every layout again),
# fast/poollow-disabled (WRCommonPool/WRNetworkPool threads keep their normal CPU priority).
# fast/fbsingle-disabled (the HWC flips between both framebuffer halves again; fb0 filled with zeros).
# fast/syncthrottle-disabled (WeRead re-downloads book light reads, best marks and chapter reviews on every open again).
# fast/homeonce-disabled (system_server starts the launcher before systemReady again; AMS then kills it and starts it a second time for HOME).
for s in skia-disabled skia-probe memclass-disabled hwcskip-disabled neonblit-disabled neoncolor-disabled fontmap-disabled webviewpool-disabled gcms-disabled svcoff-disabled heapmax8-disabled logsync-disabled gesturewait-disabled pagefix-disabled memtrim-disabled segdict-disabled seglow-disabled fontcache-disabled poollow-disabled homeonce-disabled syncthrottle-disabled chapterprefetch-disabled markusers-disabled bookprefetch-disabled;do
 if [ -e "$fast/$s" ];then : > "$root/data/local/tmp/kobo-$s";chmod 0644 "$root/data/local/tmp/kobo-$s";else rm -f "$root/data/local/tmp/kobo-$s";fi
done
# Chapters typeset ahead around the current one (fast-v188/chapterprefetch-range,
# one number 1-20; without it 3), read by ChapterPrefetch in WeRead.
rm -f "$root/data/local/tmp/kobo-chapterprefetch-range"
if [ -f "$fast/chapterprefetch-range" ] && [ ! -L "$fast/chapterprefetch-range" ];then
 read -r cp_range < "$fast/chapterprefetch-range" || :
 case "${cp_range:-}" in [1-9]|1[0-9]|20) printf '%s\n' "$cp_range" > "$root/data/local/tmp/kobo-chapterprefetch-range";chmod 0644 "$root/data/local/tmp/kobo-chapterprefetch-range";echo "FAST_CHAPTER_PREFETCH_RANGE $cp_range";;esac
fi
# Power status for ChapterPrefetch's whole-book mode (2026-11-11): the chroot
# has no /sys, so the battery's status file (Charging / Not charging / Full /
# Discharging; sysfs mode 0444) is bound at data/local/tmp/kobo-power-status
# and unbound in cleanup with the other resident binds.
pw_t=$root/data/local/tmp/kobo-power-status
if [ ! -e "$fast/chapterprefetch-disabled" ] && [ ! -e "$fast/bookprefetch-disabled" ];then
 for ps in /sys/class/power_supply/*;do
  [ -f "$ps/status" ] && [ -f "$ps/capacity" ] || continue
  rm -f "$pw_t";: > "$pw_t";chmod 0444 "$pw_t"
  if mount --bind "$ps/status" "$pw_t";then res_bound="$pw_t $res_bound";read -r pw_s < "$pw_t" || :;echo "FAST_POWER_STATUS_BOUND source=${ps##*/} status=${pw_s:-}";fi
  break
 done
else rm -f "$pw_t";fi
# Glyph cache size (fast-v188/skia-mb, one number 1-16, MB; without it 16 MB;
# fast/skia-disabled still wins and keeps Skia's 768 KB).
rm -f "$root/data/local/tmp/kobo-skia-mb"
if [ -f "$fast/skia-mb" ] && [ ! -L "$fast/skia-mb" ];then
 read -r skia_mb < "$fast/skia-mb" || :
 case "${skia_mb:-}" in [1-9]|1[0-6]) printf '%s\n' "$skia_mb" > "$root/data/local/tmp/kobo-skia-mb";chmod 0644 "$root/data/local/tmp/kobo-skia-mb";echo "FAST_SKIA_MB $skia_mb";;esac
fi
# The single-buffer HWC is opt-in (the 1.9.8 bridge does not know its BUSY state).
rm -f "$root/data/local/tmp/kobo-fbsingle-disabled"
if [ -e "$fast/fbsingle-disabled" ];then rm -f "$root/data/local/tmp/kobo-fbsingle-enabled";else : > "$root/data/local/tmp/kobo-fbsingle-enabled";chmod 0644 "$root/data/local/tmp/kobo-fbsingle-enabled";fi
# WeRead 1.8.8's reader font, stored deflated in base.apk, extracted once for
# libkobo_jit_config.so to map instead of inflating it into 11 MB of
# anonymous memory. Checked against the 1.8.8 asset's SHA-256; extracted
# again when base.apk's size or time changes.
fontdir=$root/data/local/tmp/kobo-fonts;font=SourceHanSerifCN-Medium_FDK_motify_strip.otf
font_stamp=$(stat -c '%s %Y' "$app_dir/base.apk" 2>/dev/null)
if [ ! -e "$fast/fontmap-disabled" ] && { [ ! -f "$fontdir/$font" ] || [ "$(cat "$fontdir/stamp" 2>/dev/null)" != "$font_stamp" ]; };then
 rm -rf "$fontdir";mkdir -m 0755 "$fontdir"
 if unzip -p "$app_dir/base.apk" "assets/fonts/$font" > "$fontdir/$font.tmp" 2>/dev/null &&
    [ "$(sha256sum "$fontdir/$font.tmp" | awk '{print $1}')" = c8009023d5233d982f6c84007e898496069e880167959643fcba5630b0742134 ];then
  chmod 0644 "$fontdir/$font.tmp";mv "$fontdir/$font.tmp" "$fontdir/$font";echo "$font_stamp" > "$fontdir/stamp";echo FAST_FONT_EXTRACTED
 else rm -rf "$fontdir";echo FAST_FONT_EXTRACT_FAILED;fi
fi
# Compressed swap in RAM (zram, LZO) for the session: no swap otherwise, so
# anonymous memory that is never touched again (KOReader stopped behind
# Android, idle helpers, cold heap pages) can only stay, and the kernel drops
# file cache instead (the first book open's re-reads). kobo_zsmalloc.ko and
# kobo_zram.ko are built from the kernel's own sources (src/kmod/zram; $kmods);
# 96 MB of uncompressed capacity, swappiness 100 while it is on; all undone
# in cleanup. Off with fast-v188/zram-disabled.
if [ ! -e "$fast/zram-disabled" ] && [ ! -e /sys/block/zram0 ];then
 for m in kobo_zsmalloc kobo_zram;do kmod_ok $m || { echo FAST_ZRAM_HASH_MISMATCH;exit 105; };done
 [ "$(sha256sum "$fast/kobo-mkswap" | awk '{print $1}')" = "$(cat "$fast/kobo-mkswap.sha256")" ] || { echo FAST_ZRAM_HASH_MISMATCH;exit 105; }
 if insmod "$kmods/kobo_zsmalloc.ko" && { insmod "$kmods/kobo_zram.ko" num_devices=1 || { rmmod kobo_zsmalloc;false; }; };then
  zram_loaded=1;zd=$(cat /sys/block/zram0/dev);rm -f "$zram_node";mknod "$zram_node" b "${zd%:*}" "${zd#*:}"
  if echo lzo > /sys/block/zram0/comp_algorithm && echo 100663296 > /sys/block/zram0/disksize && "$fast/kobo-mkswap" "$zram_node" >/dev/null && swapon "$zram_node";then
   zram_on=1;old_swappiness=$(cat /proc/sys/vm/swappiness);echo 100 > /proc/sys/vm/swappiness;echo FAST_ZRAM_SWAP_ON mb=96
   # Before Android starts, the only anonymous memory worth moving is
   # KOReader's (stopped for the whole session); the file cache it costs is
   # refilled by the boot prefetch below. Squeeze 64 MB once to push it out.
   [ "$(sha256sum "$fast/kobo-squeeze" | awk '{print $1}')" != "$(cat "$fast/kobo-squeeze.sha256")" ] || "$fast/kobo-squeeze" 64
  else echo FAST_ZRAM_SETUP_FAILED;fi
 else echo FAST_ZRAM_MODULE_LOAD_FAILED;fi
fi
# Non-debuggable boot image (1.8.8 CPU work, off unless
# fast-v188/bootnodebug-enabled exists). ART 7.0 compiles every boot image
# debuggable, so framework code has no inlining at all; lab/boot/out/ holds
# the same boot classes rebuilt without it (compile-boot.sh), lab/boot/apps/
# every /system odex (services, WebView, system apps) and WeRead's odex
# compiled against it
# (compile-boot-apps.sh). All are bound over the shipped files for the
# session only. Files are hashed in full once, then recognised by inode,
# size and time (resident-v188/verified-stamps-boot).
if [ -e "$fast/bootnodebug-enabled" ];then
 bstamps=$res/verified-stamps-boot;bok=1
 for sums in "$root/lab/boot/out/SHA256SUMS" "$root/lab/boot/apps/SHA256SUMS";do
  [ -f "$sums" ] || { bok=0;break; }
  while read -r want name;do
   f=${sums%/*}/$name;key="$f $(stat -c '%i %s %Y' "$f" 2>/dev/null) $want"
   grep -qxF "$key" "$bstamps" 2>/dev/null && continue
   [ "$(sha256sum "$f" 2>/dev/null | awk '{print $1}')" = "$want" ] || { bok=0;break 2; }
   echo "$key" >> "$bstamps"
  done < "$sums"
 done
 if [ "$bok" = 1 ];then
  for f in "$root"/lab/boot/out/boot*.art "$root"/lab/boot/out/boot*.oat;do
   t=$root/system/framework/arm/${f##*/};mount --bind "$f" "$t";boot_bound="$t $boot_bound"
  done
  for pair in "services.odex system/framework/oat/arm/services.odex" "webview.odex system/app/webview/oat/arm/webview.odex" \
   "ExtShared.odex system/app/ExtShared/oat/arm/ExtShared.odex" "DefaultContainerService.odex system/priv-app/DefaultContainerService/oat/arm/DefaultContainerService.odex" \
   "ExtServices.odex system/priv-app/ExtServices/oat/arm/ExtServices.odex" "PackageInstaller.odex system/priv-app/PackageInstaller/oat/arm/PackageInstaller.odex" \
   "SettingsProvider.odex system/priv-app/SettingsProvider/oat/arm/SettingsProvider.odex" "StatementService.odex system/priv-app/StatementService/oat/arm/StatementService.odex";do
   set -- $pair;mount --bind "$root/lab/boot/apps/$1" "$root/$2";boot_bound="$root/$2 $boot_bound"
  done
  bootnodebug=1;echo FAST_BOOT_NODEBUG_BOUND
 else echo FAST_BOOT_NODEBUG_HASH_MISMATCH_USING_SHIPPED;fi
fi
# Boot-file prefetch: the runtime and framework phases fault in ~90 MB of the
# boot image, services.odex and libraries page by page from the SD card
# (I/O wait 30-40%). One sequential read in first-use order, from mount on,
# fills the image's page cache ahead of them. A single cat process, stopped by
# cleanup before the image is unmounted. Off with resident/prefetch-disabled.
prefetch=
if [ ! -e "$res/prefetch-disabled" ];then
 cat "$root"/system/framework/arm/boot*.art "$root/system/framework/arm/boot.oat" "$root/system/framework/arm/boot-core-libart.oat" \
  "$root/system/lib/libart.so" "$root/system/framework/arm/boot-framework.oat" "$root/system/framework/oat/arm/services.odex" \
  "$root/system/lib/libLLVM.so" > /dev/null 2>&1 &
 prefetch=$!
 echo RESIDENT_PREFETCH_BOOT
fi
[ "${KOBO_WEREAD_REUSE:-0}" = 1 ] && [ "${KOBO_WEREAD_HOST_NETWORK:-0}" = 1 ] && { [ "${KOBO_WEREAD_AGREEMENT_DIAG:-0}" = 1 ] || [ "${KOBO_WEREAD_INTERACTIVE:-0}" = 1 ]; } || { echo SOFTWARE_SURFACE_TEST_REQUIRES_AGREEMENT_OR_INTERACTIVE_SESSION;exit 101; }

if [ "${KOBO_FAST_GOVERNOR:-1}" = 1 ] && [ -w "$gov" ];then
 old_gov=$(cat "$gov")
 echo performance > "$gov"
 echo "FAST_GOVERNOR=$(cat "$gov") previous=$old_gov"
fi
# Drop graphics.img's vfat-side page cache every 3 s: the image on loop otherwise
# caches each page twice. kobo_pagecache only drops clean, unmapped pages.
if [ "${KOBO_FAST_DROP_LOOP_CACHE:-1}" = 1 ] && [ ! -e "$fast/pagecache-disabled" ] && ! grep -q '^kobo_pagecache ' /proc/modules;then
 kmod_ok kobo_pagecache || { echo FAST_PAGECACHE_HASH_MISMATCH;exit 105; }
 insmod "$kmods/kobo_pagecache.ko";pc_loaded=1
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
# A/B of a recompiled odex against the one it replaced: resident/odex-prev
# binds the -prev copy (kept with its own .sha256 by compile-speed-profile.sh).
[ ! -e "$res/odex-prev" ] || candidate=$root/lab/fast/weread-speed-profile-prev.odex
# With the non-debuggable boot image, WeRead's odex compiled against it.
if [ "${bootnodebug:-0}" = 1 ];then
 candidate=$root/lab/boot/apps/weread-speed-profile.odex
 awk '$2=="weread-speed-profile.odex" {print $1}' "$root/lab/boot/apps/SHA256SUMS" > "$candidate.sha256"
fi
imagedir=$root/lab/fast/oat-arm
odex_mount=$odex
sha_ok(){ [ -f "$1" ] && [ -f "$1.sha256" ] && [ "$(sha256sum "$1" | awk '{print $1}')" = "$(cat "$1.sha256")" ]; }
# Verified-hash cache for the two large files inside the image (84 MB odex,
# 35 MB WebView library: ~7.4 s of hashing per start). A file whose inode,
# size and mtime match a record written after a successful hash against the
# same expected value is not hashed again; a recompiled or replaced file, or a
# new expected hash, is hashed in full. Off with resident/hashcache-disabled.
stamps=$res/verified-stamps${image_tag:+-$image_tag}
sha_cached(){ # file expected_sha256
 [ -f "$1" ] && [ ! -L "$1" ] || return 1
 key="$1 $(stat -c '%i %s %Y' "$1") $2"
 if [ ! -e "$res/hashcache-disabled" ] && [ -f "$stamps" ] && grep -qxF "$key" "$stamps";then echo "RESIDENT_HASH_CACHED file=${1##*/}";return 0;fi
 [ "$(sha256sum "$1" | awk '{print $1}')" = "$2" ] || return 1
 { grep -vF "$1 " "$stamps" 2>/dev/null || :;echo "$key"; } > "$stamps.new";mv "$stamps.new" "$stamps"
 echo "RESIDENT_HASH_VERIFIED file=${1##*/}"
}
if [ "${KOBO_FAST_ODEX:-1}" = 1 ] && [ ! -e "$fast/odex-disabled" ] && [ ! -e "$fast/appimage-disabled" ] && sha_ok "$imagedir/base.odex" && sha_ok "$imagedir/base.art";then
 odex_mount=${odex%/base.odex}
 mount --bind "$imagedir" "$odex_mount"
 odex_bound=1
 echo FAST_SPEED_PROFILE_APP_IMAGE_DIR_BOUND
elif [ "${KOBO_FAST_ODEX:-1}" = 1 ] && [ ! -e "$fast/odex-disabled" ] && [ -f "$candidate" ] && [ -f "$candidate.sha256" ];then
 if sha_cached "$candidate" "$(cat "$candidate.sha256")";then
  # 1.8.8 was put in place offline, so PackageManager never compiled it and
  # oat/arm/ has no base.odex to bind over: an empty one stands in (without
  # the bind ART rejects it and runs the dex, as it would with none).
  [ -e "$odex" ] || { mkdir -p "${odex%/*}";: > "$odex";chmod 0644 "$odex";echo FAST_ODEX_PLACEHOLDER_CREATED; }
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
res_bind "$probe" "$root/lab/install-framework-probe.dex"
res_bind KoboProbe-resident.apk "$root/system/app/KoboProbe/KoboProbe.apk"
res_bind host-default-network-resident "$root/lab/host-default-network"
res_bind touch-forward-resident "$root/lab/native-resolution/touch-forward"
# Scan the optional IME as a system APK only during enabled sessions.
ime_dir=$root/system/app/PinyinIME
rm -f "$ime_dir/PinyinIME.apk" "$ime_dir/lib/arm/libjni_pinyinime.so"
if [ -e "$fast/aosp-pinyin-enabled" ];then
 if res_sha_ok PinyinIME.apk && res_sha_ok libjni_pinyinime.so && res_sha_ok pinyin-process-broker;then
  mkdir -p "$ime_dir/lib/arm"
  chmod 0755 "$ime_dir" "$ime_dir/lib" "$ime_dir/lib/arm"
  : > "$ime_dir/lib/arm/libjni_pinyinime.so"
  res_bind libjni_pinyinime.so "$ime_dir/lib/arm/libjni_pinyinime.so"
  : > "$ime_dir/PinyinIME.apk"
  res_bind PinyinIME.apk "$ime_dir/PinyinIME.apk"
  res_bind pinyin-process-broker "$root/lab/install-process-broker"
  echo RESIDENT_AOSP_PINYIN_BOUND
 else echo RESIDENT_AOSP_PINYIN_ARTIFACTS_MISSING;fi
fi
rm -f "$root/data/local/tmp/kobo-ime-trace"
if [ -e "$fast/ime-trace-enabled" ];then
 : > "$root/data/local/tmp/kobo-ime-trace"
 chmod 0444 "$root/data/local/tmp/kobo-ime-trace"
fi
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
# WeRead's own process entry: the process broker starts WeRead with
# /lab/weread-process-entry.dex. This is the image's entry (same
# WeReadProcessEntry) plus ReaderPageRelease, which unregisters off-screen
# reader pages from their chapter's progress adapter (the 1.8.8 page leak,
# 2026-11-07; off: fast-v188/pagefix-disabled), SegDictRelease in :seg (frees
# the segmentation dictionary when no book is open, :seg threads at nice 19;
# off: segdict-disabled, seglow-disabled), and in the main process
# FontSizeCache (cached font size for WeRead's per-layout font adaptation) and
# PoolPriority (WRCommonPool / WRNetworkPool at nice 19); off:
# fontcache-disabled, poollow-disabled (2026-11-09, ported from 1.9.7). The
# image's entry is used only when all of them are off; each part checks its
# own data/local/tmp/kobo-*-disabled file.
if [ ! -e "$fast/pagefix-disabled" ] || [ ! -e "$fast/segdict-disabled" ] || [ ! -e "$fast/seglow-disabled" ] || [ ! -e "$fast/fontcache-disabled" ] || [ ! -e "$fast/poollow-disabled" ] || [ ! -e "$fast/syncthrottle-disabled" ] || [ ! -e "$fast/chapterprefetch-disabled" ] || [ ! -e "$fast/markusers-disabled" ];then
 res_bind weread-app-entry-resident.dex "$root/lab/weread-process-entry.dex"
 echo "RESIDENT_APP_ENTRY_BOUND pagefix=$([ -e "$fast/pagefix-disabled" ] && echo 0 || echo 1) segdict=$([ -e "$fast/segdict-disabled" ] && echo 0 || echo 1) seglow=$([ -e "$fast/seglow-disabled" ] && echo 0 || echo 1) fontcache=$([ -e "$fast/fontcache-disabled" ] && echo 0 || echo 1) poollow=$([ -e "$fast/poollow-disabled" ] && echo 0 || echo 1)"
fi

# Fork-free copy of the memory watchdog (same thresholds and log lines).
if [ ! -e "$fast/watchdog-disabled" ];then
 [ "$(sha256sum "$fast/native-memory-watchdog-fast.sh" | awk '{print $1}')" = "$(cat "$fast/native-memory-watchdog-fast.sh.sha256")" ] || { echo FAST_WATCHDOG_HASH_MISMATCH;exit 105; }
 mount --bind "$fast/native-memory-watchdog-fast.sh" "$watchdog"
 watchdog_bound=1
 echo FAST_WATCHDOG_BOUND
fi

component="$root/lab/libwebviewchromium-software-surface.so"
[ -f "$component" ] && [ ! -L "$component" ] || exit 102
sha_cached "$component" d39f571b5c538bf20cca6f37637dfb5dd234da28d66ec7a99db19fa1bf9820b3 || { echo SOFTWARE_SURFACE_COMPONENT_HASH_MISMATCH;exit 103; }
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
webview_time
echo PATCHED_WEBVIEW_CPU_SURFACE_COMPONENT_SELECTED
KOBO_RESIDENT_GOVERNOR="$old_gov" KOBO_WEBVIEW_SOFTWARE_SURFACE=1 "$root/lab/fd-clean-exec" /bin/sh "$root/lab/native-resolution/run-native-weread.sh"
