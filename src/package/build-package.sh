#!/bin/sh
# Build the KOReader installer package for WeRead 1.9.7 or 1.8.8 entirely from
# this repository and Google's API 24 ARM system image
# (docs/公开版设计.md). No Kobo is needed to build.
#
#   sh src/package/build-package.sh 1.9.7
#
# Inputs (bootstrap.sh prepares them):
#   KOBO_TOOLS   toolchain directory (default ./tools): ziglang/, jdk17/,
#                android/{android.jar,d8.jar,aapt2}, fbink/, aosp/, sysroot/,
#                dl/e2fsprogs-1.47.1.tar.xz
#   KOBO_GOOGLE  extracted Google image (default ./build/google): system/
#                (system.img contents) and ramdisk/ (ramdisk.img contents)
#   KOBO_KEYSTORE  your signing key for the launcher and Codex's probe apps
#                (alias "lab", store/key password in KOBO_KEY_PASSWORD,
#                default "kobolabtest"); created on first use in
#                build/keys/lab.jks. Never share it; it is not needed by
#                anyone else.
# Output: releases/wereadkobo-VERSION-DATE-local.zip. The user supplies the WeRead APK at installation.
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
top=$(CDPATH= cd -- "$here/../.." && pwd)
ver=${1:?version 1.9.7 or 1.8.8}
case "$ver" in
 1.9.7) tag=v197
        switches="anim-disabled appimage-disabled fontmap-disabled gcms-disabled heapmax8-disabled logsync-disabled memclass-disabled memtrim-disabled pagefix-disabled svcoff-disabled";;
 1.8.8) tag=v188
        switches="anim-disabled appimage-disabled gcms-disabled heapmax8-disabled logsync-disabled memclass-disabled memtrim-disabled svcoff-disabled";;
 *) echo "PACKAGE_UNKNOWN_VERSION $ver (1.9.7 or 1.8.8)" >&2;exit 2;;
esac
sha256(){ if command -v sha256sum >/dev/null 2>&1;then sha256sum "$1" | cut -c1-64;else shasum -a 256 "$1" | cut -c1-64;fi; }
export KOBO_TOOLS=${KOBO_TOOLS:-$top/tools}
G=${KOBO_GOOGLE:-$top/build/google}
for p in "$G/system/lib/libc.so" "$G/system/app/webview/webview.apk" "$G/ramdisk/file_contexts.bin" "$KOBO_TOOLS/ziglang/zig";do
 [ -e "$p" ] || { echo "PACKAGE_INPUT_MISSING $p (run bootstrap.sh)" >&2;exit 4; };done
jdk=$KOBO_TOOLS/jdk17;[ -d "$jdk/Contents/Home" ] && jdk=$jdk/Contents/Home
w=$top/build/package-$tag;rm -rf "$w";mkdir -p "$w"
log(){ echo "PACKAGE_STEP $*"; }

# 1. Signing key (yours) and its certificate for packages.xml.
pass=${KOBO_KEY_PASSWORD:-kobolabtest}
key=${KOBO_KEYSTORE:-$top/build/keys/lab.jks}
if [ ! -f "$key" ];then
 mkdir -p "$(dirname "$key")"
 "$jdk/bin/keytool" -genkeypair -keystore "$key" -storepass "$pass" -keypass "$pass" -alias lab -keyalg RSA -keysize 2048 \
  -validity 10000 -dname 'CN=Kobo Lab Test, O=Local Experiment' >/dev/null 2>&1
 log "created signing key $key"
fi
"$jdk/bin/keytool" -exportcert -keystore "$key" -storepass "$pass" -alias lab -file "$w/lab-cert.der" >/dev/null 2>&1
cert=$(od -An -v -tx1 "$w/lab-cert.der" | tr -d ' \n')

# 2. Builds.
log "runtime layer";KOBO_BUILD_OUT=$w/runtime KOBO_GOOGLE_SYSTEM=$G/system sh "$top/src/runtime/build.sh" > "$w/runtime.log" 2>&1
log "launcher $ver";KOBO_BUILD_OUT=$w/resident KOBO_KEYSTORE=$key KOBO_PROBE_STORE_PASSWORD=$pass sh "$top/src/resident-$tag/build.sh" > "$w/resident.log" 2>&1
log "display and system libraries";KOBO_SKIP_KMOD=1 sh "$top/src/BUILD.sh" "$w/fast" > "$w/fast.log" 2>&1
log "JIT shim";mkdir -p "$w/shim";sh "$top/src/resident-$tag/shim/build-shim.sh" "$w/shim/libkobo_jit_config.so" > "$w/shim.log" 2>&1
log "host tools";KOBO_BUILD_OUT=$w/tools sh "$top/src/tools/build-host-tools.sh" > "$w/tools.log" 2>&1
"$KOBO_TOOLS/ziglang/zig" cc -target arm-linux-musleabi -mcpu=cortex_a9 -static -O2 -Wall -Wextra "$top/src/diag/kobo-pageprefetch.c" -o "$w/tools/kobo-pageprefetch"
log "probe app";mkdir -p "$w/probe"
KOBO_JAVA_HOME=$jdk KOBO_ANDROID_JAR=$KOBO_TOOLS/android/android.jar KOBO_D8_JAR=$KOBO_TOOLS/android/d8.jar KOBO_AAPT2=$KOBO_TOOLS/android/aapt2 \
 KOBO_LAB_KEYSTORE=$key KOBO_BUILD_OUT=$w/probe sh "$top/upstream/codex-kobo-android/installation-prototype/build.sh" > "$w/probe.log" 2>&1
r=$w/resident/artifacts
if [ -n "${KOBO_PINYIN_SOURCE:-}" ];then
 log "optional AOSP PinyinIME"
 KOBO_BUILD_OUT=$w/pinyin KOBO_KEYSTORE=$key KOBO_PROBE_STORE_PASSWORD=$pass \
  sh "$top/src/resident-v197/pinyin/build.sh" > "$w/pinyin.log" 2>&1
fi

# 3. Launcher files (BASE/...).
h=$w/host;mkdir -p "$h/resident-$tag" "$h/fast-$tag/hwc-lib"
cp "$w/runtime/lab/fd-clean-exec" "$h/fd-clean-exec"
printf 'libra2-4.1.15-00838-g349fbf5767c-632x840\n' > "$h/device-profile"
printf '30\n' > "$h/display-cadence";printf 'reagl-text\n' > "$h/display-profile"
R=$h/resident-$tag
for f in KoboProbe-resident.apk dns-proxy-resident host-default-network-resident install-framework-probe-resident-$tag.dex live-display-resident \
 resident-dns-check resident-sched touch-forward-resident weread-app-entry-resident.dex weread-process-entry-resident.dex;do cp "$r/$f" "$R/$f";done
if [ -n "${KOBO_PINYIN_SOURCE:-}" ];then
 cp "$r/pinyin-process-broker" "$w/pinyin/PinyinIME.apk" "$w/pinyin/libjni_pinyinime.so" "$w/pinyin/PinyinIME.NOTICE" "$R/"
fi
cp "$w/tools/e2fsck" "$w/tools/tune2fs" "$w/tools/kobo-pageprefetch" "$R/"
d=$top/src/resident-$tag/device
for f in launch-resident.sh resident-ctl.sh run-resident-session.sh run-native-weread-resident.sh resident-loop.sh plugin-main-resident.lua \
 plugin-meta.lua check-shutdown.sh compile-menu.sh relaunch.sh io-tune $tag-wal.sh $tag-wal.lua boot-prefetch.list;do cp "$d/$f" "$R/$f";done
F=$h/fast-$tag
cp "$w/fast/animation-scales.dex" "$w/fast/audit-runtime-fds" "$w/fast/libkobo_fb-hwc.so" "$w/fast/loop-cache" "$F/"
cp "$w/tools/kobo-mkswap" "$w/tools/kobo-squeeze" "$w/shim/libkobo_jit_config.so" "$F/"
cp "$d/native-memory-watchdog-fast.sh" "$d/compile-speed-profile.sh" "$F/"
cp "$w/fast/hwcomposer.default.so" "$F/hwc-lib/"
for f in audio.primary.goldfish.so audio_policy.default.so gralloc.default.so;do cp "$G/system/lib/hw/$f" "$F/hwc-lib/";done
for s in $switches;do : > "$F/$s";done
mkdir -p "$h/kernels";cp -R "$top/kernels/"* "$h/kernels/"
# The installed copy lives in kobo-weread-home (same length as kobo-android-lab:
# paths compiled into the programs are replaced byte for byte).
lab=/mnt/onboard/.adds/kobo-android-lab;new=/mnt/onboard/.adds/kobo-weread-home
find "$h" -type f | while read -r f;do perl -0777 -pi -e "s#\Q$lab\E#$new#g" "$f";done
! grep -rl 'kobo-android-lab' "$h" || { echo PACKAGE_PATH_LEFT >&2;exit 5; }
p=$R/plugin-main-resident.lua
V=$ver perl -pi -e 's/name = "wereadv\d+"/name = "wereadkobo"/; s/kobo_resident_v\d+_quit_wrapped/kobo_wereadkobo_quit_wrapped/g; s/items\.weread_v\d+ /items.weread_kobo /; s/"微信读书 \Q$ENV{V}\E（启动器）"/"微信读书 $ENV{V}（安装版）"/' "$p"
grep -q 'name = "wereadkobo"' "$p" && grep -q 'items.weread_kobo' "$p" || { echo PACKAGE_PLUGIN_RENAME_FAILED >&2;exit 5; }
# .sha256 for the same files as the lab installation (sessions verify them).
for f in KoboProbe-resident.apk boot-prefetch.list dns-proxy-resident host-default-network-resident install-framework-probe-resident-$tag.dex \
 kobo-pageprefetch live-display-resident resident-dns-check resident-loop.sh resident-sched run-native-weread-resident.sh touch-forward-resident \
 weread-process-entry-resident.dex weread-app-entry-resident.dex plugin-main-resident.lua e2fsck tune2fs;do sha256 "$R/$f" > "$R/$f.sha256";done
if [ -n "${KOBO_PINYIN_SOURCE:-}" ];then
 for f in pinyin-process-broker PinyinIME.apk libjni_pinyinime.so;do sha256 "$R/$f" > "$R/$f.sha256";done
fi
for f in animation-scales.dex audit-runtime-fds kobo-mkswap kobo-squeeze libkobo_fb-hwc.so libkobo_jit_config.so native-memory-watchdog-fast.sh \
 hwc-lib/hwcomposer.default.so;do sha256 "$F/$f" > "$F/$f.sha256";done

# 4. Android image tree from the recipe.
log "image";mkdir -p "$w/payload"
python3 -I "$top/src/recipe/assemble-rootfs.py" --recipe "$top/recipe/$tag" --google-system "$G/system" --ramdisk "$G/ramdisk" \
 --webview-apk "$G/system/app/webview/webview.apk" --lab "$w/runtime/lab" --lab-cert "$cert" --payload \
 --build "launcher=$R/KoboProbe-resident.apk" --build "installed-probe=$w/probe/kobo-installed-probe.apk" \
 --build "jit-config=$w/shim/libkobo_jit_config.so" --out "$w/payload/rootfs.tar" > "$w/image.log"
python3 -I -c '
import lzma,shutil,sys
with open(sys.argv[1],"rb") as i, lzma.open(sys.argv[2],"wb",preset=6,check=lzma.CHECK_CRC32) as o: shutil.copyfileobj(i,o,1<<20)
' "$w/payload/rootfs.tar" "$w/payload/rootfs.tar.xz"
rm "$w/payload/rootfs.tar"

# 5. Installer payload and plugin (written for 1.8.8; 1.9.7 changes version and tag).
k=$w/wereadkobo.koplugin;pl=$k/payload;mkdir -p "$pl"
(cd "$h" && COPYFILE_DISABLE=1 tar --uid 0 --gid 0 --numeric-owner -czf "$pl/host.tar.gz" .) 2>/dev/null || (cd "$h" && tar --owner=0 --group=0 --numeric-owner -czf "$pl/host.tar.gz" .)
mv "$w/payload/rootfs.tar.xz" "$w/payload/apk-meta" "$w/payload/dirtimes" "$pl/"
for kd in "$top/kernels/"*/;do echo "$(basename "$kd") $(cat "$kd/firmware")";done > "$pl/kernels.list"
cp "$top/src/installer/install.sh" "$pl/";cp "$h/fd-clean-exec" "$pl/"
printf '%s\n' "$cert" > "$pl/launcher-cert"
echo "$ver-$(date +%Y%m%d)-local" > "$pl/package-version"
cp "$top/src/installer/plugin/main.lua" "$top/src/installer/plugin/_meta.lua" "$k/";cp "$top/src/installer/README.txt" "$k/README.txt"
sed '/^安装$/a\
请自行下载 weread-1.8.8.apk，放进 wereadkobo.koplugin 目录，与 main.lua 同级。
' "$k/README.txt" > "$k/README.txt.new"
mv "$k/README.txt.new" "$k/README.txt"
if [ $tag != v188 ];then
 for f in "$pl/install.sh" "$k/main.lua" "$k/_meta.lua" "$k/README.txt";do V=$ver T=$tag perl -pi -e 's/1\.8\.8/$ENV{V}/g; s/v188/$ENV{T}/g' "$f";done
fi
(cd "$pl" && for f in host.tar.gz rootfs.tar.xz apk-meta dirtimes kernels.list install.sh fd-clean-exec launcher-cert package-version;do echo "$(sha256 "$f")  $f";done > SHA256SUMS)
chmod 0755 "$pl/install.sh" "$pl/fd-clean-exec"
mkdir -p "$top/releases";zip=$top/releases/wereadkobo-$ver-$(date +%Y%m%d)-local.zip;n=2
while [ -e "$zip" ];do zip=$top/releases/wereadkobo-$ver-$(date +%Y%m%d)-local-$n.zip;n=$((n+1));done
(cd "$w" && zip -q -r -X "$zip" wereadkobo.koplugin)
echo "PACKAGE_DONE $zip"
