# 在 Kobo Libra 2 / Libra Colour 上运行微信读书（Android 7 运行环境）

[English](README.md) | **简体中文**

**Koboweread-Colour** 是 [scokyfei/Koboweread](https://github.com/scokyfei/Koboweread) 的分支，在原项目（Kobo Libra 2）的基础上增加了 **Kobo Libra Colour** 的支持。移植细节见 [docs/Libra-Colour移植.md](docs/Libra-Colour移植.md)。

这个项目通过 KOReader 插件，在 Kobo 上运行原版微信读书墨水屏 APK：
- 运行环境是一个精简的 Android 7（chroot），**不改动** Kobo 的系统、固件和书库；
- 显示由墨水屏专用的桥接程序完成，支持翻页键、触摸、前光调节、正倒翻转；
- 原项目针对 Libra 2（512 MB 内存、单核）做了一系列速度和内存优化，Libra Colour 沿用同一套。

本仓库**不包含**任何 Google 系统文件、腾讯的 APK 或个人数据：
- Google 的 Android 7.0 ARM 系统镜像由构建脚本从 Google 官方地址下载，使用前需要你接受 Android SDK 许可；
- 微信读书 APK 需要你自己提供；
- 我们对 Google 文件的改动是三个很小的字节补丁，补丁前后都会核对哈希。

## 适用范围

| 项目 | Kobo Libra 2 | Kobo Libra Colour |
|---|---|---|
| 型号 | N418 | N428 |
| 固件 | **4.34.20097** 或 **4.38.23852** | **4.42.23303** |
| 微信读书 | 墨水屏版 1.9.7 或 1.8.8 | 墨水屏版 1.9.7 |
| 状态 | 原项目在实机上验证过；本分支改过的公共部分还没在 Libra 2 上测试（见“已知限制”） | 本分支在实机上验证过：安装、登录、显示、触摸、按键 |

其他固件的内核没有对应的模块，安装时会被拒绝。

共同要求：
- 阅读器上已安装 KOReader（`/mnt/onboard/.adds/koreader`），可用空间约 2.3 GB；
- 电脑：macOS（Apple 芯片或 Intel）或 Linux x86_64，需联网；重新编译 Libra Colour 的内核模块还需要 Docker。

## 获取微信读书 APK

官网 [ink.qq.com](https://ink.qq.com) 只提供最新版。1.9.7 可以从腾讯的官方下载服务器获取：

```
https://download.weread.qq.com/pkg/105/releases/1.9.7.10244588_900.apk
sha256 fae543b19c935024d5f51ac0d42df00bc6ebb5de2fc197b51bb3131e3b16707d
```

这是官方渠道 900 的包，和原项目作者用的那份 1.9.7 是同一次构建，只有渠道信息不同，两份都能用。

## 快速开始

原项目的 Release 安装包不支持 Libra Colour，请从源码构建：

```sh
KOBO_ACCEPT_ANDROID_SDK_LICENSE=1 sh bootstrap.sh       # 下载工具链和 Google 镜像（约 650 MB，表示你接受 Android SDK 许可）
sh src/package/build-package.sh 1.9.7
```

- 生成的 `releases/wereadkobo-1.9.7-日期-local.zip` 里有 KOReader 插件 `wereadkobo.koplugin`，安装方法见 [docs/构建与安装.md](docs/构建与安装.md)。
- 安装时请把微信读书 APK 命名为 `weread-1.9.7.apk`，放到 `wereadkobo.koplugin` 目录中，与 `main.lua` 同级。安装包不含微信读书 APK 或从中提取的原生库。
- Libra Colour 的内核模块已经在 `kernels/4.9.77/` 里。要自己重新编译，运行 `sh src/kmod-mtk/build.sh`（需要 Docker）。
- 在较新的 macOS 上，zig 0.13 自带的 `objcopy` 无法链接，构建脚本会自动改用 `llvm-objcopy`（可用 `brew install llvm` 安装，也可以用 `KOBO_OBJCOPY` 指定）。

## 首次使用

1. **启动前先在 KOReader 里连好 Wi-Fi**。启动时没联网，微信读书会打不开。
2. 在 KOReader 菜单 **工具 → 更多工具 → 微信读书 1.9.7（安装版）→ Android 启动器（常驻，热启动）** 启动。第一次要同意用户协议，再用微信扫码登录。
3. 首次登录后，请保持 Wi-Fi 连接，耐心等待书架信息同步和书籍下载。首次打开书籍也可能比平时慢。
4. 正常阅读一段时间后，在启动器的「阅读优化」里选择「重新编译加速文件并重启」。编译会利用这段时间记录的阅读运行数据，完成后自动重启微信读书。

**从 Kobo 首页直接打开**：把 `src/resident-v197/device/nickelmenu-weread` 复制到 `/mnt/onboard/.adds/nm/weread`，需要先装好 [NickelMenu](https://github.com/pgaskin/NickelMenu)。之后 Kobo 首页的 NickelMenu 菜单里会出现“微信讀書”，点它会启动 KOReader，联网后自动打开微信读书。菜单名用繁体，是因为 Kobo 用英文界面时，简体的“读”“书”会显示成方块。

**操作**：右侧上键是下一页，下键是上一页；按住下一页键 1 秒全屏刷新。沿右边缘（翻页键一侧）上下滑动调亮度，沿左边缘调色温。短按电源键回到 KOReader。

## 目录

| 目录 | 内容 |
|---|---|
| `bootstrap.sh` | 准备工具链（zig 0.13、JDK 17、Android 构建工具）和 Google 镜像，版本固定、全部校验 |
| `src/package/` | 打包脚本：从源码、配方和 Google 镜像生成安装包 |
| `recipe/v197`、`recipe/v188` | 镜像配方：每个文件来自哪里（Google 原文件、字节补丁、本项目文件、APK、构建产物），以及权限、属主和时间 |
| `src/runtime/` | 运行层（镜像里的 `/lab`）构建，调用 `upstream/` 中 Codex 原型的构建脚本 |
| `src/resident-v197/`、`src/resident-v188/` | 常驻启动器：会话脚本、显示桥、触摸转发、进程入口、KOReader 菜单 |
| `src/kmod-mtk/` | Libra Colour（联发科 MT8113，内核 4.9.77）的内核模块构建：协议 7 的 `binder32`、`kobo_madvise`、`kobo_pagecache`，以及测试工具 |
| `src/`（其余） | 显示库、HWC、JIT 配置、zram 等加速组件，以及安装器 |
| `kernels/` | 已在实机验证的内核模块：Libra 2 的两种固件各一套（binder、ashmem、zram 等），Libra Colour 一套（`4.9.77`；ashmem 和 zram 用内核自带的）。Libra 2 模块的源码与补丁见 `upstream/codex-kobo-android/module-prototype/` 和 `src/kmod/` |
| `upstream/codex-kobo-android/` | 最初移植（Codex 原型）的源码和构建脚本 |
| `third_party/` | FBInk（上游 v1.25.0-544-gded92fa5，支持 Libra Colour；GPL-3.0，已精简）、AOSP 头文件（Apache-2.0） |
| `docs/` | 构建与安装说明，以及 Libra Colour 移植说明 |

## 已知限制

- **Libra 2 上还没重新测试**：本分支升级了 FBInk，也改了 Libra 2 同样会用到的启动脚本、触摸转发和前光同步。这些改动按设计对 Libra 2 不该有影响，但还没在 Libra 2 上实际验证。Libra 2 用户如果更看重稳定，请先用 [原项目](https://github.com/scokyfei/Koboweread)。
- **Libra Colour 只移植了 1.9.7**：1.8.8 的启动器没有改动。
- **Libra Colour 上 REAGL 刷新模式没有生效**：目前用普通刷新模式，残影可能比 Libra 2 多。
- **首次使用比作者的环境慢一些**：WeRead 的预编译代码（odex）要在你的设备上，根据实际阅读记录编译，尚未做成菜单项。用渠道 900 的 1.9.7 时，配方自带的启动加速记录对不上，第一次启动会再慢一点。
- **Linux 构建**：由 GitHub Actions（Ubuntu）检查。完整打包目前在 macOS 上验证过。
- **非官方项目**：与腾讯、乐天 Kobo、Google 均无关联。使用风险自负，请遵守微信读书的用户协议。

## 致谢

- [scokyfei/Koboweread](https://github.com/scokyfei/Koboweread)：Libra 2 版本的原项目，本分支的全部基础；
- [KOReader](https://github.com/koreader/koreader)、[FBInk](https://github.com/NiLuJe/FBInk)、[NickelMenu](https://github.com/pgaskin/NickelMenu)；
- 乐天 Kobo 公开的 [Libra Colour 内核源码](https://github.com/kobolabs/Kobo-Reader/tree/master/hw/mt8113-libraC_vision7)。

## 许可

本项目以 GPL-3.0 发布（见 `LICENSE`）。第三方部分保留各自的许可，见 `third_party/README.md`。内核模块来自 Linux 内核源码，使用 GPL-2.0。
