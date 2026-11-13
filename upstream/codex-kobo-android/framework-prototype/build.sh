#!/bin/sh
# Compile local probes against the matching private Android API 24 runtime.
set -eu
: "${KOBO_ZIG_ROOT:?Set to the extracted Zig 0.13 directory}"
: "${KOBO_JAVA_HOME:?Set to a JDK 17 directory}"
: "${KOBO_ANDROID_ROOT:?Set to the staged Android root containing system/lib}"
: "${KOBO_ANDROID_JAR:?Set to Android API 24 android.jar}"
: "${KOBO_D8_JAR:?Set to the Android build tools D8 jar}"
source_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
out=${KOBO_BUILD_OUT:-"$source_dir/build"}
mkdir -p "$out/classes" "$out/dex"
cc="$KOBO_ZIG_ROOT/zig"
lib="$KOBO_ANDROID_ROOT/system/lib"
build_shared() {
 file=$1;shift
 "$cc" cc -target arm-linux-gnueabi -mcpu=cortex_a9 -O2 -shared -fPIC -nostdlib -fno-stack-protector "$source_dir/$file.c" "$lib/libc.so" "$@" -o "$out/$file.so"
}
build_shared property-server
build_shared egl-probe "$lib/libEGL.so" "$lib/libGLESv1_CM.so" "$lib/libdl.so"
build_shared egl-driver-probe "$lib/libdl.so"
headers="$KOBO_ZIG_ROOT/lib/libc/include"
build_shared kobo-virtual-fb -isystem "$headers/arm-linux-any" -isystem "$headers/any-linux-any"
mv "$out/kobo-virtual-fb.so" "$out/libkobo_fb.so"
build_shared virtual-fb-probe -isystem "$headers/arm-linux-any" -isystem "$headers/any-linux-any"
"$KOBO_JAVA_HOME/bin/javac" -encoding UTF-8 -source 8 -target 8 -bootclasspath "$KOBO_ANDROID_JAR" -d "$out/classes" "$source_dir/FrameworkProbe.java" "$source_dir/SurfaceProbe.java" "$source_dir/../window-prototype/WindowClient.java" "$source_dir/ActivityLauncher.java" "$source_dir/OfflineSyncAdapterQueries.java" "$source_dir/OfflineConnectivityService.java" "$source_dir/HostDefaultConnectivityService.java" "$source_dir/HostWifiQueries.java" "$source_dir/NoOutputAudioQueries.java" "$source_dir/WebViewBootstrap.java" "$source_dir/InstallProbe.java" "$source_dir/ApkIdentityCache.java" "$source_dir/LiveFocusService.java" "$source_dir/AccessibilityBootstrap.java" "$source_dir/SystemSamplingService.java" "$source_dir/StartupServiceGrace.java" "$source_dir/StartupServiceGracePolicy.java" "$source_dir/PackageProcessRegistry.java"
"$KOBO_JAVA_HOME/bin/java" -cp "$KOBO_D8_JAR" com.android.tools.r8.D8 --min-api 24 --lib "$KOBO_ANDROID_JAR" --output "$out/dex" "$out/classes/"*.class
cp "$out/dex/classes.dex" "$out/framework-probe.dex"
mkdir -p "$out/launcher-classes" "$out/launcher-dex"
"$KOBO_JAVA_HOME/bin/javac" -encoding UTF-8 -source 8 -target 8 -bootclasspath "$KOBO_ANDROID_JAR" -d "$out/launcher-classes" "$source_dir/../window-prototype/ServerLauncher.java"
"$KOBO_JAVA_HOME/bin/java" -cp "$KOBO_D8_JAR" com.android.tools.r8.D8 --min-api 24 --lib "$KOBO_ANDROID_JAR" --output "$out/launcher-dex" "$out/launcher-classes/"*.class
cp "$out/launcher-dex/classes.dex" "$out/server-launcher.dex"
printf 'Built probes in %s\n' "$out"
