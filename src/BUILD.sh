#!/bin/sh
# Build every fast-entry component from this repository only.
#   sh src/BUILD.sh [OUT_DIR]        (default: build/out, created fresh)
# Tools come from tools/ and the Codex prototype headers/sources from
# upstream/codex-kobo-android/; nothing outside the repository is read except the
# system clang and python3 used by the kernel module build.
# The resident launcher has its own script: src/resident/build.sh.
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
tools=${KOBO_TOOLS:-$root/tools}  # toolchain directory (bootstrap.sh)
jdk_home=$tools/jdk17/Contents/Home;[ -d "$jdk_home" ] || jdk_home=$tools/jdk17  # macOS bundle or Linux layout
src=$root/src
up=$root/upstream/codex-kobo-android
zig=$tools/ziglang/zig
fbink=$tools/fbink
jdk=$jdk_home
jar=$tools/android/android.jar
d8=$tools/android/d8.jar
sysroot=$tools/sysroot
aosp=$tools/aosp/include
kb=$tools/kbuild
out=${1:-$root/build/out}
[ ! -e "$out" ] || [ -z "$(ls -A "$out")" ] || { echo "Output directory is not empty: $out" >&2;exit 2; }
mkdir -p "$out" "$root/build/zig-cache"
export ZIG_GLOBAL_CACHE_DIR=$root/build/zig-cache ZIG_LOCAL_CACHE_DIR=$root/build/zig-cache
h=$tools/ziglang/lib/libc/include
musl="-target arm-linux-musleabi -mcpu=cortex_a9 -static -O2 -Wall -Wextra"
bionic_inc="-isystem $h/arm-linux-musl -isystem $h/generic-musl -isystem $h/arm-linux-any -isystem $h/any-linux-any"

# Host-side static tools (musl).
for t in loop-cache startup-trace fb-watch fb-seq-watch audit-runtime-fds;do "$zig" cc $musl "$src/$t.c" -o "$out/$t";done

# JIT profile + heap headroom shim (bionic shared library).
# No unwind tables: their personality routine would pull in compiler-rt (__aeabi_*).
# NEON (softfp: integer-only interfaces) for the glyph blending that replaces
# libskia's portable SkOpts::blit_mask_d32_a8.
(cd "$src" && "$zig" cc -target arm-linux-gnueabi -mcpu=cortex_a9 -mfpu=neon -mfloat-abi=softfp -O2 -shared -fPIC -nostdlib -fno-stack-protector -fno-unwind-tables -fno-asynchronous-unwind-tables $bionic_inc \
 -I"$jdk/include" -I"$src/resident-v197/shim" jit-config.c "$sysroot/system/lib/libc.so" "$sysroot/system/lib/libdl.so" -o "$out/libkobo_jit_config.so")

# SurfaceFlinger single-buffer variant of Codex's virtual framebuffer shim.
(cd "$src/fbshim" && "$zig" cc -DKOBO_LIBRA2_NATIVE -DKOBO_TARGET_LIBRA2 -target arm-linux-gnueabi -mcpu=cortex_a9 -O2 -shared -fPIC -nostdlib -fno-stack-protector \
 -isystem "$h/arm-linux-any" -isystem "$h/any-linux-any" kobo-virtual-fb.c "$sysroot/system/lib/libc.so" \
 -Wl,--version-script=fb.exports -o "$out/libkobo_fb-hwc.so")

# HWC 1.1 (bionic shared library, AOSP android-7.0.0_r1 headers).
(cd "$src/hwc" && "$zig" cc -DKOBO_LIBRA2_NATIVE -DKOBO_TARGET_LIBRA2 -target arm-linux-gnueabi -mcpu=cortex_a9 -mfloat-abi=softfp -O2 -Wall -Wextra \
 -shared -fPIC -nostdlib -fno-stack-protector -fno-unwind-tables -fno-asynchronous-unwind-tables -fvisibility=hidden \
 -I"$aosp" -Iinclude-shim $bionic_inc hwcomposer.c "$sysroot/system/lib/libc.so" "$sysroot/system/lib/libhardware.so" \
 -Wl,-soname,hwcomposer.default.so -o "$out/hwcomposer.default.so")

# Display bridge (static, FBInk), with Codex's bridge headers.
"$zig" cc -DKOBO_LIBRA2_NATIVE -DKOBO_TARGET_LIBRA2 -DFBINK_FOR_KOBO -DFBINK_MINIMAL -DFBINK_WITH_IMAGE -DFBINK_WITH_DRAW \
 -target arm-linux-musleabi -mcpu=cortex_a9 -static -O2 \
 -I"$up/display-bridge-prototype" -I"$fbink" -I"$fbink/i2c-tools/include" \
 "$src/display-bridge/live-display-fast.c" "$fbink/fbink.c" "$fbink/qimagescale/qimagescale.c" "$fbink/i2c-tools/lib/smbus.c" -lm -o "$out/live-display-fast"

# Java helpers.
for j in sampling/FastMethodSampling anim/FastAnimationScales;do
 n=$(basename "$j");mkdir -p "$out/java/$n/c" "$out/java/$n/d"
 "$jdk/bin/javac" -nowarn -encoding UTF-8 -source 8 -target 8 -bootclasspath "$jar" -d "$out/java/$n/c" "$src/$j.java"
 "$jdk/bin/java" -cp "$d8" com.android.tools.r8.D8 --min-api 24 --lib "$jar" --output "$out/java/$n/d" "$out/java/$n/c"/*.class
done
cp "$out/java/FastMethodSampling/d/classes.dex" "$out/method-sampling-fast.dex"
cp "$out/java/FastAnimationScales/d/classes.dex" "$out/animation-scales.dex"
# Input helper: Codex's entry dex with the page-key fix (fast entry).
KOBO_JAVA_HOME=$jdk KOBO_ANDROID_JAR=$jar KOBO_D8_JAR=$d8 KOBO_BUILD_OUT="$out/java/entry" sh "$src/entry/weread-prototype/build-entry.sh"
cp "$out/java/entry/weread-process-entry.dex" "$out/weread-process-entry-fast.dex"

# Kernel module kobo_pagecache (prepared Libra 2 tree; Module.symvers carries
# invalidate_mapping_pages 0xdaf26649, checked against the stock usb_f_mass_storage.ko).
# KOBO_SKIP_KMOD=1 skips it (the public build ships the verified modules in kernels/).
if [ "${KOBO_SKIP_KMOD:-0}" != 1 ];then
mkdir -p "$out/kmod" && cp "$src/kmod/pagecache/Makefile" "$src/kmod/pagecache/kobo_pagecache.c" "$out/kmod/"
make -s -C "$kb/libra2-kernel-source" ARCH=arm CC="$kb/arm-clang" LD="$kb/arm-ld" HOSTCC=clang HOSTCFLAGS="-I$kb/host-include" \
 LOCALVERSION=-00838-g349fbf5767c M="$out/kmod" modules
cp "$out/kmod/kobo_pagecache.ko" "$out/kobo_pagecache.ko"
fi

# Scripts are deployed as they are; record every artifact's SHA-256.
(cd "$out" && for f in *;do [ -f "$f" ] || continue;case "$f" in *.sha256) continue;;esac;shasum -a 256 "$f" | awk '{print $1}' > "$f.sha256";done)
echo "Built into $out"
