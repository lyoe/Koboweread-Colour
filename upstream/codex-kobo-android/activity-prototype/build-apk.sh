#!/bin/sh
# Rebuild only our bootstrap/test APK, using an explicitly supplied existing key.
set -eu
: "${KOBO_JAVA_HOME:?Set JDK 17 directory}"
: "${KOBO_ANDROID_JAR:?Set API 24 android.jar}"
: "${KOBO_D8_JAR:?Set D8 jar}"
: "${KOBO_AAPT2:?Set aapt2 executable}"
: "${KOBO_PROBE_KEYSTORE:?Set the existing private bootstrap signing keystore}"
: "${KOBO_PROBE_STORE_PASSWORD:?Set keystore password}"
: "${KOBO_PROBE_KEY_PASSWORD:?Set signing key password}"
: "${KOBO_BUILD_OUT:?Set an intermediate build directory}"
source_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
mkdir -p "$KOBO_BUILD_OUT/classes" "$KOBO_BUILD_OUT/dex"
"$KOBO_JAVA_HOME/bin/javac" -encoding UTF-8 -source 8 -target 8 -bootclasspath "$KOBO_ANDROID_JAR" -d "$KOBO_BUILD_OUT/classes" "$source_dir/src/com/kobo/lab/smoke/"*.java
"$KOBO_JAVA_HOME/bin/java" -cp "$KOBO_D8_JAR" com.android.tools.r8.D8 --min-api 24 --lib "$KOBO_ANDROID_JAR" --output "$KOBO_BUILD_OUT/dex" "$KOBO_BUILD_OUT/classes/com/kobo/lab/smoke/"*.class
"$KOBO_AAPT2" link -I "$KOBO_ANDROID_JAR" --manifest "$source_dir/AndroidManifest.xml" --min-sdk-version 24 --target-sdk-version 24 -o "$KOBO_BUILD_OUT/unsigned.apk"
zip -q -j "$KOBO_BUILD_OUT/unsigned.apk" "$KOBO_BUILD_OUT/dex/classes.dex"
"$KOBO_JAVA_HOME/bin/jarsigner" -keystore "$KOBO_PROBE_KEYSTORE" -storepass:env KOBO_PROBE_STORE_PASSWORD -keypass:env KOBO_PROBE_KEY_PASSWORD -signedjar "$KOBO_BUILD_OUT/kobo-activity-probe.apk" "$KOBO_BUILD_OUT/unsigned.apk" lab
"$KOBO_JAVA_HOME/bin/jarsigner" -verify "$KOBO_BUILD_OUT/kobo-activity-probe.apk"
