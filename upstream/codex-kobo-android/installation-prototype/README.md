# 普通 APK 安装实验

目标设备仍是 Kobo Forma 原厂固件 4.26.16704，内核未更换。此目录是开发源码和验证记录，不是刷机包、通用 APK 安装器或微信读书成品。

本阶段从此前的 coreApp/persistent 系统测试包，推进到通过 Android 原版 PackageInstaller.Session 接口安装普通 APK。测试包 `com.kobo.lab.installed` 没有 coreApp、persistent、system 或特殊 UID 标志；由真实 PMS 分配应用身份，安装到 `/data/app`，由真实 AMS、ART、ActivityThread 与 WMS 启动。

## 实机结果

普通 APK 安装、启动、窗口绘制和退出已通过；完整日志见 `installation-result.txt`。安装回调为 `status=0 INSTALL_SUCCEEDED`，PMS 分配 UID 10006，来源为 `/data/app/com.kobo.lab.installed-1/base.apk`。原始与安装 APK 的 SHA-256 一致，编译器成功生成 25000 字节的 oat 文件。

停止并重新启动整个实验环境后，没有重新安装，UID 仍为 10006，应用私有启动计数从 2 增加到 3，验证记录见 `reopen-result.txt` 与 `verified-reopen/`。这次重启指 Android 实验环境的重启，并非硬件重启。画面见 `verified-install/installed-app.png` 和 `verified-reopen/installed-app.png`。结束后模块、镜像挂载、loop 与 Android 进程均已清理，KOReader 仍在运行，见 `cleanup-result.txt`。

安装编译器使用 64 MiB 堆上限和原版 `-Xnorelocate` 运行时参数，避免这台旧内核设备上的大地址预留及镜像重定位失败。

## 系统身份与服务

框架服务使用真实 UID/GID 1000。Linux 4.1 没有 ambient capabilities，所以专用启动程序先 exec 原版 app_process32，再由显式预加载的构造函数切换身份。仅保留 CAP_KILL、CAP_SYS_NICE、CAP_SYS_RESOURCE，实测 CapEff 为 `0000000001800020`。普通应用的有效 capabilities 为零。

属性、installd、vold/cryptd 和进程启动套接字仅向私有环境中的 root 与系统 UID 开放。系统数据目录按实际系统 UID 整理权限；普通应用的私有目录继续保留各自的应用 UID。

PMS 使用正常模式（onlyCore=false），并调用真实 systemReady。补入匹配系统镜像的 DefaultContainerService、PackageInstaller、ExtServices、ExtShared、StatementService 及所需原生库，满足真实组件发现与安装流程，没有伪造安装成功结果。

## 安装与进程后端

`InstallProbe` 使用真实安装会话，流式写入并 fsync APK，再接收安装服务的实际状态 Intent。流式写入使用 API 支持的 length=-1，处理 ext2 不支持可选 posix_fallocate 的限制。提交成功后重新读取 PMS 的应用身份，核对普通应用标志、`/data/app` 路径和原始/安装文件 SHA-256，再经真实 AMS 发起 Activity。

进程后端本阶段验证时仅放行三个已知包：实验主页、原版包复制服务和普通测试 APK。UID、seinfo 与数据路径从真实 PMS 写入受保护的注册文件，普通测试包的目标 SDK 为 24；后续微信读书实验增加了单独固定包的 SDK 31 校验及 PMS 分配的网络、蓝牙附加组支持，详见相邻 weread-prototype 目录。它不是完整 Zygote 或通用沙箱。

本内核没有挂载命名空间。后续微信读书测试中扩展为：仅在 `/storage` 为空，或只包含 root:root 0700、应用无法遍历的 emulated 目录，且没有相关外部挂载时，后端接受 Android 的外部存储启动标志，表示此实验环境没有可用外部介质；不会创建或宣称存在命名空间。检测到可访问的目录、符号链接、意外条目或实际挂载后会拒绝这条启动路径。8 个实机边界测试记录见相邻 weread-prototype/storage-guard-selftest-result.txt。

## 镜像与构建

实验镜像扩展到 768 MiB。扩容前将旧 384 MiB 镜像完整备份并逐字节比较；扩容前后原版 e2fsck 检查均通过。备份位于设备实验目录的 `graphics-pre-install.img`。只改动文件镜像，没有调整 Kobo 分区。扩容结果见 `image-grow-result.txt`。

`image-tool-exec.c` 仅打开固定的实验镜像普通文件，关闭 SSH 继承的文件描述符，再将这个文件句柄交给独立小环境中的原版工具；不接受真实块设备参数。

- `build.sh`：构建并签署自制普通测试 APK，参数与上一阶段相同。
- `build-framework.sh`：构建公共框架探针及此阶段专用共享命名空间入口。
- `build-native.sh`：交叉编译 UID 切换、套接字、进程后端与镜像工具入口。
- `run-install-test.sh` / `run-install.sh`：只适用于已经准备好的这台设备实验镜像。

本目录不提供 Google 系统组件、系统镜像、第三方微信读书 APK 或账号数据。

## 当前边界

Android 网络管理与同步服务仍未适配，继续使用此前的受控离线框架设置。没有完成持续物理墨水屏输出、触控、休眠或微信读书测试。提供的画面来自设备上 SurfaceFlinger 的文件型显示扫描输出。

chroot、UID 和受限进程后端不能替代完整的 Android 安全隔离。宿主 `/proc` 仍可见，原生存储守护进程仍是受控 root 实验程序。详情也见前一阶段的存储和文件描述符限制说明。

安装 API 的流式写入约定见 [Android PackageInstaller.Session](https://developer.android.com/reference/android/content/pm/PackageInstaller.Session#openWrite(java.lang.String,%20long,%20long))。
