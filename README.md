# 在 Kobo Libra 2 上运行微信读书（Android 7 运行环境）

*Run the WeRead e-ink app (微信读书墨水屏版 1.9.7 or 1.8.8) on a Kobo Libra 2 next to KOReader, in a small Android 7 environment built from Google's official system image, this repository and your own copy of the APK.*

这个项目在 Kobo Libra 2 上，通过 KOReader 插件运行原版微信读书墨水屏 APK（1.9.7 或 1.8.8）：
- 运行环境是一个精简的 Android 7（chroot），**不改动** Kobo 的系统、固件和书库；
- 显示由墨水屏专用的桥接程序完成，支持翻页键、触摸、前光调节、正倒翻转；
- 带有一系列针对这台 512 MB 内存、单核设备的速度和内存优化。

本仓库**不包含**任何 Google 系统文件、腾讯的 APK 或个人数据：
- Google 的 Android 7.0 ARM 系统镜像由构建脚本从 Google 官方地址下载，使用前需要你接受 Android SDK 许可；
- 微信读书 APK 需要你自己提供；
- 我们对 Google 文件的改动是三个很小的字节补丁，补丁前后都会核对哈希。

## 适用范围

| 项目 | 要求 |
|---|---|
| 阅读器 | Kobo Libra 2（N418），固件 **4.34.20097** 或 **4.38.23852**（其他固件安装时会被拒绝） |
| 阅读器上的软件 | 已安装 KOReader（`/mnt/onboard/.adds/koreader`），可用空间约 2.3 GB |
| 电脑 | macOS（Apple 芯片或 Intel）或 Linux x86_64，需联网 |
| 微信读书 APK | 墨水屏版 1.9.7，sha256 `351105bda1ac58bc037c2d604982635a24018eb0f9b9964abeb5b84647565f1c`；或 1.8.8，sha256 `f0b8ab65f08162559e7f67920b069785748e197459489c4e73fdbd75cc3ccba4` |

## 快速开始

```sh
sh bootstrap.sh                                         # 下载工具链和 Google 镜像（约 650 MB，需接受 Android SDK 许可）
sh src/package/build-package.sh 1.9.7
```

生成的 `releases/wereadkobo-1.9.7-日期-local.zip` 里有 KOReader 插件 `wereadkobo.koplugin`，安装方法见 [docs/构建与安装.md](docs/构建与安装.md)。

安装时请自行下载对应版本的微信读书 APK，放到 `wereadkobo.koplugin` 目录中，与 `main.lua` 同级。安装包不含微信读书 APK 或从中提取的原生库。

## 首次使用

1. 首次登录后，请保持 Wi-Fi 连接，耐心等待书架信息同步和书籍下载。首次打开书籍也可能比平时慢。
2. 正常阅读一段时间后，在启动器的「阅读优化」里选择「重新编译加速文件并重启」。编译会利用这段时间记录的阅读运行数据，完成后自动重启微信读书。

## 目录

| 目录 | 内容 |
|---|---|
| `bootstrap.sh` | 准备工具链（zig 0.13、JDK 17、Android 构建工具）和 Google 镜像，版本固定、全部校验 |
| `src/package/` | 打包脚本：从源码、配方、Google 镜像和你的 APK 生成安装包 |
| `recipe/v197`、`recipe/v188` | 镜像配方：每个文件来自哪里（Google 原文件、字节补丁、本项目文件、APK、构建产物），以及权限、属主和时间 |
| `src/runtime/` | 运行层（镜像里的 `/lab`）构建，调用 `upstream/` 中 Codex 原型的构建脚本 |
| `src/resident-v197/`、`src/resident-v188/` | 常驻启动器：会话脚本、显示桥、触摸转发、进程入口、KOReader 菜单 |
| `src/`（其余） | 显示库、HWC、JIT 配置、zram 等加速组件，以及安装器 |
| `kernels/` | 已在实机验证的内核模块（binder、ashmem、zram 等），两种固件各一套；源码与补丁见 `upstream/codex-kobo-android/module-prototype/` 和 `src/kmod/` |
| `upstream/codex-kobo-android/` | 最初移植（Codex 原型）的源码和构建脚本 |
| `third_party/` | FBInk v1.25.0（GPL-3.0，已精简）、AOSP 头文件（Apache-2.0） |

## 已知限制

- **从源码构建的安装包已在实验机上验证**：1.9.7 和 1.8.8 都能装好并显示登录页。登录后的长期使用，目前只在作者自己维护的环境里测过，那里的组件逐字节一致或代码一致。
- **首次使用比作者的环境慢一些**：WeRead 的预编译代码（odex）要在你的设备上，根据实际阅读记录编译，尚未做成菜单项。
- **Linux 构建**：由 GitHub Actions（Ubuntu）检查。完整打包目前在 macOS 上验证过。
- **非官方项目**：与腾讯、乐天 Kobo、Google 均无关联。使用风险自负，请遵守微信读书的用户协议。

## 许可

本项目以 GPL-3.0 发布（见 `LICENSE`）。第三方部分保留各自的许可，见 `third_party/README.md`。内核模块来自 Linux 内核源码，使用 GPL-2.0。
