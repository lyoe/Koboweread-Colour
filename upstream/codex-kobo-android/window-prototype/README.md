# Android 包管理与窗口服务实验

本阶段在 Kobo Forma 原厂内核 `4.1.15-00286-g2a6a4cb08c66` 上继续运行 Android API 24 用户空间。保留原系统，不依赖远程 Android。

## 已验证

- 原始 installd 通过真实 socket 协议响应；创建 UID 10000 的测试应用数据目录，随后销毁并检查目录消失。
- Android PackageParser 实际验证微信读书 APK 证书，单签名 SHA256：`e4dd2f049941989ffafa45aef52eda9701372c27bc1708e89c217935140bfa1c`。
- ActivityManager Binder 权限调用通过；PackageManager 以 onlyCore 模式启动，另一进程查询 android 包，返回 UID 1000。
- 原始 SettingsProvider 加载成功；私有测试键写入、读回、还原均通过。
- WindowManager、InputManager、InputMethodManager 启动。最后一轮服务列表包含 22 项；早一轮未启动输入法时为 21 项。
- ServerLauncher 使用 Android 7 系统服务的共享原生库命名空间，修复重复加载 libandroid_runtime 导致的 JNI 缓存未初始化；实际可执行映射由两份变成一份。

## 真实窗口与输入测试结果

`WindowClient` 使用 root 权限的 TYPE_SYSTEM_DIALOG、标准 ViewRoot 和 WindowManager。最新两轮已通过：

- `attached-window-result.txt`：真正附着的 720×960 窗口执行 dispatchDraw，SurfaceFlinger 扫描输出中有 659,953 个白色像素和 4,514 个暗色像素，退出码 0；图像见 `attached-window.png`。
- `window-input-result.txt`：通过 InputManager 注入 DOWN/UP，系统输入分发触发按钮回调，按钮文字随之变化；输出图像有 4,753 个暗色像素，退出码 0；图像见 `attached-window-input.png`。
- 注入从独立线程等待输入完成，主线程继续处理事件；没有直接调用 performClick 或控件触摸回调。Kobo 物理触摸屏仍未接入。

早期 `No attached window draw` 是探针漏报：Android 7 ViewTreeObserver.merge 没有迁移附着前注册的 OnDrawListener。改用控件 dispatchDraw 后实测通过。同时补齐 WindowManager.enableScreenAfterBoot 与 setEventDispatching 的正式调用。不能将两项改动分别视为已经独立验证的唯一原因。

初次点击触发默认音效时，独立探针进程因缺少 AMS 应用记录，查询 SettingsProvider 失败。测试按钮现显式关闭音效和触觉反馈；普通应用注册、音频服务没有因此完成。原始失败日志保留在 `initial-click-sound-failure.txt`。

**尚未安装或启动微信读书 APK，也没有完整 Zygote / SystemServer 启动流程。** 普通 UID 的 ART、私有文件和 Binder 测试另见 [应用进程实验](../app-process-prototype/README.md)；当前交互窗口仍是 root 探针，不能等同于普通 APK Activity。384 MiB 实验镜像还需要扩容，触控、旋转、翻页键、休眠与账号登录均未接入。

本轮不暂停 KOReader、不修改实机 framebuffer；Android framebuffer 是镜像内普通文件。dev/input 为空，proc 只读，sys 为空，不挂载真实 cgroup 或 acct。SELinux xattr 在此内核上不支持，日志保留原始警告。

## 文件与复现

- `../framework-prototype/FrameworkProbe.java`：服务启动和独立客户端检查。
- `ServerLauncher.java`：单独编译为 server-launcher.dex；服务端外层 CLASSPATH 必须只有该 dex，避免父加载器提前加载 FrameworkProbe。
- `WindowClient.java`：已通过的真实窗口绘制与合成输入测试。
- `android-socket-exec.c`、`installd-ping.c`：socket 继承启动器与真实 installd 数据目录检查。
- `run-window.sh`：完整实验；`run-window-test.sh`：挂载、运行和清理包装器。
- `*-result.txt` 与 `attached-window-failure.txt`：实机日志，包含成功和失败，不作为完整 Android 可用证明。
- `final-cleanup.txt`：最后设备审计。

设置 framework-prototype/build.sh 所列 SDK/JDK/Zig/Android 库环境变量后运行该脚本，再执行本目录 build-native.sh。只生成自有探针；Google Android 二进制、APK 和镜像不随源码包分发。

设备已经部署的实验入口（仅用于这台设备，不是安装器）：

```sh
sh /mnt/onboard/.adds/kobo-android-lab/run-window-test.sh
```

当前窗口与点击测试均通过；包装器退出时清理服务、模块和镜像。若已有实验挂载或模块，入口拒绝并行运行。

依据的源码：[AOSP Android 7 ViewTreeObserver.merge](https://android.googlesource.com/platform/frameworks/base/+/android-7.0.0_r1/core/java/android/view/ViewTreeObserver.java)。
