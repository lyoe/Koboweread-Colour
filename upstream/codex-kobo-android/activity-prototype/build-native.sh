#!/bin/sh
set -eu
: "${KOBO_ZIG_ROOT:?}" "${KOBO_ANDROID_ROOT:?}"
source_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
out=${KOBO_BUILD_OUT:-"$source_dir/build"}
mkdir -p "$out"
cc="$KOBO_ZIG_ROOT/zig"
for name in process-broker vold-socket-exec fd-clean-exec;do
 "$cc" cc -target arm-linux-musleabi -mcpu=cortex_a9 -static -O2 -Wall -Wextra "$source_dir/$name.c" -o "$out/$name"
done
"$cc" cc -target arm-linux-gnueabi -mcpu=cortex_a9 -O2 -shared -fPIC -nostdlib -fno-stack-protector -fno-builtin "$source_dir/kobo-vold-scope.c" "$KOBO_ANDROID_ROOT/system/lib/libc.so" "$KOBO_ANDROID_ROOT/system/lib/libdl.so" -o "$out/libkobo_vold_scope.so"

"$cc" cc -target arm-linux-musleabi -mcpu=cortex_a9 -static -O2 -Wall -Wextra "$source_dir/../runtime-prototype/android-legacy-exec.c" -o "$out/legacy-exec"
"$cc" cc -target arm-linux-musleabi -mcpu=cortex_a9 -static -O2 -Wall -Wextra "$source_dir/../window-prototype/android-socket-exec.c" -o "$out/android-socket-exec"
"$cc" cc -target arm-linux-musleabi -mcpu=cortex_a9 -static -O2 -Wall -Wextra "$source_dir/../app-process-prototype/app-uid-exec.c" -o "$out/app-uid-exec"
