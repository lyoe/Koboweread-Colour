#!/bin/sh
set -eu
: "${KOBO_ANDROID_ROOT:?}" "${KOBO_ZIG_ROOT:?}" "${KOBO_JAVA_HOME:?}" "${KOBO_ANDROID_JAR:?}" "${KOBO_D8_JAR:?}" "${KOBO_BUILD_OUT:?}"
src=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
mkdir -p "$KOBO_BUILD_OUT/classes" "$KOBO_BUILD_OUT/dex"
"$KOBO_ZIG_ROOT/zig" cc -target arm-linux-musleabi -mcpu=cortex_a9 -static -O2 -Wall -Wextra "$src/dns-proxy.c" -o "$KOBO_BUILD_OUT/dns-proxy"
"$KOBO_ZIG_ROOT/zig" cc -target arm-linux-musleabi -mcpu=cortex_a9 -static -O2 -Wall -Wextra "$src/network-uid-exec.c" -o "$KOBO_BUILD_OUT/network-uid-exec"
"$KOBO_ZIG_ROOT/zig" cc -target arm-linux-musleabi -mcpu=cortex_a9 -static -O2 -Wall -Wextra "$src/webview-relay.c" -o "$KOBO_BUILD_OUT/webview-relay"
"$KOBO_ZIG_ROOT/zig" cc -target arm-linux-musleabi -mcpu=cortex_a9 -static -O2 -Wall -Wextra -Wno-sign-compare "$src/host-default-network.c" -o "$KOBO_BUILD_OUT/host-default-network"
"$KOBO_ZIG_ROOT/zig" cc -target arm-linux-musleabi -mcpu=cortex_a9 -static -O2 -Wall -Wextra "$src/host-default-network-probe.c" -o "$KOBO_BUILD_OUT/host-default-network-probe"
headers="$KOBO_ZIG_ROOT/lib/libc/include"
"$KOBO_ZIG_ROOT/zig" cc -target arm-linux-gnueabi -mcpu=cortex_a9 -O2 -shared -fPIC -nostdlib -fno-stack-protector -isystem "$headers/arm-linux-musl" -isystem "$headers/generic-musl" -isystem "$headers/arm-linux-any" -isystem "$headers/any-linux-any" "$src/ipv4-sockets.c" "$KOBO_ANDROID_ROOT/system/lib/libc.so" -o "$KOBO_BUILD_OUT/libkobo_ipv4.so"
"$KOBO_JAVA_HOME/bin/javac" -encoding UTF-8 -source 8 -target 8 -bootclasspath "$KOBO_ANDROID_JAR" -d "$KOBO_BUILD_OUT/classes" "$src/NetworkProbe.java"
"$KOBO_JAVA_HOME/bin/java" -cp "$KOBO_D8_JAR" com.android.tools.r8.D8 --min-api 24 --lib "$KOBO_ANDROID_JAR" --output "$KOBO_BUILD_OUT/dex" "$KOBO_BUILD_OUT/classes/"*.class
cp "$KOBO_BUILD_OUT/dex/classes.dex" "$KOBO_BUILD_OUT/network-probe.dex"
