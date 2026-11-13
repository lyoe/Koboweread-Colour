# 真实测试 APK 的本机运行实验

此目录记录保留 Kobo Forma 原厂系统的 Android 7 ARM 原型。不是固件安装包，也不是通用 APK 安装器。微信读书尚未安装或启动。

自制 `com.kobo.lab.smoke` APK 由原版 PackageManager 扫描、注册，AMS 通过 Android 的 Process.start 协议启动原版 app_process32，随后由 ActivityThread 执行真实 Application/Activity 生命周期。应用进程 UID/GID 为 10000，具有 Android 请求的标准附加组 50000、9997；界面由 WMS、ViewRoot、Surface、SurfaceFlinger 实际绘制，读取实验 framebuffer 保存为 `activity-window.png`。没有远程渲染。测试 APK 的 HomeActivity 提供真实 HOME 界面，ProbeActivity 在独立任务中启动；其自动 finish 后收到 onDestroy，主页恢复 onResume，完整测试退出码 0。

测试 APK 被放在私有镜像的 `/system/app/KoboProbe`，带有 coreApp、persistent、directBootAware 标志，以适配当前只启动核心服务的引导实验。这尚不能证明普通第三方 APK 可安装或微信读书可以运行。

## 进程启动后端

原厂内核没有命名空间。原版 Zygote 的存储命名空间建立会失败，所以本阶段用 `process-broker.c` 接收真实 Android 7 Zygote 请求并 fork/exec 原版 ART，返回实际 PID。它仅接受固定测试包、UID/GID 10000、SDK 24 与限定参数；不支持外部存储挂载请求、任意包启动、SELinux 域切换、cgroup 或通用 Android 沙箱。不是完整 Zygote 实现。

## 存储服务

`vold-socket-exec.c` 提供真实 vold/cryptd 继承套接字，执行实验镜像中的原版 Android vold。`kobo-vold-scope.c` 仅为这个进程替换 `/proc/mounts`、`/proc/self/mounts` 的读取和 `/proc` 目录枚举：前者只含实验挂载记录，后者为空目录。存储命令仍由原版 daemon 处理，未伪造协议成功响应。启动前检查未挂载真实 sysfs、没有块设备、没有 vold 管理的磁盘配置。所有入口先关闭继承的非标准文件描述符；`audit-runtime-fds.sh` 在实机复查各实验进程没有块设备或原系统显示/输入设备句柄。

该限制不是安全容器；原厂内核没有命名空间，实验 daemon 仍有 root 权限。当前只用于已知代码的受控测试。未把真实磁盘、Kobo 书库或触摸设备挂入实验目录。`/proc` 仍来自宿主；chroot 与这组限制不能作为完整安全边界。

## 构建与运行

- `build.sh`：使用 API 24 android.jar、JDK 17、D8 和 aapt2 构建并签署自制测试 APK。参数见脚本开头；测试密钥自动生成在构建目录，不能用于发布。
- `build-native.sh`：使用 Zig 0.13 和匹配 Android 的 libc/libdl 交叉编译进程后端及 vold 辅助程序。
- 框架入口由相邻 `framework-prototype/build.sh` 构建；ServerLauncher 保持独立 DEX，使用平台共享 native namespace，避免重复载入 libandroid_runtime。
- `run-apk-test.sh`：只适用于已经准备好的这台设备镜像，调用 `run-apk.sh` 后清理服务、模块和挂载。不是安装步骤。

本目录包含自制测试 APK，不包含 Google Android 镜像、系统库、微信读书 APK 或账号数据。后续还需完成通用包安装、外部存储、必要系统服务、物理屏幕与触控的持续接入，以及微信读书实测。

## 当前网络限制

仅在 app-server 实验的私有属性区设置 `config.disable_network=true`，使用 Android 原版 ContentService 的条件分支停止创建 SyncManager。此次生命周期实验不启动 ConnectivityService、netd 或通用 JobScheduler。原版 ContentService 对关闭同步后的适配器查询缺少空值保护，因此 `OfflineSyncAdapterQueries` 仅转接这一项 Binder 查询和包管理器的同项回调，直接调用原版 SyncAdaptersCache 查询真实包数据，并沿用原版跨用户权限检查；其余请求仍转交原版 ContentService。没有把网络状态或存储命令伪装为成功。该开关没有建立网络隔离，也没有关闭 Kobo 的 Wi-Fi；Android 应用的网络管理 API 和账户同步仍待适配，不能据此声称支持微信读书联网。

Android ARM32 的 `RTLD_NEXT` 为 `0xfffffffe`，与 LP64 不同；兼容库使用匹配的值，见 [Android 7 Bionic dlfcn.h](https://android.googlesource.com/platform/bionic/+/android-7.0.0_r1/libc/include/dlfcn.h)。

## 已保存的验证材料

- `activity-lifecycle-result.txt`：完整启动、显示、退出、清理日志。
- `activity-window.png` / `activity-returned-home.png`：实际应用界面与返回主页画面。
- `device-artifact-sha.txt` / `build-artifacts.json`：实机与本机构建产物摘要。
- `final-cleanup.txt`：模块、挂载、loop 设备、进程清理检查及 KOReader 状态。

当前图像来自文件型显示；不代表已完成实体触控或持续墨水屏交互。实验镜像挂载启用 `user_xattr`，用于原版包管理器的真实扩展属性读写；不提供 SELinux 强制访问控制。

## 验证边界

最终日志不再有扩展属性 ENOTSUP 或应用/框架致命异常。日志仍包含部分电源/统计节点缺失、ART 引导标记不存在以及 GLES 缩略图截图接口不可用的消息；这些功能仍未补齐。提供的 PNG 直接读取软件 framebuffer，不是 WMS 缩略图截图。当前软件渲染器不具备已验证的 GLES 2/WebView 支持。

`rebuild-verification.json` 记录构建复核：7 项产物摘要直接匹配设备，另外 3 个静态 ELF 因构建路径的非装载元数据而摘要不同，已逐节确认所有装载节与已上传副本完全一致。`build-artifacts.json` 记录该早期生命周期阶段当时部署的摘要。当前自制引导 APK 已增加 reading 配置，见 `current-bootstrap-artifact.json`；早期日志和摘要保留原样。


## 当前阅读引导 APK

`kobo-activity-probe.apk` 已更新为同一私有测试证书签名的自制引导版本。reading 配置保留 Application attach 与 AMS ready，但不会安排测试 Activity；原诊断配置仍执行原生命周期测试。微信读书 APK 未改动。当前产物摘要见 current-bootstrap-artifact.json，实机验证见相邻 launcher-prototype/verified-startup-bootstrap。

使用 build-apk.sh 可重新构建自制引导 APK，需要显式提供现有签名密钥；私钥不随源码包分发。该构建入口不处理微信读书或 Google 系统 APK。
