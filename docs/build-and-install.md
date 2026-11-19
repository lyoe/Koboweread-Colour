# Build and install

**English** | [简体中文](构建与安装.md)

## 1. Prepare the computer

- **macOS**: install the Xcode command line tools (`xcode-select --install`). The system's curl, python3, perl, make, zip and cpio are enough.
- **Linux x86_64** (Ubuntu as an example):

  ```sh
  sudo apt-get install -y build-essential curl unzip zip cpio python3 perl
  ```

For the Libra Colour: the kernel modules are already in `kernels/4.9.77/`. Docker is only needed to rebuild them with `sh src/kmod-mtk/build.sh` (see [libra-colour-port.md](libra-colour-port.md)). On newer macOS releases, the `objcopy` bundled with zig 0.13 fails to link; the build then uses `llvm-objcopy` (`brew install llvm`), or set `KOBO_OBJCOPY`.

## 2. Download the toolchain and the Google image

```sh
sh bootstrap.sh
```

The script explains which files are covered by the Android SDK License; type `yes` to accept it (set `KOBO_ACCEPT_ANDROID_SDK_LICENSE=1` for unattended runs). It downloads about 650 MB, all at fixed versions and checked against sha256 values, then:

- puts the tools in `tools/`: zig 0.13.0, Temurin JDK 17.0.20.1+1, `android.jar` (platform 24 r02), `d8.jar` (build-tools 30.0.3), `aapt2` (8.4.0-11315950), and a `debugfs` compiled on this computer;
- extracts Google's Android 7.0 ARM system image (`armeabi-v7a-24_r07`) to `build/google/`.

## 3. Package

Build the 1.9.7 package:

```sh
sh src/package/build-package.sh 1.9.7
```

- The first run creates your own signing key in `build/keys/lab.jks`, used for the launcher and the test apps. Keep it: later packages must use the same key to upgrade an installation in place.
- It takes about 4–5 minutes and writes `releases/wereadkobo-1.9.7-DATE-local.zip`.

The packaging script:
1. builds the runtime layer, the launcher, the display and system libraries, the JIT configuration library, and tools such as `e2fsck` and `tune2fs`;
2. assembles the Android root filesystem from Google's image files, byte patches, this project's files and build outputs according to `recipe/v197`, and puts your launcher certificate into `packages.xml`;
3. generates the KOReader plugin and the installation data.

## 4. Install on the Kobo

The project's release versions and the WeRead APK versions are labelled separately. Pick the 1.8.8 or 1.9.7 package that matches your APK (the Libra Colour supports 1.9.7 only). The installation checks that the APK is present and extracts its native libraries; WeRead's identity is checked again at launch.

1. Unzip the package. Download the WeRead e-ink APK of the same version as the package yourself (for 1.9.7, see [Getting the WeRead APK](../README.md#getting-the-weread-apk)), name it `weread-1.9.7.apk` or `weread-1.8.8.apk`, and put it in the `wereadkobo.koplugin` folder, next to `main.lua`. Copy the whole folder over USB to `.adds/koreader/plugins/` on the Kobo, then eject safely. The native libraries are extracted from the APK during installation.
2. In KOReader, open "Menu → Tools (wrench) → More tools → 微信读书 1.9.7（安装）→ 安装微信读书 1.9.7".
   - Installation takes 5–10 minutes; keep the reader on a charger.
   - Everything goes to `.adds/kobo-weread-home`, including a 2 GB image file. The image is ext4 with a journal: after a forced restart or a flat battery, the journal recovers it at the next start.
3. Restart KOReader when asked; the menu entry becomes "微信读书 1.9.7（安装版）". Choose "Android 启动器" (Android launcher); the first start takes about 1–2 minutes. Connect to Wi-Fi first.
4. In WeRead, accept the agreement and log in by scanning the QR code with WeChat.

**Note**: a WeRead account stays logged in on only one device and one version at a time. Logging in here logs out the e-ink edition on other devices.

## Upgrading in place (same version)

Close the background Android. Copy the whole `wereadkobo.koplugin` folder from the new package to `.adds/koreader/plugins/` on the Kobo, replacing the old plugin, and restart KOReader. Open "Tools → More tools → 微信读书（安装版）→ 原地升级" (upgrade in place), then restart KOReader again when it finishes.

An upgrade requires the same WeRead version and launcher signature as the existing installation. It keeps the Android image, the login, the reading settings, screenshots and the speed-up files on the device. The APK is not needed again. If the power goes out during an upgrade, run "原地升级" again with the same package to recover and retry.

## 5. Use

- Page keys: the upper key turns to the next page, the lower key to the previous page. Hold the current next-page key for 1 second for an immediate full refresh. Hold the current previous-page key for 3 seconds and release it: the screen briefly shows the result of saving a PNG screenshot to the `snapshot/` folder of the installation (`.adds/kobo-weread-home/snapshot/` for the installed version).
- Swipe up or down along a screen edge to adjust the frontlight: the page-key side for brightness, the opposite side for warmth.
- Hold both page keys for 2 seconds and release them to return to the Android launcher.
- Press the power key to return to KOReader; Android stays in the background.
- The menu has "关闭后台 Android" (close background Android), display options, instructions and the running status.

## 6. Uninstall

In KOReader, first choose "关闭后台 Android", then delete `.adds/kobo-weread-home` and `.adds/koreader/plugins/wereadkobo.koplugin` over USB.

## 7. Switches

Both launchers have a 「阅读优化」 (reading optimisation) page. It sets the auto-sleep delay, page-key fade-in, chapter pre-layout and the cache of popular-highlight users. On battery, auto-sleep is on by default with a 5-minute delay. Under 「阅读优化 → 自动休眠等待时间」 choose 1, 3, 5, 10, 15 or 30 minutes. After waking from a sleep triggered this way, the original WeRead session comes back. Page fade-in is off by default; when on, the page refreshes in 8 strips from the screen edge. Saved settings take effect when the WeRead session restarts.

You can also create the following empty files in the installation folder by hand; they take effect at the next start.

| Switch | Effect |
|---|---|
| `fast-v197/idle-suspend-disabled` | Turn off WeRead idle sleep |
| `fast-v197/page-wipe-enabled` | Turn on page-key fade-in |
| `fast-v197/zram-disabled` | Turn off zram compressed swap |
| `fast-v197/skia-disabled` | Glyph cache back to 768 KB |
| `fast-v197/segdict-disabled` | Keep the word-segmentation dictionary after closing a book |
| `fast-v197/seglow-disabled` | Do not lower the priority of the segmentation process |
| `fast-v197/fontcache-disabled` | Turn off the font-size cache |
| `fast-v197/poollow-disabled` | Do not lower the priority of the background thread pool |
| `fast-v197/hwc-disabled` | Turn off direct HWC composition. Note: the public version does not ship its fallback display library |

For 1.8.8, the folder is `fast-v188/`.

### Optional AOSP offline Pinyin

1.8.8 and 1.9.7 share one build script for the Pinyin APK and its ARMv7 decoder library. When packaging, point `KOBO_PINYIN_SOURCE` at a local Git clone of AOSP `platform/packages/inputmethods/PinyinIME` that contains the commit named in `src/resident-v197/pinyin/build.sh`. Also set `KOBO_AIDL` to the Android SDK's `aidl` and `KOBO_ANDROID_CC` to the NDK's `armv7a-linux-androideabi24-clang++`. The usual package command then includes the Pinyin component.

After installation, turn on 「AOSP 离线拼音」 (AOSP offline Pinyin) under 「阅读优化」 in the launcher and restart the WeRead session. It is off by default. For on-device latency diagnostics, build with `KOBO_PINYIN_TRACE=1`, then create `ime-trace-enabled` in `fast-v188` or `fast-v197` under the installation root and restart the session.
