# 微信读书原始 APK 实机实验

最新第 75 轮：已修复“书友想法”调用提示消息时因缺少通知服务而崩溃。用户复测正常，实际运行 428.74 秒、42 次手势，主动退出返回 0，画面恢复及清理通过。完整 30 分钟和休眠尚未验证。详见 [验证记录](../launcher-prototype/verified-thoughts-toast/README.md)。

启动流程已跳过测试页面并合并重复原文件哈希；本次连续十轮启动与退出全部通过，启动中位数约 151 秒（范围约 133–157 秒），状态提示已增加阶段、等待时间和错误原因。详见 launcher-prototype/verified-startup-bootstrap。完整 30 分钟连续阅读与休眠仍待验证。

当前验证：原 APK 已完成登录、保留登录、书架加载、正文显示与翻页。用户从 KOReader 菜单启动后确认注释打开、翻页、返回书架再进入书本均正常；第 63 轮完整运行与清理退出 0。启动权限、线程优先级及触控窗口过渡处理已修复。现增加最长 30 分钟入口，用户已确认阅读和长按退出正常；完整 30 分钟连续运行及休眠未验证，启动速度与成功率仍在优化。原 APK 未修改或重签。证据见相邻 launcher-prototype/verified-menu-startup。

目标设备为 Kobo Forma 4.26.16704，保留原厂系统。这里记录本机 Android 用户空间的微信读书安装与启动实验，不是可用软件包。

输入为用户提供的 `2.1.2.10245900_900.apk`，包名 `com.tencent.weread.eink`，最低 API 24，目标 API 31，原始 APK SHA-256 为 `04d86bcdc1bbd77cab4ecc4d1c04e869d76dbabc79c60c52a707c64fc3e4b75f`，签名证书 SHA-256 为 `e4dd2f049941989ffafa45aef52eda9701372c27bc1708e89c217935140bfa1c`。未修改或重签 APK。

安装入口仅接受这个固定 APK 和自制测试 APK。微信读书需同时通过完整文件和证书哈希校验；安装由原版 PackageInstaller.Session 执行，完成后再次读取真实 PMS 身份并核对安装字节。其进程 UID、seinfo、数据目录和 SDK 从 PMS 注册，进程后端只额外接受这个固定包的 SDK 31 与声明权限对应的 BLUETOOTH 组 3002、INTERNET 组 3003。进程后端现只将 APK 声明的 `:seg` 子进程映射至同一个 PMS 包身份；UID、SDK、seinfo、数据目录和组校验继续执行。任意其他后缀（包括 `:patch`）仍被拒绝。

`run-weread.sh` 先复验已有实验 Activity 的真实生命周期，再安装微信读书并观察首次启动。`run-weread-test.sh` 负责挂载与清理已有私有镜像。使用相邻 installation-prototype 的构建入口和 framework-prototype 的 Java 源码。这里未包含第三方 APK、Google 系统组件或系统镜像。

安装回调成功、AMS 接受启动请求和 Activity 显示是不同结果；只有实机日志与画面支持的部分才计为通过。网络、外部存储、触控、旋转与休眠的限制仍然存在。下文按开发顺序记录历史故障，各阶段的“尚未”描述以末尾最新验证结果为准。


## 已验证的安装结果

原始 APK 的安装回调为 `status=0 INSTALL_SUCCEEDED`，PMS 分配 UID 10007，安装路径 `/data/app/com.tencent.weread.eink-1/base.apk`。安装文件 SHA-256 与原文件相同；97 个 ARMv7 原生库由安装器提取，dex2oat 用约 88.742 秒完成，生成 79614376 字节的 base.odex。记录见 `verified-first-install/`。

首次启动请求已被 AMS 接受，但进程后端拒绝了实际创建请求，应用尚未进入可用界面。`weread-first-launch.png` 只是启动尝试后的扫描输出，不能作为微信读书运行成功的证据。更详细的启动参数诊断确认 PMS 还分配了蓝牙组 3002；已补上对应校验，正在继续启动验证。


## 主进程与 Application 初始化

补齐 PMS 分配的蓝牙组校验后，真实 AMS 创建了微信读书 UID 10007 的主进程，真实 ActivityThread 调用到 WRApplicationContext.onCreate 与 ModuleInitializer.initModules。此时因 SQLite 原生库依赖 `libandroid.so` 未提取而崩溃，记录见 `verified-application-start/`。该结果证明已经执行微信读书本体，但尚不能显示阅读界面。

对 APK 全部 97 个 ARMv7 原生库的 DT_NEEDED 做递归检查后，确认当前私有系统还缺三个原版 NDK 入口库：libandroid.so、libjnigraphics.so、libOpenSLES.so，共 111492 字节。已从匹配的 API 24 系统镜像补入，已有兼容库未被覆盖；依赖检查没有发现 API 24 镜像中缺失的依赖库名称，报告见 `ndk-dependency-report.json`。这项静态检查不保证所有运行时符号或可选 dlopen 都兼容。


## 不可用外部存储的目录检查

真实存储服务会留下 /storage/emulated 下的 root 专属 0700 目录。后端现在允许这一应用无法遍历的目录外壳，不宣称提供外部卷。应用可遍历的目录、符号链接、其他条目和任何相关挂载记录仍被拒绝。`storage-guard-selftest.c` 在设备的临时 chroot 中测试 8 种情况；挂载检测使用临时文本夹具，不挂载实际卷。结果见 `storage-guard-selftest-result.txt`。这些测试未验证完整的 Android 外部存储语义。


## 公共库命名空间与软件界面入口

只补入 NDK 文件仍不足：此前实验环境的 public.libraries.txt 为精简名单，缺失的库名让普通应用仍报告 libandroid.so 找不到。已恢复同版本镜像的原始 21 项名单，并补齐其余 6 个原版系统库及依赖，共 333680 字节；名单及哈希见 `public-libraries-report.json`。这里不分发这些系统二进制。

微信读书的专用进程入口 `WeReadProcessEntry.java` 在真实 ActivityThread.main 之前设置原始 targetSdk=31，并通过原版 ThreadedRenderer.disable(true) 选择软件 UI 绘制，适配当前没有 GLES 2 的后端。APK 和 Application/Activity 代码未被替换。此入口不实现 GLES、WebView 或网络。`build-entry.sh` 构建入口 DEX，相邻安装实验的原生构建入口构建进程后端。


## 离线网络查询接口

微信读书现已越过原生库加载，真实 Application 的下一处失败是 initBoardCast 创建 ConnectivityManager 时缺少 IConnectivityManager。框架侧新增 `OfflineConnectivityService.java`，仅在实验 config.disable_network=true 且没有已有连接服务时发布，返回无 Android 网络、无代理、无网络能力。网络状态查询保留 ACCESS_NETWORK_STATE 检查；默认代理查询按原版服务的行为不要求该权限。修改网络、注册网络或其他未实现的方法会明确抛出 UnsupportedOperationException。

这不是联网实现，也不会改变 Kobo 宿主网络。真实客户端查询与微信读书下一步初始化正在实测。接口格式及代理权限行为参考同版本 [IConnectivityManager.aidl](https://android.googlesource.com/platform/frameworks/base/+/android-7.0.0_r1/core/java/android/net/IConnectivityManager.aidl) 与 [ConnectivityService.java](https://android.googlesource.com/platform/frameworks/base/+/android-7.0.0_r1/services/core/java/com/android/server/ConnectivityService.java)。


## Application 已完成，进入真实 Activity

离线查询和默认网络监听的注册、注销均已通过原版 ConnectivityManager 客户端实测。微信读书随后成功注册自己的默认网络监听，完成 Application 初始化，并执行到 WeReadFragmentActivity.onCreate / setContentView。监听在应用死亡时已清理，记录见 `verified-activity-create/`。

当前真实 Activity 创建失败点为顶部状态栏的音量控件：缺少 IAudioService。新增 NoOutputAudioQueries.java 提供查询型静音策略：虚拟音量范围 0..15，当前值 0，静音；不提供音频硬件、播放、录音或音量修改。系统点击音效被明确丢弃，音效加载返回 false；未实现的方法明确拒绝。此接口正在用原版 AudioManager 客户端和微信读书验证。


## WebView 接入

静音音量查询已通过原版 AudioManager 客户端验证，微信读书随后越过 TopStatusVolumeItem，进入 WRWebViewPool.preInit。下一处真实失败为缺少 IWebViewUpdateService，日志见 `verified-webview-required/`。

已准备匹配 API 24 镜像中的原版 com.android.webview 51.0.2704.91、预编译文件、功能声明及加载器，共 47813311 字节，哈希见 `webview-component-report.json`。组件仅放入设备私有镜像，附件不分发其二进制。WebViewBootstrap 启动原版服务，在 AMS 实际 ready 后调用原版 provider preparation；微信读书进程入口调用原版地址空间预留函数，并核对真实预留标志。没有放行新的隔离进程；RELRO 创建失败时是否能使用原版回退加载仍待实测。


## 进程失败响应的协议修正

WebView 原版准备路径会尝试启动 RELRO 辅助进程，当前受限后端按设计拒绝该进程。实测暴露了一个协议错误：原版 Android 7 客户端读到负 PID 后立即抛异常，不读取 wrapper 标志；旧后端仍写了该字节，后续复用连接时 PID 因而错位，实际应用无法向 AMS 登记。后端已调整为失败只写 PID，成功才追加 wrapper 标志。详见 pid-protocol-failure.txt。修正后的完整路径正在复测。

运行脚本已增加真实主 Activity 的显示日志检查；发出启动请求、进程存在或显示系统启动图均不会单独计为界面显示成功。


## Chromium 的软件启动与环境白名单

修正进程失败响应后，Chromium 真正加载并初始化到 GLES 2 驱动信息采集；当前 PixelFlinger 后端仅支持 GLES 1，因此发生原生中止，记录见 `verified-chromium-start/`。

原版 Chromium 51 源码提供 `skip-gpu-data-loading`。配合关闭 GPU 合成、GPU 栅格化、加速 Canvas 与 WebGL，并保留软件 UI 路径，已越过原先的 EGL 崩溃，随后遇到 Chromium 普通网络监听接口缺失，记录见 `verified-software-webview/`。配置见 `webview-command-line`，仅供这台开发镜像的可调试 WebView 读取，不填充虚假的 GPU 字符串。软件启动依据见 [GPU 数据管理代码](https://chromium.googlesource.com/chromium/src/+/51.0.2704.91/content/browser/gpu/gpu_data_manager_impl_private.cc) 与 [WebView 配置读取代码](https://chromium.googlesource.com/chromium/src/+/51.0.2704.91/android_webview/glue/java/src/com/android/webview/chromium/WebViewChromiumFactoryProvider.java)。

进程后端已在 exec 前清空继承环境，再仅设置固定 Android 变量。实机检查只看到 ANDROID_ROOT、ANDROID_DATA、ANDROID_STORAGE、ANDROID_ASSETS、EXTERNAL_STORAGE、PATH、BOOTCLASSPATH、LD_PRELOAD 和专用入口 CLASSPATH；不再继承 SSH 或宿主 shell 环境。证据见 verified-software-webview/environment-audit.txt。公开日志去除了原始内存转储，仅保留诊断信息。

被动网络监听的支持与默认网络跟踪相同，均只等待连接、持有生命周期记录，并在注销或死亡时清理。注册不会建立网络，尚未实现实际网络请求、DNS 或联网。新增接口正在实测。


## 首次完整主界面显示验证

原版 RenderScript JNI、CPU 驱动、内核位码和所需数学库已补齐，哈希见 renderscript-component-report.json。随后完整实验通过：原始微信读书 APK 经真实 PMS、AMS、ART 与 WebView 运行，WeReadFragmentActivity 的 Displayed 时间为 +9s149ms，总启动时间 +28s763ms；75 秒观察窗口内没有记录到 FATAL EXCEPTION，脚本输出 WEREAD_REAL_MAIN_ACTIVITY_DISPLAYED，退出码 0。

这是受限的启动和显示验证，不代表应用已可供日常阅读。实际 Android 网络、DNS、登录、书籍打开、物理墨水屏刷新、触控与休眠均未完成。原生媒体音频策略服务尚缺，仍有等待日志；Java 侧只提供静音查询，不提供听书或声音输出。下一步还需验证主线程交互响应与后续页面。


## 本轮保存结果

最终截图见 [登录页](verified-main-display/weread-first-launch.png)，包含“获取二维码”按钮与尚未勾选的协议项；这是本机实际运行的微信读书页面。完整结果见 verified-main-display/runner-result.txt。未进行协议确认、登录或书籍读取。

最终清理已通过，原厂内核启动标识保持不变，KOReader 当前运行，实验进程、模块、挂载和 loop 设备已清理，见 cleanup-result.txt。内核 OOM 关键词扫描未发现记录。此轮不宣称 KOReader 的进程号始终未变。


## 原生音频服务与返回键测试

新增 `native-audio-exec.c` 和 `build-native.sh`，运行匹配镜像中的原版 audioserver。启动器固定实验根目录、关闭继承文件描述符、清空继承环境，在执行前降为 UID/GID 1041，仅附加 audio 组 1005，并设置 no_new_privs；不授予 capabilities。原版程序及新增依赖共 1,245,322 字节，哈希见 native-audio-component-report.json，二进制只放在私有工作目录和待部署包中，不随公开附件分发。实验没有开放 Kobo 音频设备，不承诺播放或录音能力。

`run-weread.sh` 保留默认显示验证。显式设置 `KOBO_WEREAD_NATIVE_AUDIO=1` 时，才会启动原生音频服务并核对真实 Binder 服务登记与 CapEff=0；已在 Forma 上确认初始化成功，UID/GID 均为 1041，附加组为 1005，CapEff=0。

新增 `WeReadInputProbe.java`，使用真实 InputManager 给已聚焦的微信读书主窗口发送一次返回键，要求焦点实际回到自制 Android HOME，再由 AMS 重新打开微信读书并核对焦点。它不点击协议、获取二维码或登录。必须同时设置 `KOBO_WEREAD_REUSE=1 KOBO_WEREAD_INPUT_TEST=1`，且已有受核对的安装身份。

新增部分已在设备重新连通后部署并实测通过。完整运行退出码 0，音频策略服务等待日志为 0，未记录到 FATAL EXCEPTION。返回键经过真实 InputManager 分发，焦点从微信读书回到 HOME；再次通过 AMS 启动后，微信读书主窗口恢复焦点并重新绘制登录页。首次主界面 Displayed 为 +9s192ms（总 +29s63ms），重新打开为 +3s743ms（总 +8s367ms）。证据见 verified-input-and-audio/。


## 物理墨水屏与持续显示桥接

本轮将重新打开后的真实微信读书登录页截图短暂输出到物理墨水屏 5 秒，随后恢复原画面，2,764,800 字节 framebuffer 比较一致，两个经过身份核对的 KOReader 进程已恢复。显示的是实验环境生成的页面截图，尚不是这一检查中的实时刷新。

持续显示桥接见 ../display-bridge-prototype/。独立 5 秒检查和完整页面切换实验均通过，完整实验退出码 0，连续输出 8 帧，三张关键 Android 页面截图均与实际桥接输出帧的灰度校验值匹配。用户确认显示方向和清晰度正常；测试结束后原画面和 KOReader 均已恢复。桥接只负责显示，输入目前仍由受控的 InputManager 测试注入，物理触控、网络、账号登录和阅读尚未实现。


## 触控接入与真实应用响应的边界

物理触屏经宿主单指转发器、root UNIX socket、真实 InputManager，已在独立四按钮窗口按 1→2→3→4 完整验证；用户确认完成，手势数为 4，原画面和 KOReader 恢复。真实应用页面上的第一次文字链接点击没有观察到响应，第二轮物理按钮测试没有获得有效转发手势，不能宣称微信读书物理触控已经可用。

补全手指元数据后，程序化固定点击诊断已在未勾选协议的真实登录页上成功打开微信读书自己的协议与隐私确认窗口。未确认协议、未获取有效登录二维码、未登录。截图与窗口归属证据见 ../display-bridge-prototype/verified-app-button-diagnostic/。原 APK 的触摸分发入口会吞掉异常，因此没有 FATAL EXCEPTION 本身不证明触摸路径完整；目前没有把某项缺失服务确定为无响应原因。

后续仍需复测真实页面物理输入，再完成网络、DNS、登录与阅读。最新诊断结束后清理检查通过，实验进程、模块、挂载均已移除，原内核启动标识保持不变，KOReader 运行。


## 真实微信读书页面的物理触摸跳转（已通过一次）

最新一轮捕获一次落点 (379,707) 的人工手势，打开了微信读书自己的“用户协议”页面。原生输入共 25 条，转发 3 条 MotionEvent，无多指或阻挡帧；最终截图显示真实协议页标题及底部导航，正文仍为空。最终画面与墨水屏桥接输出的灰度指纹匹配，原画面和 KOReader 恢复。证据见 ../display-bridge-prototype/verified-weread-physical-navigation/。

这确认了原始 APK 页面上的一次真实触控导航；没有确认协议、获取有效二维码或登录。网络与 DNS、正文加载、登录、阅读和日常交互仍需完成。此前失败原因不能仅据此归结为手指元数据。


## DNS、HTTPS 与协议正文排查

普通应用 UID 已通过真实 DNS、无效域名失败、原生 IPv6 目标失败及两个 HTTPS 地址的默认系统证书校验。Kobo 原内核没有 IPv6，实验用进程内适配将 Android 的 IPv4 映射套接字转换为真实 IPv4 操作。接入原始微信读书 UID 10007 后，多次 DNS 查询成功；打开协议页等待 20 秒后正文仍为空，尚未完成 WebView 内容加载或 Android 联网状态适配。没有确认协议、获取有效二维码或登录。详见 ../network-prototype/README.md。


此前 run26 的 WebView 只读诊断确认：协议正文实际已加载完成，8346 个字符，布局与 CSS 可见性正常，但 Android 扫描输出中的正文仍为空白。当时重点转为绘制路径，navigator.onLine 为 false。后续绘制修复见下文。证据见 ../network-prototype/verified-webview-content-not-painted/。


最新进展：原版微信读书的协议正文绘制已在 Forma 上通过验证。私有 WebView 原生组件使用可复现的 24 字节补丁，run32 在关闭调试及强制绘制后仍显示完整中文正文；保存画面的物理墨水屏显示与恢复也通过。APK 未改包、未重签。证据见 [WebView CPU 输出修复](../framework-prototype/verified-webview-cpu-surface/README.md)。默认网络查询与连接回调已在后续 run38 通过验证；联网变化、二维码登录和阅读仍待完成。


run38 已通过默认网络接入回归：真实 Network(100) DNS/HTTPS、联网信息与未知网络检查、默认和被动连接回调均通过；补齐只读 Wi-Fi 元数据查询后，原版微信读书正常显示主界面及完整协议正文。未启用 WebView 调试，未接受协议或登录；退出清理通过。见 [联网接入实机证据](../network-prototype/verified-host-default-integration/README.md)。
