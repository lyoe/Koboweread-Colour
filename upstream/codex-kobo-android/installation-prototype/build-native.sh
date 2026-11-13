#!/bin/sh
set -eu
: "${KOBO_ZIG_ROOT:?}" "${KOBO_ANDROID_ROOT:?}"
source_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
out=${KOBO_BUILD_OUT:-"$source_dir/build"}
mkdir -p "$out"
cc="$KOBO_ZIG_ROOT/zig"
static_binary(){
 "$cc" cc -target arm-linux-musleabi -mcpu=cortex_a9 -static -O2 -Wall -Wextra "$source_dir/$1.c" -o "$out/$2"
}
static_binary system-server-exec system-server-exec
static_binary android-socket-exec install-android-socket-exec
static_binary vold-socket-exec install-vold-socket-exec
static_binary process-broker install-process-broker
static_binary image-tool-exec image-tool-exec
lib="$KOBO_ANDROID_ROOT/system/lib"
"$cc" cc -target arm-linux-gnueabi -mcpu=cortex_a9 -O2 -shared -fPIC -nostdlib -fno-stack-protector "$source_dir/system-server-identity.c" "$lib/libc.so" -o "$out/libkobo_system_uid.so"
"$cc" cc -target arm-linux-gnueabi -mcpu=cortex_a9 -O2 -shared -fPIC -nostdlib -fno-stack-protector -Wno-builtin-requires-header "$source_dir/property-server.c" "$lib/libc.so" -o "$out/install-property-server.so"
