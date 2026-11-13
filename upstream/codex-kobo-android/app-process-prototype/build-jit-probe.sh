#!/bin/sh
set -eu
: "${KOBO_ZIG_ROOT:?}" "${KOBO_JAVA_HOME:?}" "${KOBO_ANDROID_ROOT:?}" "${KOBO_ANDROID_JAR:?}" "${KOBO_D8_JAR:?}" "${KOBO_BUILD_OUT:?}"
src=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
mkdir -p "$KOBO_BUILD_OUT/classes" "$KOBO_BUILD_OUT/dex"
"$KOBO_ZIG_ROOT/zig" cc -target arm-linux-musleabi -mcpu=cortex_a9 -static -O2 -Wall -Wextra "$src/jit-uid-exec.c" -o "$KOBO_BUILD_OUT/jit-uid-exec"
h="$KOBO_ZIG_ROOT/lib/libc/include"
"$KOBO_ZIG_ROOT/zig" cc -target arm-linux-gnueabi -mcpu=cortex_a9 -O2 -shared -fPIC -nostdlib -fno-stack-protector -isystem "$h/arm-linux-musl" -isystem "$h/generic-musl" -isystem "$h/arm-linux-any" -isystem "$h/any-linux-any" "$src/jit-config.c" "$KOBO_ANDROID_ROOT/system/lib/libc.so" -o "$KOBO_BUILD_OUT/libkobo_jit_config.so"
"$KOBO_JAVA_HOME/bin/javac" -encoding UTF-8 -source 8 -target 8 -bootclasspath "$KOBO_ANDROID_JAR" -d "$KOBO_BUILD_OUT/classes" "$src/JitProbe.java"
"$KOBO_JAVA_HOME/bin/java" -cp "$KOBO_D8_JAR" com.android.tools.r8.D8 --min-api 24 --lib "$KOBO_ANDROID_JAR" --output "$KOBO_BUILD_OUT/dex" "$KOBO_BUILD_OUT/classes/JitProbe.class"
cp "$KOBO_BUILD_OUT/dex/classes.dex" "$KOBO_BUILD_OUT/jit-probe.dex"
