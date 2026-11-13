#!/bin/sh
set -eu
: "${KOBO_ZIG_ROOT:?}" "${KOBO_BUILD_OUT:?}"
src=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
mkdir -p "$KOBO_BUILD_OUT"
for name in priority-broker priority95-client;do
 "$KOBO_ZIG_ROOT/zig" cc -target arm-linux-musleabi -mcpu=cortex_a9 -static -O2 -Wall -Wextra "$src/$name.c" -o "$KOBO_BUILD_OUT/$name"
done
