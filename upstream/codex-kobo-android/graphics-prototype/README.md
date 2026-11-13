# Android 图形实机实验

此文件记录上一阶段 Canvas 原型；新增框架显示服务和中文控件结果见 `../framework-prototype/README.md`。下文的“尚未”描述为该阶段的历史状态。

本轮在 Kobo Forma 原厂系统内执行了 Android 7 的 `app_process32`，输出 `ANDROID_FRAMEWORK_MAIN_OK`，随后用 Android Canvas/Skia 在本机生成 `canvas-test.png`，输出 `ANDROID_CANVAS_PNG_OK 720x960`。位图黑白像素检查及 PNG 编码通过，取回电脑后人工检查图像正常。

显示桥接工具用 FBInk 读取 framebuffer 原画面到内存，暂时停止经路径验证的 KOReader 主进程，显示测试图约 5 秒，恢复原画面，再继续 KOReader。第二次测试在恢复后重新读取 framebuffer，2,764,800 字节逐字节比较一致。未保存用户原画面到文件。日志见 `screen-display-result.txt`。

这只证明本机 Android 位图绘制和一次性墨水屏输出。没有运行 Android Activity、SurfaceFlinger、APK 安装器或微信读书；没有持续帧传输和输入事件路由。字体目录暂只提供 Roboto，原始 fonts.xml 引用的其他字体会报缺失，中文字体尚未接入。

## 实现

- `CanvasProbe.java`：Android API 24 Java 测试，用 javac 和 D8 生成 Dex。
- `property-init.c`：通过 Android Bionic 私有接口创建真实属性共享区，加载实验镜像 `/default.prop` 与 `/system/build.prop`，读回 ABI 验证。原厂内核没有 SELinux，Bionic 返回 -2 表示属性映射成功但无法设置 SELinux 扩展属性；仅此返回值被明确接受，其他失败终止。这不是有安全隔离的 Android 容器。
- 实验 default.prop 将 `dalvik.vm.usejit` 设为 false，与当前 ART 解释执行模式一致。属性区仅存在于实验镜像的 dev 目录，无系统属性写入服务。
- `run-graphics.sh`：实验根目录内部的临时模块、服务、ART 与 Canvas 测试；每次重新建立属性区并删除旧 PNG，检查框架输出标志和新 PNG 文件。不会启动 Android init。
- `stage-graphics.sh`：首次创建独立 384 MiB ext2 文件镜像，保留已有 192 MiB Java 实验镜像。拒绝覆盖现有文件。
- `run-graphics-test.sh`：挂载、执行、清理图形实验，无开机启动项。可在 SSH 中执行 `sh /mnt/onboard/.adds/kobo-android-lab/run-graphics-test.sh`，只生成 PNG，不切换屏幕。
- `screen-probe.c`：基于 FBInk 1.25.0 的临时显示工具，包含恢复字节比较。调用者负责暂停和恢复前台程序；不要在前台程序仍在绘制时直接使用。

FBInk 上游：[NiLuJe/FBInk](https://github.com/NiLuJe/FBInk)，[v1.25.0 源码下载](https://github.com/NiLuJe/FBInk/releases/download/v1.25.0/FBInk-v1.25.0.tar.xz)，对应未修改源代码压缩包 `fbink-source.tar.xz`，许可证 `FBInk-LICENSE`。本目录不含 Android 系统镜像。

## 设备文件

`/mnt/onboard/.adds/kobo-android-lab/graphics.img` 为 384 MiB 图形实验镜像，另有 `screen-probe` 与自生成 `canvas-test.png`。192 MiB `runtime.img` 仍保留。实验结束已卸载镜像和模块，并恢复 KOReader；原系统分区与启动配置未更改。
