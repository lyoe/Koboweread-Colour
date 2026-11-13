#!/bin/sh
# Disposable local test APK, never a modification of WeRead.
set -eu
: "${KOBO_JAVA_HOME:?}" "${KOBO_ANDROID_JAR:?}" "${KOBO_D8_JAR:?}" "${KOBO_AAPT2:?}"
source_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
out=${KOBO_BUILD_OUT:-"$source_dir/build"}
mkdir -p "$out/classes" "$out/dex"
"$KOBO_JAVA_HOME/bin/javac" -encoding UTF-8 -source 8 -target 8 -bootclasspath "$KOBO_ANDROID_JAR" -d "$out/classes" "$source_dir/src/com/kobo/lab/smoke/ProbeApplication.java" "$source_dir/src/com/kobo/lab/smoke/ProbeActivity.java" "$source_dir/src/com/kobo/lab/smoke/HomeActivity.java"
"$KOBO_JAVA_HOME/bin/java" -cp "$KOBO_D8_JAR" com.android.tools.r8.D8 --min-api 24 --lib "$KOBO_ANDROID_JAR" --output "$out/dex" "$out/classes/com/kobo/lab/smoke/"*.class
"$KOBO_AAPT2" link -I "$KOBO_ANDROID_JAR" --manifest "$source_dir/AndroidManifest.xml" --min-sdk-version 24 --target-sdk-version 24 -o "$out/unsigned.apk"
(cd "$out/dex" && zip -q ../unsigned.apk classes.dex)
key=${KOBO_LAB_KEYSTORE:-"$out/lab-test.jks"}
if [ ! -f "$key" ];then
 "$KOBO_JAVA_HOME/bin/keytool" -genkeypair -keystore "$key" -storepass kobolabtest -keypass kobolabtest -alias lab -keyalg RSA -keysize 2048 -validity 3650 -dname 'CN=Kobo Lab Test, O=Local Experiment'
fi
"$KOBO_JAVA_HOME/bin/jarsigner" -keystore "$key" -storepass kobolabtest -keypass kobolabtest -signedjar "$out/kobo-activity-probe.apk" "$out/unsigned.apk" lab
"$KOBO_JAVA_HOME/bin/jarsigner" -verify "$out/kobo-activity-probe.apk"
