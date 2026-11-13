#!/bin/sh
# Runtime layer (the image's /lab) from source (docs/公开版设计.md):
#   KOBO_BUILD_OUT=DIR KOBO_GOOGLE_SYSTEM=DIR sh src/runtime/build.sh
# KOBO_GOOGLE_SYSTEM is the extracted /system of Google's API 24 ARM image
# (its libc.so/libdl.so/libEGL.so are linked against, as Codex did). Tools as
# for the other builds: KOBO_TOOLS (default ./tools) with ziglang/zig,
# jdk17 (or KOBO_JAVA_HOME), android/android.jar, android/d8.jar, fbink/.
# Runs Codex's own prototype build scripts (upstream/codex-kobo-android) into
# DIR/tmp and collects what a session uses into DIR/lab, under the names the
# image uses. Verified 2026-11-09 against the 1.9.7 image: every dex is
# byte-identical, every native program is identical after strip-all (only
# the embedded build paths differ).
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
top=$(CDPATH= cd -- "$here/../.." && pwd)
: "${KOBO_BUILD_OUT:?Set an empty output directory}" "${KOBO_GOOGLE_SYSTEM:?Set to the extracted Google /system}"
tools=${KOBO_TOOLS:-$top/tools}
up=$top/upstream/codex-kobo-android
out=$KOBO_BUILD_OUT
[ ! -e "$out/lab" ] || { echo "RUNTIME_OUT_NOT_EMPTY $out" >&2; exit 2; }
mkdir -p "$out/tmp/android" "$out/lab/native-resolution" "$out/lab/fast"
ln -sfn "$KOBO_GOOGLE_SYSTEM" "$out/tmp/android/system"
if [ -z "${KOBO_JAVA_HOME:-}" ]; then
 if [ -d "$tools/jdk17/Contents/Home" ]; then KOBO_JAVA_HOME=$tools/jdk17/Contents/Home; else KOBO_JAVA_HOME=$tools/jdk17; fi
fi
export KOBO_ZIG_ROOT=$tools/ziglang KOBO_JAVA_HOME KOBO_ANDROID_ROOT=$out/tmp/android \
 KOBO_ANDROID_JAR=$tools/android/android.jar KOBO_D8_JAR=$tools/android/d8.jar KOBO_FBINK_ROOT=$tools/fbink
export ZIG_GLOBAL_CACHE_DIR=${ZIG_GLOBAL_CACHE_DIR:-$top/build/zig-cache} ZIG_LOCAL_CACHE_DIR=${ZIG_LOCAL_CACHE_DIR:-$top/build/zig-cache}
zig=$KOBO_ZIG_ROOT/zig
run(){ # name script [env...]
 n=$1;s=$2;shift 2
 mkdir -p "$out/tmp/$n"
 env "$@" KOBO_BUILD_OUT="$out/tmp/$n" sh "$up/$s" > "$out/tmp/$n.log" 2>&1 || { echo "RUNTIME_BUILD_FAILED $n (see $out/tmp/$n.log)" >&2; exit 3; }
}
run activity activity-prototype/build-native.sh
run install installation-prototype/build-native.sh
run install-java installation-prototype/build-framework.sh
run network network-prototype/build.sh
run weread weread-prototype/build-native.sh
run window window-prototype/build-native.sh
run priority launcher-prototype/verified-priority95/build.sh
run half libra2-prototype/build-native.sh
run native libra2-prototype/build-native-resolution.sh
# Programs without a Codex build script: same flags as their neighbours.
static(){ "$zig" cc -target arm-linux-musleabi -mcpu=cortex_a9 -static -O2 -Wall -Wextra "$1" -o "$2"; }
static "$up/runtime-prototype/android-log-capture.c" "$out/lab/android-log-capture"
static "$up/runtime-prototype/vm-helper-smoke.c" "$out/lab/vm-helper-smoke"
# libkobo_vm.so: Codex built it with Apple clang 14 (no unwind tables); without
# -fno-unwind-tables zig's clang 18 adds an import of __aeabi_unwind_cpp_pr0.
# Same imports and exports as Codex's copy; the code differs by compiler only.
"$zig" cc -target arm-linux-gnueabi -mcpu=cortex_a9 -O2 -shared -fPIC -nostdlib -fno-stack-protector \
 -fno-unwind-tables -fno-asynchronous-unwind-tables \
 "$up/module-prototype/kobo-vm-shim.c" "$KOBO_GOOGLE_SYSTEM/lib/libc.so" -o "$out/lab/libkobo_vm.so"
# Our own: Android animation scales (src/anim) for lab/fast.
mkdir -p "$out/tmp/anim/c" "$out/tmp/anim/d"
"$KOBO_JAVA_HOME/bin/javac" -nowarn -encoding UTF-8 -source 8 -target 8 -bootclasspath "$KOBO_ANDROID_JAR" -d "$out/tmp/anim/c" "$top/src/anim/FastAnimationScales.java" 2>/dev/null
"$KOBO_JAVA_HOME/bin/java" -cp "$KOBO_D8_JAR" com.android.tools.r8.D8 --min-api 24 --lib "$KOBO_ANDROID_JAR" --output "$out/tmp/anim/d" "$out/tmp/anim/c/"*.class
cp "$out/tmp/anim/d/classes.dex" "$out/lab/fast/animation-scales.dex"

t=$out/tmp
take(){ cp "$t/$1" "$out/lab/$2"; }
take activity/process-broker process-broker
take activity/vold-socket-exec vold-socket-exec
take activity/fd-clean-exec fd-clean-exec
take activity/legacy-exec legacy-exec
take activity/app-uid-exec app-uid-exec
take window/android-socket-exec android-socket-exec
take window/installd-ping installd-ping
take install/system-server-exec system-server-exec
take install/install-android-socket-exec install-android-socket-exec
take install/install-vold-socket-exec install-vold-socket-exec
take install/install-process-broker install-process-broker
take install-java/install-server-launcher.dex install-server-launcher.dex
take install-java/install-framework-probe.dex install-framework-probe.dex
take install-java/framework-probe.dex framework-probe.dex
take install-java/server-launcher.dex server-launcher.dex
take install-java/property-server.so property-server.so
take network/dns-proxy dns-proxy
take network/network-uid-exec network-uid-exec
take weread/native-audio-exec native-audio-exec
take priority/priority-broker priority-broker
take priority/priority95-client priority95-client
# preloaded by the process broker / system-server / vold launchers (compiled-in paths)
take install/libkobo_system_uid.so libkobo_system_uid.so
take activity/libkobo_vold_scope.so libkobo_vold_scope.so
take network/libkobo_ipv4.so libkobo_ipv4.so
take network/network-probe.dex network-probe.dex
take half/libkobo_fb.so libkobo_fb.so
take half/virtual-fb-probe.so virtual-fb-probe.so
take half/live-display live-display
take half/touch-forward touch-forward
take half/host-default-network host-default-network
for f in property-server.so virtual-fb-probe.so libkobo_fb.so live-display touch-forward native-check.dex;do take native/$f native-resolution/$f;done
take native/entry/weread-process-entry.dex native-resolution/weread-process-entry.dex
cp "$out/lab/native-resolution/weread-process-entry.dex" "$out/lab/weread-process-entry.dex"
if command -v sha256sum >/dev/null 2>&1;then h=sha256sum;else h="shasum -a 256";fi
(cd "$out/lab" && find . -type f | sort | while read -r f;do $h "$f";done) > "$out/runtime-sha256.txt"
echo "RUNTIME_DONE files=$(wc -l < "$out/runtime-sha256.txt" | tr -d ' ') out=$out/lab"
