#!/bin/sh
set -eu
: "${KOBO_ZIG_ROOT:?}" "${KOBO_FBINK_ROOT:?}"
source_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
out=${KOBO_BUILD_OUT:-"$source_dir/build"}
mkdir -p "$out"
"$KOBO_ZIG_ROOT/zig" cc -target arm-linux-musleabi -mcpu=cortex_a9 -static -O2 -DFBINK_FOR_KOBO -DFBINK_MINIMAL -DFBINK_WITH_IMAGE -DFBINK_WITH_DRAW -I"$KOBO_FBINK_ROOT" -I"$KOBO_FBINK_ROOT/i2c-tools/include" "$source_dir/live-display.c" "$KOBO_FBINK_ROOT/fbink.c" "$KOBO_FBINK_ROOT/qimagescale/qimagescale.c" "$KOBO_FBINK_ROOT/i2c-tools/lib/smbus.c" -lm -o "$out/live-display"

"$KOBO_ZIG_ROOT/zig" cc -target arm-linux-musleabi -mcpu=cortex_a9 -static -O2 -Wall -Wextra "$source_dir/input-metadata.c" -o "$out/input-metadata"

"$KOBO_ZIG_ROOT/zig" cc -target arm-linux-musleabi -mcpu=cortex_a9 -static -O2 -Wall -Wextra "$source_dir/touch-forward.c" -o "$out/touch-forward"
