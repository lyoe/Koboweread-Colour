> 后续进展：同一框架入口现已加入固定测试 APK 的真实 Activity 启动分支；当前能力与限制见 `../activity-prototype/README.md`。下文保留此前离屏/窗口阶段的记录。

> 后续进展见 [包管理与窗口服务实验](../window-prototype/README.md)：本文件记录早期图形阶段，后续已启动包管理及窗口服务，并通过真实窗口绘制与合成点击。

# Android 框架服务开发记录

这是 Forma 原厂内核上的手动开发实验，不是 APK 安装器。`FrameworkProbe` 没有执行微信读书代码。

## 本轮已验证

- Android 自身的 PackageParser 解析用户提供的微信读书 APK，读取资源名称“微信读书”，确认 minSdk 24、targetSdk 31、启动入口 `com.tencent.weread.LauncherActivity`、60 个 Activity、8 个 Service 和 3 个 Provider。这只属于解析测试；没有验证签名、安装包或启动应用。
- 新增 Android 7 属性协议的实验服务。Java 使用原生 SystemProperties 接口新增、更新并读回属性；`ro.*` 不能修改。属性区保存在实验镜像，服务只接受 root 连接；`persist.*` 本轮也只在内存中更新，不写持久配置，`ctl.*` 不执行。不是完整 Android init/property_service。
- 文件型 framebuffer 提供 720×960 RGBA 双缓冲，FBIOGET/PUT/PAN 与 mmap 共享内存检查通过，错误偏移和未知 ioctl 返回错误。没有把真实 `/dev/fb0` 暴露给 Android，也没有修改 Kobo 的显示模式。虚拟 framebuffer 的输出尚未持续转发到墨水屏。
- 原生 SurfaceFlinger 使用 PixelFlinger 软件 OpenGL ES 1.0 初始化，加载真实 gralloc.default。不是模拟器远程图形。
- 通过原版 LightsService、PowerManagerService 和 DisplayManagerService 的受控启动，识别出默认 720×960、160 dpi、30 Hz 软件显示；跨进程 service list 显示 display、power、gpu、SurfaceFlinger 四项。PowerManager 未完成启动阶段，没有接入 Kobo 电源/休眠；实验根目录没有挂载真实 sysfs，缺少唤醒锁接口会输出错误。
- 裁剪 fonts.xml 到实际提供的 Roboto 和 Noto CJK 字体，中文测试图人工检查正常；测试图已显示到墨水屏约 5 秒并恢复，framebuffer 2,764,800 字节比较一致。
- 单独的 Java 进程取得系统 Context 和真实 DisplayManager，在软件 Bitmap 上完成 LinearLayout、TextView、Button 的测量、布局和绘制，生成 `framework-view.png`。**不是 Activity 或已附着窗口，也没有按钮触控处理。** 离屏测试 Context 明确设置无菜单键，并拒绝跨用户访问权限检查；没有伪造窗口或 Activity 服务。

- 新增真实 `SurfaceSession` / `SurfaceControl`，将控件图通过 `Surface.lockCanvas`、`unlockCanvasAndPost` 提交给 BufferQueue。读取 SurfaceFlinger 实际扫描输出的虚拟 framebuffer，691,200 个像素和 4,706 个暗色像素均通过比较。取回前后 PNG 后又用 Pillow 验证，RGB 差值最大值为 0，即所有像素完全一致。这验证了单个静态图层的软件合成，不是应用窗口生命周期。

## 软件 EGL 修复

红色清屏后读回黑色的问题已定位并修复：原始 libagl 中 RGBX/RGBA/A8 的 `EGL_CONFIG_ID` 与创建 surface 使用的格式映射不一致，RGB 配置会创建成 A8 缓冲区。实机探针观察到格式 8，修正后格式为 2，RGBA 读回为 255,0,0,255；通过标准 EGL/GL 包装接口和直接驱动两条路径的 16 像素检查。

`patch-egl-configs.py` 只接受 SHA256 已固定的原始库，验证八张完整表位于 ELF 只读加载段且唯一，仅改六个配置编号，拒绝不匹配输入或覆盖已有输出。`egl-config-patch-report.json` 保存改动偏移和前后 SHA256；`egl-config-id-fix.patch` 提供对应源码修复。设备镜像保留 `libGLES_android.original.so`，只替换实验库，未修改原系统。

早期日志中的 `EGL_PIXEL_TEST_BLOCKED` 属于修复前失败记录；最新完整日志应显示 `ANDROID_SOFTWARE_EGL_PIXELS_OK` 和 `DIRECT_SOFTWARE_EGL_PIXELS_OK`。这仍不等于完整应用兼容。

## 早期实验的剩余工作（历史记录）

本目录最初只验证 Surface/BufferQueue/软件合成。当时尚无完整应用服务，微信读书尚未安装。后续实验已经加入原生 AMS/WMS/PMS、受限进程 broker 和输入桥接，并安装启动了原版微信读书；当前状态见项目根目录与 weread-prototype。原厂内核缺少命名空间和 cgroups，当前实验没有完整安全隔离。

## 文件与复测

- `FrameworkProbe.java`：平台解析、属性写入、受控显示服务启动与控件离屏绘制。
- `property-server.c`：Android Bionic 构造函数形式的临时属性服务，加载在独立 Android shell 进程中。
- `kobo-virtual-fb.c`：仅对实验 `/dev/graphics/fb0` 常规文件处理 framebuffer ioctl，其他 fd 转交原始内核 ioctl。
- `virtual-fb-probe.c`：映射、偏移、非法 ioctl 验证。
- `egl-probe.c`、`egl-driver-probe.c`：EGL 包装接口和直接驱动像素检查；直接探针另有只读、格式受限的 libagl 私有对象诊断，不用于显示后端。
- `SurfaceProbe.java`：提交真实 Surface 缓冲区，读取实际虚拟扫描输出，验证序号、偏移和图像像素。当前 GLES1 驱动缺少 FBO 扩展，`SurfaceControl.screenshot` 测试失败；没有伪造标准截图接口。改用后端真实文件输出，并通过独立像素比较验证。
- `run-framework.sh`：镜像内部启动、验证、停止服务并卸载实验模块。
- `run-framework-test.sh`：镜像挂载与清理入口。

设备内运行：

```sh
sh /mnt/onboard/.adds/kobo-android-lab/run-framework-test.sh
```

这只生成文件和日志，不切换屏幕。诊断流程结束不等于 APK 已可运行，应查看每项检查与退出码。此入口保留在用户存储，没有开机启动项。

实验镜像剩余约 33 MiB，安装完整 APK 及解压原生库前需要扩大镜像或调整数据存放方式。

实验 `graphics.img` 内额外保存用户提供的 APK、原版 services.jar/odex、图形依赖和中文字体；本地源码附件不含 APK、Android 系统镜像或字体文件。初版 192 MiB runtime.img 仍保留。

EGL 软件驱动显式以 `system/lib/egl/libGLES.so -> libGLES_android.so` 选择。相关原版实现参照 [AOSP SystemServer](https://android.googlesource.com/platform/frameworks/base/+/android-7.0.0_r1/services/java/com/android/server/SystemServer.java) 与 [LineageOS Android 7.1 RenderEngine](https://github.com/LineageOS/android_frameworks_native/blob/cm-14.1/services/surfaceflinger/RenderEngine/RenderEngine.cpp)。本轮运行二进制仍来自此前校验过的 Google API 24 镜像。

## 本地编译

`build.sh` 接受 `KOBO_ZIG_ROOT`（Zig 0.13）、`KOBO_JAVA_HOME`（JDK 17）、`KOBO_ANDROID_ROOT`（本轮裁剪根目录）、`KOBO_ANDROID_JAR`（API 24）与 `KOBO_D8_JAR`。它只编译探针，不下载依赖、不上传设备、不改启动配置。所有匿名 Java 类都要一同传给 D8。Linux framebuffer ABI 头文件使用 Zig 的 ARM Linux UAPI 头文件。

`screen-result.txt` 是实际显示恢复日志。`framework-services-result.txt` 是最后一轮完整运行日志；修复前后记录均保留，不应仅凭退出码判断 Android 应用兼容。

最终结果：`framework-services-result.txt` 中 `FRAMEWORK_RUN_EXIT=0`，`CLEANUP_AUDIT` 后没有实验挂载、loop 设备或模块条目。`surface-view.png` 是实际合成扫描输出，`framework-view.png` 是合成前控件图，`pixel-comparison.txt` 是取回后的独立比较。静态图层测试没有实现触控、Activity 或微信读书启动。


最新进展：原版微信读书的协议正文绘制已在 Forma 上通过验证。私有 WebView 原生组件使用可复现的 24 字节补丁，run32 在关闭调试及强制绘制后仍显示完整中文正文；保存画面的物理墨水屏显示与恢复也通过。APK 未改包、未重签。证据见 [WebView CPU 输出修复](verified-webview-cpu-surface/README.md)。默认网络查询与连接回调已在后续 run38 通过验证；联网变化、二维码登录和阅读仍待完成。


run38 已通过默认网络接入回归：真实 Network(100) DNS/HTTPS、联网信息与未知网络检查、默认和被动连接回调均通过；补齐只读 Wi-Fi 元数据查询后，原版微信读书正常显示主界面及完整协议正文。未启用 WebView 调试，未接受协议或登录；退出清理通过。见 [联网接入实机证据](../network-prototype/verified-host-default-integration/README.md)。
