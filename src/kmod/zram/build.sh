#!/bin/sh
# Build kobo_zram.ko for the Libra 2 kernel (4.1.15, no zram/zsmalloc built
# in, LZO built in but not exported to modules):
#   sh src/kmod/zram/build.sh OUTDIR
# Two modules (kobo_zsmalloc.ko first, then kobo_zram.ko) from the kernel's
# own sources: drivers/block/zram (LZO backend
# only), mm/zsmalloc.c, and lib/lzo compressor/decompressor renamed to
# kobo_lzo1x_* with their exports removed, so nothing clashes with the
# kernel's symbols. The sources are copied from the prepared tree; only the
# renaming below is applied. The kernel checks every imported symbol's CRC
# (MODVERSIONS); the tree's Module.symvers lacks 42 of them: extra.symvers
# holds 5 read from the stock modules' __versions and 37 computed by
# building their defining objects in the prepared tree (kernel/cpu.o,
# block/genhd.o, ...; the 4 that overlap known values all match).
set -eu
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
root=$(CDPATH= cd -- "$here/../../.." && pwd)
kb=$root/tools/kbuild;k=$kb/libra2-kernel-source
out=${1:?output directory};mkdir -p "$out";out=$(CDPATH= cd -- "$out" && pwd)
for f in zram_drv.c zram_drv.h zcomp.c zcomp.h zcomp_lzo.c zcomp_lzo.h;do cp "$k/drivers/block/zram/$f" "$out/";done
cp "$k/mm/zsmalloc.c" "$out/"
for f in lzo1x_compress.c lzo1x_decompress_safe.c lzodefs.h;do
 sed -e 's/EXPORT_SYMBOL_GPL(lzo1x_[a-z0-9_]*);//' -e 's/\blzo1x_1_compress\b/kobo_lzo1x_1_compress/g' -e 's/\blzo1x_decompress_safe\b/kobo_lzo1x_decompress_safe/g' "$k/lib/lzo/$f" > "$out/$f"
done
sed -e 's/\blzo1x_1_compress\b/kobo_lzo1x_1_compress/g' -e 's/\blzo1x_decompress_safe\b/kobo_lzo1x_decompress_safe/g' "$k/drivers/block/zram/zcomp_lzo.c" > "$out/zcomp_lzo.c"
cat > "$out/kobo_lzo.h" <<'H'
#include <linux/lzo.h>
int kobo_lzo1x_1_compress(const unsigned char *src, size_t src_len, unsigned char *dst, size_t *dst_len, void *wrkmem);
int kobo_lzo1x_decompress_safe(const unsigned char *src, size_t src_len, unsigned char *dst, size_t *dst_len);
H
sed -i.orig 's|#include <linux/lzo.h>|#include "kobo_lzo.h"|' "$out/zcomp_lzo.c";rm -f "$out/zcomp_lzo.c.orig"
cat > "$out/Makefile" <<'M'
obj-m := kobo_zsmalloc.o kobo_zram.o
kobo_zsmalloc-y := zsmalloc.o
kobo_zram-y := zram_drv.o zcomp.o zcomp_lzo.o lzo1x_compress.o lzo1x_decompress_safe.o
M
make -s -C "$k" ARCH=arm CC="$kb/arm-clang" LD="$kb/arm-ld" HOSTCC=clang HOSTCFLAGS="-I$kb/host-include" \
 LOCALVERSION=-00838-g349fbf5767c KBUILD_EXTRA_SYMBOLS="$here/extra.symvers" M="$out" modules
ls -la "$out/kobo_zsmalloc.ko" "$out/kobo_zram.ko"
