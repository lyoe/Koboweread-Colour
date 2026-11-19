# WeRead on the Kobo Libra 2 / Libra Colour (Android 7 runtime)

**English** | [简体中文](README.zh-CN.md)

**Koboweread-Colour** is a fork of [scokyfei/Koboweread](https://github.com/scokyfei/Koboweread). It adds **Kobo Libra Colour** support to the original project, which targets the Kobo Libra 2. Porting details (in Chinese): [docs/Libra-Colour移植.md](docs/Libra-Colour移植.md).

The project runs the original WeRead e-ink app (微信读书墨水屏版) on a Kobo through a KOReader plugin:
- The runtime is a minimal Android 7 (chroot). It does **not** modify the Kobo's system, firmware or library.
- The display goes through a bridge built for e-ink, with page keys, touch, frontlight control and upside-down rotation.
- The original project tuned speed and memory for the Libra 2 (512 MB RAM, single core); the Libra Colour uses the same set.

This repository contains **no** Google system files, no Tencent APK and no personal data:
- Google's Android 7.0 ARM system image is downloaded by the build scripts from Google's official servers. You need to accept the Android SDK License to use it.
- You supply the WeRead APK yourself.
- Our changes to Google's files are three small byte patches, with hashes checked before and after.

## Supported devices

| | Kobo Libra 2 | Kobo Libra Colour |
|---|---|---|
| Model | N418 | N428 |
| Firmware | **4.34.20097** or **4.38.23852** | **4.42.23303** |
| WeRead | e-ink edition 1.9.7 or 1.8.8 | e-ink edition 1.9.7 |
| Status | Verified on a device by the original project. The shared parts changed in this fork have not been tested on a Libra 2 yet (see Known limitations). | Verified on a device in this fork: installation, login, display, touch, keys |

Other firmware versions have no matching kernel modules, so the installer refuses them.

Common requirements:
- KOReader installed on the reader (`/mnt/onboard/.adds/koreader`), about 2.3 GB free space.
- A computer running macOS (Apple silicon or Intel) or Linux x86_64, with internet access. Rebuilding the Libra Colour kernel modules also needs Docker.

## Getting the WeRead APK

The official site [ink.qq.com](https://ink.qq.com) only offers the latest version. Version 1.9.7 is still on Tencent's official download server:

```
https://download.weread.qq.com/pkg/105/releases/1.9.7.10244588_900.apk
sha256 fae543b19c935024d5f51ac0d42df00bc6ebb5de2fc197b51bb3131e3b16707d
```

This is the official channel-900 build. It comes from the same build as the 1.9.7 the original author used; only the channel information differs. Both work.

## Quick start

The original project's release packages do not support the Libra Colour. Build from source:

```sh
KOBO_ACCEPT_ANDROID_SDK_LICENSE=1 sh bootstrap.sh       # toolchain and Google image (about 650 MB; this accepts the Android SDK License)
sh src/package/build-package.sh 1.9.7
```

- The resulting `releases/wereadkobo-1.9.7-DATE-local.zip` contains the KOReader plugin `wereadkobo.koplugin`. Installation steps (in Chinese): [docs/构建与安装.md](docs/构建与安装.md).
- Name the WeRead APK `weread-1.9.7.apk` and put it in the `wereadkobo.koplugin` folder, next to `main.lua`. The package contains neither the WeRead APK nor any native library extracted from it.
- The Libra Colour kernel modules are already in `kernels/4.9.77/`. To rebuild them, run `sh src/kmod-mtk/build.sh` (needs Docker).
- On newer macOS releases, the `objcopy` bundled with zig 0.13 fails to link. The build script then falls back to `llvm-objcopy` (`brew install llvm`), or set `KOBO_OBJCOPY`.

## First use

1. **Connect to Wi-Fi in KOReader before launching.** Without a network at launch, WeRead does not open.
2. Launch from the KOReader menu: **Tools → More tools → 微信读书 1.9.7（安装版）→ Android 启动器（常驻，热启动）**. The first time, accept the user agreement, then log in by scanning the QR code with WeChat.
3. After the first login, stay on Wi-Fi and let the bookshelf sync and books download. The first book you open may also be slower than usual.
4. After reading for a while, choose 「重新编译加速文件并重启」 under 「阅读优化」 in the launcher. The compile uses the runtime data recorded while you read, then restarts WeRead.

**Open it from the Kobo home screen**: install [NickelMenu](https://github.com/pgaskin/NickelMenu), then copy `src/resident-v197/device/nickelmenu-weread` to `/mnt/onboard/.adds/nm/weread`. The NickelMenu menu on the Kobo home screen then shows 「微信讀書」. Tapping it starts KOReader and opens WeRead as soon as KOReader is online. The label uses traditional characters because, with an English Kobo UI, the simplified 读 and 书 show as boxes.

**Controls**: the upper key turns to the next page, the lower key to the previous page; hold the next-page key for 1 second for a full refresh. Swipe up or down along the right edge (the page-key side) for brightness, along the left edge for warmth. A short press of the power key returns to KOReader.

## Layout

| Path | Contents |
|---|---|
| `bootstrap.sh` | Prepares the toolchain (zig 0.13, JDK 17, Android build tools) and the Google image, at fixed versions, all checksummed |
| `src/package/` | Packaging script: builds the installer from the sources, the recipes and the Google image |
| `recipe/v197`, `recipe/v188` | Image recipes: where each file comes from (Google original, byte patch, this project, APK, build output), with permissions, owners and times |
| `src/runtime/` | Runtime layer (`/lab` in the image), built with the Codex prototype scripts in `upstream/` |
| `src/resident-v197/`, `src/resident-v188/` | Resident launcher: session scripts, display bridge, touch forwarding, process entry, KOReader menu |
| `src/kmod-mtk/` | Kernel modules for the Libra Colour (MediaTek MT8113, kernel 4.9.77): protocol-7 `binder32`, `kobo_madvise`, `kobo_pagecache`, plus test tools |
| `src/` (rest) | Display library, HWC, JIT configuration, zram and other speed-ups, and the installer |
| `kernels/` | Kernel modules verified on devices: one set for each Libra 2 firmware (binder, ashmem, zram, …) and one for the Libra Colour (`4.9.77`; it uses the kernel's own ashmem and zram). Sources and patches of the Libra 2 modules: `upstream/codex-kobo-android/module-prototype/` and `src/kmod/` |
| `upstream/codex-kobo-android/` | Sources and build scripts of the first port (the Codex prototype) |
| `third_party/` | FBInk (upstream v1.25.0-544-gded92fa5, with Libra Colour support; GPL-3.0, trimmed), AOSP headers (Apache-2.0) |
| `docs/` | Build and installation notes, and the Libra Colour porting notes (in Chinese) |

## Known limitations

- **Not re-tested on the Libra 2**: this fork updated FBInk and changed launch scripts, touch forwarding and frontlight syncing that the Libra 2 also uses. These changes are designed not to affect the Libra 2, but they have not been checked on one. Libra 2 users who prefer stability should use the [original project](https://github.com/scokyfei/Koboweread) for now.
- **Only 1.9.7 is ported to the Libra Colour**: the 1.8.8 launcher is unchanged.
- **REAGL refresh is not active on the Libra Colour**: the normal refresh mode is used, so ghosting may be stronger than on the Libra 2.
- **First use is slower than in the author's setup**: WeRead's precompiled code (odex) has to be compiled on your device from your actual reading, and this is not a menu item yet. With the channel-900 1.9.7, the startup profile shipped in the recipe does not match, so the first start is a little slower still.
- **Linux builds**: checked by GitHub Actions (Ubuntu). Full packaging has been verified on macOS so far.
- **Unofficial project**: not affiliated with Tencent, Rakuten Kobo or Google. Use at your own risk and follow WeRead's user agreement.

## Acknowledgements

- [scokyfei/Koboweread](https://github.com/scokyfei/Koboweread): the original Libra 2 project and the foundation of this fork
- [KOReader](https://github.com/koreader/koreader), [FBInk](https://github.com/NiLuJe/FBInk), [NickelMenu](https://github.com/pgaskin/NickelMenu)
- Rakuten Kobo's published [Libra Colour kernel source](https://github.com/kobolabs/Kobo-Reader/tree/master/hw/mt8113-libraC_vision7)

## License

This project is released under GPL-3.0 (see `LICENSE`). Third-party parts keep their own licenses, see `third_party/README.md`. Kernel modules come from the Linux kernel source and are GPL-2.0.
