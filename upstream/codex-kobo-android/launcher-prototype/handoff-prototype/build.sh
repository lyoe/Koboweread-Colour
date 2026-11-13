#!/bin/sh
set -eu
: "${KOBO_ZIG_ROOT:?}" "${KOBO_BUILD_OUT:?Use a private build directory}"
s=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
mkdir -p "$KOBO_BUILD_OUT"
for file in handoff-test WatchdogTest;do
 "$KOBO_ZIG_ROOT/zig" cc -target arm-linux-musleabi -mcpu=cortex_a9 -static -O2 -Wall -Wextra "$s/$file.c" -o "$KOBO_BUILD_OUT/$file"
done
