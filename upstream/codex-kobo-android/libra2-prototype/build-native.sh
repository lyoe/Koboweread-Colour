#!/bin/sh
set -eu
: "${KOBO_ZIG_ROOT:?}" "${KOBO_FBINK_ROOT:?}" "${KOBO_ANDROID_ROOT:?}" "${KOBO_BUILD_OUT:?}"
src=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
mkdir -p "$KOBO_BUILD_OUT"
cc="$KOBO_ZIG_ROOT/zig"
set --
case "${KOBO_LIBRA2_RESOLUTION:-half}" in half) :;; native) set -- -DKOBO_LIBRA2_NATIVE;; *) exit 2;;esac
"$cc" cc "$@" -target arm-linux-musleabi -mcpu=cortex_a9 -static -O2 -DKOBO_TARGET_LIBRA2 -DFBINK_FOR_KOBO -DFBINK_MINIMAL -DFBINK_WITH_IMAGE -DFBINK_WITH_DRAW -I"$KOBO_FBINK_ROOT" -I"$KOBO_FBINK_ROOT/i2c-tools/include" "$src/display-bridge-prototype/live-display.c" "$KOBO_FBINK_ROOT/fbink.c" "$KOBO_FBINK_ROOT/qimagescale/qimagescale.c" "$KOBO_FBINK_ROOT/i2c-tools/lib/smbus.c" -lm -o "$KOBO_BUILD_OUT/live-display"
"$cc" cc "$@" -target arm-linux-musleabi -mcpu=cortex_a9 -static -O2 -Wall -Wextra -DKOBO_TARGET_LIBRA2 "$src/display-bridge-prototype/touch-forward.c" -o "$KOBO_BUILD_OUT/touch-forward"
headers="$KOBO_ZIG_ROOT/lib/libc/include"
for unit in kobo-virtual-fb virtual-fb-probe;do
 "$cc" cc "$@" -target arm-linux-gnueabi -mcpu=cortex_a9 -O2 -shared -fPIC -nostdlib -fno-stack-protector -DKOBO_TARGET_LIBRA2 -isystem "$headers/arm-linux-any" -isystem "$headers/any-linux-any" "$src/framework-prototype/$unit.c" "$KOBO_ANDROID_ROOT/system/lib/libc.so" -o "$KOBO_BUILD_OUT/$unit.so"
done
mv "$KOBO_BUILD_OUT/kobo-virtual-fb.so" "$KOBO_BUILD_OUT/libkobo_fb.so"

"$cc" cc "$@" -target arm-linux-musleabi -mcpu=cortex_a9 -static -O2 -Wall -Wextra -Wno-sign-compare -DKOBO_TARGET_LIBRA2 "$src/network-prototype/host-default-network.c" -o "$KOBO_BUILD_OUT/host-default-network"

"$cc" cc "$@" -target arm-linux-musleabi -mcpu=cortex_a9 -static -O2 -Wall -Wextra "$src/network-prototype/host-default-network-probe.c" -o "$KOBO_BUILD_OUT/host-default-network-probe"
