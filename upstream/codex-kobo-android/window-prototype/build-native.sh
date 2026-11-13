#!/bin/sh
set -eu
: "${KOBO_ZIG_ROOT:?Set to extracted Zig 0.13 directory}"
source_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
out=${KOBO_BUILD_OUT:-"$source_dir/build"}
mkdir -p "$out"
for program in android-socket-exec installd-ping; do
 "$KOBO_ZIG_ROOT/zig" cc -target arm-linux-musleabi -mcpu=cortex_a9 -static -O2 "$source_dir/$program.c" -o "$out/$program"
done
