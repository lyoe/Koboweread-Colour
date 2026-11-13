#!/bin/sh
# Prepare a fresh checkout for building (docs/公开版设计.md):
#   sh bootstrap.sh
# Downloads, at fixed versions and checked against fixed SHA-256 values:
#   zig 0.13.0, Eclipse Temurin JDK 17.0.20.1+1, Android platform android-24
#   (android.jar), build-tools 30.0.3 (d8.jar), aapt2 8.4.0-11315950,
#   e2fsprogs 1.47.1 source, and Google's Android 7.0 (API 24) ARM system
#   image armeabi-v7a-24_r07.
# and prepares tools/ (KOBO_TOOLS) and build/google/ (KOBO_GOOGLE): Google's
# /system and ramdisk extracted with a debugfs built here from e2fsprogs.
# Google's files are subject to the Android Software Development Kit License
# Agreement (https://developer.android.com/studio/terms); you accept it by
# answering "yes" below or with KOBO_ACCEPT_ANDROID_SDK_LICENSE=1.
# Hosts: macOS (arm64, x86_64), Linux x86_64. Needs: curl, tar, unzip,
# python3, perl, make, a C compiler (Xcode Command Line Tools / gcc), cpio,
# gzip, zip.
set -eu
top=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
tools=${KOBO_TOOLS:-$top/tools}
G=${KOBO_GOOGLE:-$top/build/google}
dl=$tools/dl
mkdir -p "$dl"
say(){ echo "BOOTSTRAP $*"; }
need(){ for c in "$@";do command -v "$c" >/dev/null 2>&1 || { echo "BOOTSTRAP_MISSING_COMMAND $c" >&2;exit 2; };done; }
need curl tar unzip python3 perl make cpio gzip zip cc
sha256(){ if command -v sha256sum >/dev/null 2>&1;then sha256sum "$1" | cut -c1-64;else shasum -a 256 "$1" | cut -c1-64;fi; }
fetch(){ # url sha256 file
 if [ -f "$dl/$3" ] && [ "$(sha256 "$dl/$3")" = "$2" ];then return 0;fi
 say "download $3"
 curl -fsSL --retry 3 -o "$dl/$3.part" "$1"
 [ "$(sha256 "$dl/$3.part")" = "$2" ] || { echo "BOOTSTRAP_CHECKSUM_MISMATCH $3" >&2;rm -f "$dl/$3.part";exit 3; }
 mv "$dl/$3.part" "$dl/$3"
}
case "$(uname -s)-$(uname -m)" in
 Darwin-arm64) zig=zig-macos-aarch64-0.13.0;zig_sha=46fae219656545dfaf4dce12fb4e8685cec5b51d721beee9389ab4194d43394c
  jdk=aarch64_mac;jdk_sha=196d13ba5f10414bef7f6a05a9b3f00edacb18ebacef2b99485db9e2ee18f0e8
  aapt=osx;aapt_sha=05d9ece54eb37b7b6fcf76df1a379b5b29543fb84adab2804446cf36bdad4477;;
 Darwin-x86_64) zig=zig-macos-x86_64-0.13.0;zig_sha=8b06ed1091b2269b700b3b07f8e3be3b833000841bae5aa6a09b1a8b4773effd
  jdk=x64_mac;jdk_sha=c01975da12ed4235250ff891fe8bba73a9e73037d444b269c9d0922b5dbc8e0a
  aapt=osx;aapt_sha=05d9ece54eb37b7b6fcf76df1a379b5b29543fb84adab2804446cf36bdad4477;;
 Linux-x86_64) zig=zig-linux-x86_64-0.13.0;zig_sha=d45312e61ebcc48032b77bc4cf7fd6915c11fa16e4aad116b66c9468211230ea
  jdk=x64_linux;jdk_sha=3808d1d15e3ec6bd5b84057fb5d84c33d8a1536a258146bcea2e603fc726e08e
  aapt=linux;aapt_sha=5638a5b151f9c50ba2a9e5291edfc9227aec3fedc3ee8f8a8583fa54cd1dd9ca;;
 *) echo "BOOTSTRAP_UNSUPPORTED_HOST $(uname -s)-$(uname -m) (macOS arm64/x86_64 or Linux x86_64)" >&2;exit 2;;
esac

if [ "${KOBO_ACCEPT_ANDROID_SDK_LICENSE:-0}" != 1 ];then
 echo "Google's Android platform, build tools and system image are needed, under the"
 echo "Android Software Development Kit License Agreement: https://developer.android.com/studio/terms"
 printf 'Do you accept it? (yes/no) ';read -r answer
 [ "$answer" = yes ] || { echo "BOOTSTRAP_LICENSE_NOT_ACCEPTED" >&2;exit 4; }
fi

# 1. zig
fetch "https://ziglang.org/download/0.13.0/$zig.tar.xz" $zig_sha $zig.tar.xz
if [ ! -x "$tools/ziglang/zig" ];then
 rm -rf "$tools/ziglang" "$tools/$zig";python3 -I -c 'import sys,tarfile;tarfile.open(sys.argv[1]).extractall(sys.argv[2])' "$dl/$zig.tar.xz" "$tools"
 mv "$tools/$zig" "$tools/ziglang";say "zig $("$tools/ziglang/zig" version)"
fi
# 2. JDK 17
j=OpenJDK17U-jdk_${jdk}_hotspot_17.0.20.1_1.tar.gz
fetch "https://github.com/adoptium/temurin17-binaries/releases/download/jdk-17.0.20.1%2B1/$j" $jdk_sha $j
if [ ! -d "$tools/jdk17" ];then
 rm -rf "$tools/jdk-17.0.20.1+1";tar -xzf "$dl/$j" -C "$tools";mv "$tools/jdk-17.0.20.1+1" "$tools/jdk17";say "jdk 17.0.20.1+1"
fi
# 3. Android: android.jar (platform 24), d8.jar (build-tools 30.0.3), aapt2.
mkdir -p "$tools/android"
B=https://dl.google.com/android/repository
fetch $B/platform-24_r02.zip f268f5945c6ece7ea95c1c252067280854d2a20da924e22ae4720287df8bdbc9 platform-24_r02.zip
fetch $B/build-tools_r30.0.3-linux.zip 24593500aa95d2f99fb4f10658aae7e65cb519be6cd33fa164f15f27f3c4a2d6 build-tools_r30.0.3-linux.zip
fetch "https://dl.google.com/android/maven2/com/android/tools/build/aapt2/8.4.0-11315950/aapt2-8.4.0-11315950-$aapt.jar" $aapt_sha aapt2-$aapt.jar
unzip -p "$dl/platform-24_r02.zip" 'android-7.0/android.jar' > "$tools/android/android.jar"
unzip -p "$dl/build-tools_r30.0.3-linux.zip" 'android-11/lib/d8.jar' > "$tools/android/d8.jar"
unzip -p "$dl/aapt2-$aapt.jar" aapt2 > "$tools/android/aapt2";chmod 755 "$tools/android/aapt2"
for c in "android.jar 4c438bf1b3d4e6104faccc62e1ffe95ef197b88055497b540a871349e93c8685" "d8.jar 3958eda74eb298337361e974c9b733856eae08795516bf78a0dd16df620e83bc";do
 set -- $c;[ "$(sha256 "$tools/android/$1")" = "$2" ] || { echo "BOOTSTRAP_CHECKSUM_MISMATCH android/$1" >&2;exit 3; }
done
# 4. Headers and libraries kept in this repository (FBInk GPL-3.0, AOSP Apache-2.0).
ln -sfn "$top/third_party/fbink" "$tools/fbink"
ln -sfn "$top/third_party/aosp" "$tools/aosp"
# 5. e2fsprogs (e2fsck for the Kobo; debugfs for this computer).
fetch https://mirrors.edge.kernel.org/pub/linux/kernel/people/tytso/e2fsprogs/v1.47.1/e2fsprogs-1.47.1.tar.xz \
 5a33dc047fd47284bca4bb10c13cfe7896377ae3d01cb81a05d406025d99e0d1 e2fsprogs-1.47.1.tar.xz
debugfs=$tools/host/debugfs
if [ ! -x "$debugfs" ];then
 say "build debugfs";w=$tools/host/e2fsprogs;rm -rf "$w";mkdir -p "$w";tar -xJf "$dl/e2fsprogs-1.47.1.tar.xz" -C "$w" 2>/dev/null || \
  python3 -I -c 'import sys,tarfile;tarfile.open(sys.argv[1]).extractall(sys.argv[2])' "$dl/e2fsprogs-1.47.1.tar.xz" "$w"
 (cd "$w/e2fsprogs-1.47.1" && ./configure --disable-nls --disable-fuse2fs --disable-defrag --disable-imager --without-libarchive >../configure.log 2>&1 \
  && make -j4 libs >../make.log 2>&1 && make -j4 -C debugfs >>../make.log 2>&1) || { echo "BOOTSTRAP_DEBUGFS_FAILED (see $w)" >&2;exit 5; }
 cp "$w/e2fsprogs-1.47.1/debugfs/debugfs" "$debugfs"
fi
# 6. Google's Android 7.0 ARM system image: /system and the ramdisk.
fetch $B/sys-img/android/armeabi-v7a-24_r07.zip 42690977fe4b97b0643da11a2b42742b2e3d5b8849dcdd6893e0b92d6618c91d armeabi-v7a-24_r07.zip
if [ ! -f "$G/system/lib/libc.so" ] || [ ! -f "$G/ramdisk/file_contexts.bin" ];then
 say "extract Google image";rm -rf "$G";mkdir -p "$G/system" "$G/ramdisk" "$G/tmp"
 unzip -o -q "$dl/armeabi-v7a-24_r07.zip" armeabi-v7a/system.img armeabi-v7a/ramdisk.img -d "$G/tmp"
 "$debugfs" -R "rdump / $G/system" "$G/tmp/armeabi-v7a/system.img" 2>/dev/null || :
 [ -f "$G/system/lib/libc.so" ] || { echo "BOOTSTRAP_SYSTEM_EXTRACT_FAILED" >&2;exit 6; }
 (cd "$G/ramdisk" && gunzip -c "$G/tmp/armeabi-v7a/ramdisk.img" | cpio -id 2>/dev/null)
 rm -rf "$G/tmp"
fi
# 7. Android libraries the native builds link against (from Google's image).
mkdir -p "$tools/sysroot/system/lib/hw"
for f in libc.so libdl.so libhardware.so hw/gralloc.default.so;do cp "$G/system/lib/$f" "$tools/sysroot/system/lib/$f";done
say "done: tools in $tools, Google image in $G"
