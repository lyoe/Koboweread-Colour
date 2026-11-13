#!/bin/sh
set -eu
: "${KOBO_JAVA_HOME:?}" "${KOBO_ANDROID_JAR:?}" "${KOBO_D8_JAR:?}"
source_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
out=${KOBO_BUILD_OUT:-"$source_dir/build"}
mkdir -p "$out/classes" "$out/dex"
"$KOBO_JAVA_HOME/bin/javac" -encoding UTF-8 -source 8 -target 8 -bootclasspath "$KOBO_ANDROID_JAR" -d "$out/classes" "$source_dir/WeReadProcessEntry.java" "$source_dir/WeReadInputProbe.java" "$source_dir/WeReadTouchProbe.java" "$source_dir/WeReadPageTouchProbe.java" "$source_dir/WeReadInteractiveInput.java" "$source_dir/LiveFocusClient.java" "$source_dir/TouchLeasePolicy.java" "$source_dir/WindowOwnership.java" "$source_dir/TouchEvents.java" "$source_dir/LoginTapDiagnostic.java" "$source_dir/AgreementPageDiagnostic.java" "$source_dir/../network-prototype/WebViewReadDiagnostics.java" "$source_dir/../network-prototype/WebViewRenderingDiagnostic.java"
"$KOBO_JAVA_HOME/bin/java" -cp "$KOBO_D8_JAR" com.android.tools.r8.D8 --min-api 24 --lib "$KOBO_ANDROID_JAR" --output "$out/dex" "$out/classes/"*.class
cp "$out/dex/classes.dex" "$out/weread-process-entry.dex"
