#!/bin/sh
set -eu
: "${KOBO_ZIG_ROOT:?}" "${KOBO_JAVA_HOME:?}" "${KOBO_ANDROID_JAR:?}" "${KOBO_D8_JAR:?}"
source_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
out=${KOBO_BUILD_OUT:-"$source_dir/build"}
mkdir -p "$out/classes" "$out/dex"
"$KOBO_ZIG_ROOT/zig" cc -target arm-linux-musleabi -mcpu=cortex_a9 -static -O2 "$source_dir/app-uid-exec.c" -o "$out/app-uid-exec"
"$KOBO_JAVA_HOME/bin/javac" -encoding UTF-8 -source 8 -target 8 -bootclasspath "$KOBO_ANDROID_JAR" -d "$out/classes" "$source_dir/AppUidProbe.java"
"$KOBO_JAVA_HOME/bin/java" -cp "$KOBO_D8_JAR" com.android.tools.r8.D8 --min-api 24 --lib "$KOBO_ANDROID_JAR" --output "$out/dex" "$out/classes/"*.class
cp "$out/dex/classes.dex" "$out/app-uid-probe.dex"
