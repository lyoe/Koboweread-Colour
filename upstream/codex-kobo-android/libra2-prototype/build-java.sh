#!/bin/sh
set -eu
: "${KOBO_JAVA_HOME:?}" "${KOBO_ANDROID_JAR:?}" "${KOBO_D8_JAR:?}" "${KOBO_BUILD_OUT:?}"
src=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
out=$KOBO_BUILD_OUT
mkdir -p "$out/source" "$out/classes" "$out/dex"
# Production baseline94 includes accessibility and focus leases, excludes profiler96.
sed 's/captureVirtualFramebuffer(720,960)/captureVirtualFramebuffer(632,840)/' "$src/launcher-prototype/verified-system96/FrameworkProbe-before96.java" > "$out/source/FrameworkProbe.java"
f="$src/framework-prototype"
sed 's/eth0/wlan0/g' "$f/HostDefaultConnectivityService.java" > "$out/source/HostDefaultConnectivityService.java"
"$KOBO_JAVA_HOME/bin/javac" -encoding UTF-8 -source 8 -target 8 -bootclasspath "$KOBO_ANDROID_JAR" -d "$out/classes" "$out/source/FrameworkProbe.java" "$src/libra2-prototype/Libra2RuntimeCheck.java" "$f/SurfaceProbe.java" "$f/../window-prototype/WindowClient.java" "$f/ActivityLauncher.java" "$f/OfflineSyncAdapterQueries.java" "$f/OfflineConnectivityService.java" "$out/source/HostDefaultConnectivityService.java" "$f/HostWifiQueries.java" "$f/NoOutputAudioQueries.java" "$f/WebViewBootstrap.java" "$f/InstallProbe.java" "$f/ApkIdentityCache.java" "$f/LiveFocusService.java" "$f/AccessibilityBootstrap.java" "$f/StartupServiceGrace.java" "$f/StartupServiceGracePolicy.java" "$f/PackageProcessRegistry.java"
"$KOBO_JAVA_HOME/bin/java" -cp "$KOBO_D8_JAR" com.android.tools.r8.D8 --min-api 24 --lib "$KOBO_ANDROID_JAR" --output "$out/dex" "$out/classes/"*.class
cp "$out/dex/classes.dex" "$out/install-framework-probe.dex"
KOBO_BUILD_OUT="$out/entry" sh "$src/weread-prototype/build-entry.sh"
