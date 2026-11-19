# Kobo Libra Colour port (Koboweread-Colour)

**English** | [简体中文](Libra-Colour移植.md)

This fork takes the WeRead 1.9.7 Android 7 runtime of [Koboweread](https://github.com/scokyfei/Koboweread) to the **Kobo Libra Colour (N428)**. Unofficial project, not affiliated with Tencent, Rakuten Kobo or Google.

## Scope

| | Libra 2 (original project) | Libra Colour (this fork) |
|---|---|---|
| Model | N418 | N428 |
| SoC | NXP i.MX6SLL, single core | MediaTek MT8113 (the kernel says "MT8110 board"), dual Cortex-A53, 32-bit system |
| RAM | 512 MB | 965 MB |
| Kernel | 4.1.15 | 4.9.77 |
| Verified firmware | 4.34.20097, 4.38.23852 | 4.42.23303 |
| WeRead | 1.9.7, 1.8.8 | 1.9.7 (1.8.8 not ported) |

## Differences from the Libra 2 and how they are handled

| Part | Libra Colour | Handling |
|---|---|---|
| ashmem | Built into the kernel | The kernel's own ashmem is used; no module is loaded |
| binder | Built into the kernel, but protocol 8 (64-bit). Google's Android 7 ARM image needs protocol 7 | A separate protocol-7 `binder_linux.ko` whose device, device class, debugfs directory and trace events are all named `binder32`, beside the kernel's own binder. The launch scripts read the device number from `/sys/class/binder32/binder32/dev` and create `/dev/binder` in the chroot |
| madvise | The kernel has `CONFIG_ADVISE_SYSCALLS` | `kobo_madvise.ko` is still built, so the launch scripts and the VM preload library stay unchanged |
| pagecache | — | `kobo_pagecache.ko` built for 4.9 |
| zram | Built into the kernel (`zram0`) | The original scripts only load modules when there is no `zram0`, so that part is skipped |
| Module versions | `CONFIG_MODVERSIONS=y`, no module signing | The kernel objects are compiled from Rakuten Kobo's published source with the device's own `/proc/config.gz`, and the CRCs are read from them. The `module_layout` CRC must equal the value in Kobo's own Wi-Fi modules, `0xa81081b3` |
| Display | MediaTek hwtcon display driver; framebuffer 1264×1680, 32 bpp, 5056 bytes per line, the same as the Libra 2 | FBInk updated to the latest upstream code (v1.25.0-544, with Libra Colour support) |
| Touch | Elan Touchscreen, range 1680×1264 like the Libra 2, but the X axis runs the other way (KOReader's Kobo_monza sets `touch_mirrored_y`) | On the N428, `touch-forward.c` first maps raw X to `1679 − X` |
| Keys | Page keys 193/194 and the rotation signal are on `gpio-keys`; the power key 116 is on its own `bd71828-pwrkey` device | When `gpio-keys` has no 116, `touch-forward.c` also opens `bd71828-pwrkey` |
| Battery | `bd71827_bat` | `bd71827_bat` first, `mc13892_bat` as a fallback |
| Frontlight | `mxc_msp430.0` (brightness 0–100), `lm3630a_led/color` (warmth 0–10) | Same as the original code |
| WeRead APK | Official channel-900 1.9.7: same size and native libraries as the author's copy, different channel bytes (the channel is in classes10.dex, so the startup profile shipped in the recipe does not match; this only affects the first start's speed) | The install probe and the precompile script accept both digests |
| Frontlight sync | A light that is off reads 0 | Back in KOReader, 0 means "off" and the previous level is kept; if the brightness is the same as when WeRead was opened, KOReader's setting is left alone. The original plugin treated 0 as a brightness level, which also affects the Libra 2 |
| binder security | anbox's old binder driver | Fixed CVE-2019-2215 (POLLFREE removes the waitqueue when a thread exits) and added a NULL check |

## Opening WeRead from the Kobo home screen

Copy `src/resident-v197/device/nickelmenu-weread` to `/mnt/onboard/.adds/nm/weread`; NickelMenu then shows 「微信讀書」. Tapping it starts KOReader and opens WeRead once KOReader is online. The label uses traditional characters: with an English UI, the Kobo shows Chinese with Japanese fonts, which lack the simplified 读 and 书.

## Build

```sh
KOBO_ACCEPT_ANDROID_SDK_LICENSE=1 sh bootstrap.sh
sh src/kmod-mtk/build.sh            # optional, needs Docker; regenerates kernels/4.9.77/
sh src/package/build-package.sh 1.9.7
```

- `src/kmod-mtk/build.sh` compiles inside a Linux container: the kernel source has file names that differ only in case, which macOS's default filesystem cannot hold.
- On newer macOS releases, zig 0.13's `objcopy` fails to link; `src/tools/build-host-tools.sh` then uses `llvm-objcopy` (for example `brew install llvm`), or set `KOBO_OBJCOPY`.

## WeRead APK

The official site ink.qq.com only offers the latest version. Version 1.9.7 is on Tencent's official download server:

```
https://download.weread.qq.com/pkg/105/releases/1.9.7.10244588_900.apk
sha256 fae543b19c935024d5f51ac0d42df00bc6ebb5de2fc197b51bb3131e3b16707d
```

Name it `weread-1.9.7.apk` and put it in the `wereadkobo.koplugin` folder. The APK is not distributed with this repository.

## Test tools

- `src/kmod-mtk/test/binder32-smoke.c`: checks ashmem, a real transaction over the protocol-7 binder between two processes, and epoll + `BINDER_THREAD_EXIT` + close.
- `src/kmod-mtk/test/hwprobe.c`: read-only; lists the input devices' coordinate ranges and key codes, and the framebuffer parameters.
