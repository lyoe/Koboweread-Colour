#!/bin/sh
set -eu
: "${KOBO_ZIG_ROOT:?}"
source_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
out=${KOBO_BUILD_OUT:-"$source_dir/build"}
mkdir -p "$out"
"$KOBO_ZIG_ROOT/zig" cc -target arm-linux-musleabi -mcpu=cortex_a9 -static -O2 -Wall -Wextra "$source_dir/native-audio-exec.c" -o "$out/native-audio-exec"
