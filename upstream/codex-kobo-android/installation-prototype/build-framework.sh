#!/bin/sh
set -eu
: "${KOBO_JAVA_HOME:?}" "${KOBO_ANDROID_JAR:?}" "${KOBO_D8_JAR:?}"
source_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
out=${KOBO_BUILD_OUT:-"$source_dir/build"}
KOBO_BUILD_OUT="$out" sh "$source_dir/../framework-prototype/build.sh"
cp "$out/framework-probe.dex" "$out/install-framework-probe.dex"
mkdir -p "$out/install-launcher-classes" "$out/install-launcher-dex"
"$KOBO_JAVA_HOME/bin/javac" -encoding UTF-8 -source 8 -target 8 -bootclasspath "$KOBO_ANDROID_JAR" -d "$out/install-launcher-classes" "$source_dir/ServerLauncher.java"
"$KOBO_JAVA_HOME/bin/java" -cp "$KOBO_D8_JAR" com.android.tools.r8.D8 --min-api 24 --lib "$KOBO_ANDROID_JAR" --output "$out/install-launcher-dex" "$out/install-launcher-classes/"*.class
cp "$out/install-launcher-dex/classes.dex" "$out/install-server-launcher.dex"
