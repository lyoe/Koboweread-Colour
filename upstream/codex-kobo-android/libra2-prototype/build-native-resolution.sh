#!/bin/sh
set -eu
: "${KOBO_ZIG_ROOT:?}" "${KOBO_FBINK_ROOT:?}" "${KOBO_ANDROID_ROOT:?}" "${KOBO_BUILD_OUT:?}" "${KOBO_JAVA_HOME:?}" "${KOBO_ANDROID_JAR:?}" "${KOBO_D8_JAR:?}"
src=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
out=$KOBO_BUILD_OUT
KOBO_LIBRA2_RESOLUTION=native sh "$src/libra2-prototype/build-native.sh"
"$KOBO_ZIG_ROOT/zig" cc -target arm-linux-gnueabi -mcpu=cortex_a9 -O2 -shared -fPIC -nostdlib -fno-stack-protector -DKOBO_LIBRA2_NATIVE "$src/installation-prototype/property-server.c" "$KOBO_ANDROID_ROOT/system/lib/libc.so" -o "$out/property-server.so"
KOBO_BUILD_OUT="$out/entry" sh "$src/weread-prototype/build-entry.sh"
mkdir -p "$out/check-classes" "$out/check-dex"
"$KOBO_JAVA_HOME/bin/javac" -encoding UTF-8 -source 8 -target 8 -bootclasspath "$KOBO_ANDROID_JAR" -d "$out/check-classes" "$src/libra2-prototype/NativeResolutionCheck.java"
"$KOBO_JAVA_HOME/bin/java" -cp "$KOBO_D8_JAR" com.android.tools.r8.D8 --min-api 24 --lib "$KOBO_ANDROID_JAR" --output "$out/check-dex" "$out/check-classes/"*.class
cp "$out/check-dex/classes.dex" "$out/native-check.dex"
