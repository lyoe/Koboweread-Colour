#!/bin/sh
# Build the optional AOSP PinyinIME APK from a pinned upstream checkout.
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
root=$(CDPATH= cd -- "$here/../../.." && pwd)
tools=${KOBO_TOOLS:-$root/tools}
: "${KOBO_BUILD_OUT:?Set a fresh output directory}" "${KOBO_PINYIN_SOURCE:?Set the upstream checkout}" "${KOBO_AIDL:?Set the Android SDK aidl path}" "${KOBO_ANDROID_CC:?Set the NDK armv7a-linux-androideabi24-clang++ path}" "${KOBO_PROBE_STORE_PASSWORD:?Set the APK signing password}"
revision=05cc8f8f2465553bc6d19b2ede24073d079eeed7
out=$KOBO_BUILD_OUT
mkdir -p "$out"
out=$(cd "$out" && pwd)
mkdir "$out/work"
work=$out/work
git -C "$KOBO_PINYIN_SOURCE" archive "$revision" | tar -xf - -C "$work"
jdk=$tools/jdk17; [ ! -d "$jdk/Contents/Home" ] || jdk=$jdk/Contents/Home
android=$tools/android
cd "$work"
# Keep the keyboard and decoder in one process with no network or contacts access.
perl -ni -e 'print unless /uses-permission/' AndroidManifest.xml
perl -pi -e 's/android:exported="true"/android:exported="false"/; s/<application /<application android:hardwareAccelerated="false" android:directBootAware="true" android:defaultToDeviceProtectedStorage="true" /' AndroidManifest.xml
perl -pi -e 's/com\.android\.internal\.R\.string\.inputMethod/R.string.ime_name/; s/InputMethodManager\.getInstance\(\)/((InputMethodManager) getSystemService(INPUT_METHOD_SERVICE))/' src/com/android/inputmethod/pinyin/PinyinIME.java
perl -pi -e 's/#include <cutils\/log.h>/#include <android\/log.h>\n#define PINYIN_FATAL_IF(condition, ...) if (condition) __android_log_assert(#condition, "PinyinIME", __VA_ARGS__)\n#define ALOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "PinyinIME", __VA_ARGS__)/' jni/android/*.cpp jni/share/userdict.cpp
perl -pi -e 's/GetStringChars\(fixed_str, false\)/GetStringChars(fixed_str, nullptr)/; s/LOG_FATAL_IF/PINYIN_FATAL_IF/g' jni/android/*.cpp
perl -pi -e 's/android:isDefault="\@bool\/im_is_default"/android:isDefault="false"/' res/xml/method.xml
# Use public View APIs and calibrate for directly mapped Kobo touch coordinates.
perl -pi -e 's/\bmPadding(Left|Right|Top|Bottom)\b/getPadding$1()/g' src/com/android/inputmethod/pinyin/*.java
perl -pi -e 's/\bmContext\b/getContext()/g' src/com/android/inputmethod/pinyin/SkbContainer.java src/com/android/inputmethod/pinyin/SoftKeyboardView.java
perl -0777 -pi -e 's/import android\.os\.SystemProperties;\n//; s/if \("1"\.equals\(SystemProperties.*?mYBiasCorrection = Y_BIAS_CORRECTION;\s*}/mYBiasCorrection = 0;/s' src/com/android/inputmethod/pinyin/SkbContainer.java
# Custom Canvas views need a CJK face in the trimmed Android font setup.
cat > src/com/android/inputmethod/pinyin/CandidateFont.java <<'JAVA'
package com.android.inputmethod.pinyin;

import android.graphics.Paint;
import android.graphics.Typeface;
import java.util.Locale;

final class CandidateFont {
    private static Typeface face;

    static void apply(Paint paint) {
        if (face == null) {
            try {
                face = Typeface.createFromFile("/system/fonts/NotoSansCJK-Regular.ttc");
            } catch (RuntimeException unavailable) {
                face = Typeface.DEFAULT;
            }
        }
        paint.setTypeface(face);
        paint.setTextLocale(Locale.SIMPLIFIED_CHINESE);
    }
}
JAVA
perl -pi -e 's/(mCandidatesPaint = new Paint\(\);)/$1\n        CandidateFont.apply(mCandidatesPaint);/' src/com/android/inputmethod/pinyin/CandidateView.java
perl -pi -e 's/(mPaint = new Paint\(\);)/$1\n        CandidateFont.apply(mPaint);/' src/com/android/inputmethod/pinyin/ComposingView.java
python3 -I "$here/eink-ui.py"
if [ "${KOBO_PINYIN_TRACE:-0}" = 1 ];then
 python3 -I "$here/latency-trace.py"
fi
mkdir -p generated classes dex lib/armeabi-v7a
"$KOBO_AIDL" -o generated lib/com/android/inputmethod/pinyin/IPinyinDecoderService.aidl
"$android/aapt2" compile --dir res -o res.zip
"$android/aapt2" link -I "$android/android.jar" --manifest AndroidManifest.xml --min-sdk-version 24 --target-sdk-version 24 -0 dat --java generated -o "$out/PinyinIME.unsigned.apk" res.zip
find src generated -name '*.java' > sources
"$jdk/bin/javac" -encoding UTF-8 -source 8 -target 8 -bootclasspath "$android/android.jar" -d classes @sources
"$jdk/bin/java" -cp "$android/d8.jar" com.android.tools.r8.D8 --min-api 24 --lib "$android/android.jar" --output dex $(find classes -name '*.class')
"$KOBO_ANDROID_CC" -std=c++11 -O2 -shared -fPIC -fno-exceptions -fno-rtti -static-libstdc++ jni/android/*.cpp jni/share/*.cpp -llog -o lib/armeabi-v7a/libjni_pinyinime.so
zip -q -0 "$out/PinyinIME.unsigned.apk" lib/armeabi-v7a/libjni_pinyinime.so
(cd dex && zip -q "$out/PinyinIME.unsigned.apk" classes.dex)
key=${KOBO_KEYSTORE:-$tools/keys/activity-test-key.jks}
"$jdk/bin/jarsigner" -keystore "$key" -storepass:env KOBO_PROBE_STORE_PASSWORD -keypass:env KOBO_PROBE_STORE_PASSWORD -signedjar "$out/PinyinIME.apk" "$out/PinyinIME.unsigned.apk" lab
"$jdk/bin/jarsigner" -verify "$out/PinyinIME.apk"
cp NOTICE "$out/PinyinIME.NOTICE"
cp lib/armeabi-v7a/libjni_pinyinime.so "$out/"
(cd "$out"; for f in PinyinIME.apk libjni_pinyinime.so;do shasum -a 256 "$f" | cut -d' ' -f1 > "$f.sha256";done)
