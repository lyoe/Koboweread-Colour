#!/bin/sh
# Build WeRead 1.8.8's JIT/heap/Skia shim (fast-v188/libkobo_jit_config.so):
#   sh src/resident-v188/shim/build-shim.sh OUT.so [SOURCE.c]
# Forked from src/jit-config.c (1.9.8's, fast/) with the same flags as
# src/BUILD.sh, so the two versions can be optimized apart.
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
root=$(CDPATH= cd -- "$here/../../.." && pwd)
tools=${KOBO_TOOLS:-$root/tools}  # toolchain directory (bootstrap.sh)
jdk_home=$tools/jdk17/Contents/Home;[ -d "$jdk_home" ] || jdk_home=$tools/jdk17  # macOS bundle or Linux layout
out=$1;src=${2:-$here/jit-config.c}
h=$tools/ziglang/lib/libc/include
export ZIG_GLOBAL_CACHE_DIR=$root/build/zig-cache ZIG_LOCAL_CACHE_DIR=$root/build/zig-cache
"$tools/ziglang/zig" cc -I"$here" -I"$jdk_home/include" -target arm-linux-gnueabi -mcpu=cortex_a9 -mfpu=neon -mfloat-abi=softfp -O2 -shared -fPIC -nostdlib -fno-stack-protector -fno-unwind-tables -fno-asynchronous-unwind-tables \
 -isystem "$h/arm-linux-musl" -isystem "$h/generic-musl" -isystem "$h/arm-linux-any" -isystem "$h/any-linux-any" \
 "$src" "$tools/sysroot/system/lib/libc.so" "$tools/sysroot/system/lib/libdl.so" -o "$out"
