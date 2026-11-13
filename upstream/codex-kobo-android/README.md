# Kobo Forma 本机 Android 环境：可行性验证

开发过程的统一复盘见 [开发复盘与移植参考](开发复盘与移植参考.md)，包含阶段路线、主要故障、失败实验、机型迁移与当前验证边界。

**常驻启动器（2026-10-24）**：Android 只冷启动一次，此后从 KOReader 菜单约 1 秒切回；返回 KOReader 时 Android 留在后台，可正常休眠，唤醒后可继续使用。首次冷启动约 31 秒出现启动器，每次常驻的首次打开微信读书约 26–34 秒，之后直接恢复。另加路由器 DNS 失败时的公共 DNS 回退。用户已确认往返、按键与休眠唤醒；自动关闭钩子（退出、USB、关机、重启）及后台长时间停留尚未实测。见 [常驻启动器](resident-prototype/README.md)。

测试设备已切换为 **Kobo Libra 2**，新的硬件适配、验证范围和构建说明见 [Libra 2 移植](libra2-prototype/README.md)。以下 Forma 记录为历史结果，不代表 Libra 2 已完成相同验证。

第 83 版 REAGL＋文字增强已获用户确认：文字与残影均改善，交互运行、手动退出、恢复和清理通过；完整 30 分钟长测仍待完成。第 84 版真实五秒交接亦已通过，用户确认 KOReader 自动重开且操作正常。第 85 版独立阅读首次测试因触控仍依赖暂停的 KOReader 而退出 116；第 86 版已修复，用户确认独立开书、翻页及返回正常，约 91 秒主动退出、画面恢复和清理通过；完整时限与速度收益仍待验证；常规阅读入口保留原暂停模式。运行环境与缓存继续使用已验证的第 80 版。见 [显示优化记录](launcher-prototype/verified-display83/README.md) 和 [独立运行记录](launcher-prototype/handoff-prototype/README.md)。

第 87 版书架编译参数试验未通过，首次开书约 32 秒，返回书架时发生窗口暂停相关输入 ANR，已完整回退。第 88 版窗口切换触控修复已获用户确认正常，时间大致不变。第 89 版关闭动画的收益有限，已恢复原值。第 90 版单独试验 15Hz 软件合成，设备已核实帧率，人工反馈返回约 14/5 秒、重新开书约 25/14 秒，仍有黑屏和载入等待，暂未证实提速；下一轮恢复 30Hz 采集切换线程栈。见 [第 87 版失败记录](launcher-prototype/verified-shelf87/README.md) 和 [第 88 版修复](launcher-prototype/verified-shelf88/README.md)。

第 81 版预编译实验在返回书架时发生应用无响应并退出 129，未通过回归测试，已撤回该运行缓存，恢复第 80 版。调用栈定位在书架差异计算与列表更新；后续优化以该阻塞路径为重点。第 81 版冷启动也未显示明显改善，不作为日常版本推荐。详见 [失败与回退记录](launcher-prototype/verified-speed81/README.md)。

最新第 80 版已优化触控焦点查询、系统服务 JIT、墨水屏刷新调度和原 APK 身份校验缓存。缓存命中单轮启动 119.46 秒，旧第 77 版单轮 151.34 秒；不同页面、网络和缓存会影响结果，不作为多轮性能承诺。用户确认阅读、按键、全刷及注释正常，12 分钟主动退出和清理通过；开书与书架加载仍较慢，继续优化。第 78 版曾发生后段 Wi-Fi 统计回调异常，第 79/80 版修复并通过两种 UID 调用验证，完整 30 分钟稳定性仍待验证。见 [优化记录](launcher-prototype/verified-speed80/README.md)。


第 70 版基线连续十轮启动与退出通过，启动中位数约 151 秒（约 133–157 秒）。后续改用服务实际就绪检查，并为首次 DB 服务启动增加有限超时宽限。第 73 轮长测试中，用户确认普通阅读、翻页及注释正常，但打开“书友想法”触发缺少通知服务的空指针崩溃，未通过完整 30 分钟验证。第 75 版已启动原生通知服务，用户确认书友想法与翻页均正常；约 7 分钟复测、主动退出及清理通过。详见 [故障与修复记录](launcher-prototype/verified-thoughts-toast/README.md)。休眠仍待验证。

第 76 版日常操作第一阶段已部署：实体翻页键、双键全刷与退出、周期全刷及启动进度。用户已确认操作正常，日志核实 7 次实体键翻页、双键长按退出及清理通过。后续菜单复测已确认启动进度正常，并记录到 2 次手动全刷。详见 [日常操作验证](launcher-prototype/verified-daily-controls/README.md)。

第 77 版已按用户要求部署上键下一页、下键上一页，以及任一翻页键长按 1 秒松手全刷。边界测试与实际操作复测均通过：16 次翻页、3 次手动全刷、双键退出及清理正常。详见 [新按键规则](launcher-prototype/verified-daily-controls-v77/README.md)。

目标：保留原厂 Kobo 系统，由菜单启动设备本机上的 Android 环境，在其中安装 APK，退出后返回 Kobo。运行时允许暂停原厂界面以节省内存，不要求两套界面同时运行。不依赖手机或远程服务器。

当前状态：原始微信读书 APK 已在 Forma 本机完成登录、书架加载、正文显示与翻页。用户已通过 [KOReader 本机入口](launcher-prototype/README.md) 独立启动，并确认注释、书架返回及重新开书正常；第 63 轮完整运行、显示恢复与清理退出 0，断开电脑 SSH 后仍继续运行。原 APK 未修改或重签。现提供最长 30 分钟入口，用户已确认阅读和长按退出正常，设备记录清理退出 0；完整 30 分钟连续运行、休眠与通用 APK 安装界面尚未完成。启动速度和成功率仍在优化。

原生音频服务、返回主页与重新打开测试已在真机通过，协议项保持未勾选。持续显示桥接也已通过完整页面切换测试：8 帧更新覆盖微信读书、HOME、重新打开；原画面恢复字节一致，用户确认方向和清晰度正常。详见微信读书实验说明末尾。

新增触控实验：四个独立 Android 按钮已通过真实触屏顺序点击；程序化单指点击也已打开微信读书自身的协议确认窗，未确认协议。最新真实触屏测试已点击登录页“用户协议”并打开应用内协议页面；当时正文仍为空；一次页面导航已验证，完整交互仍需测试。首次文字链接点击没有可见变化，另一轮未获得有效手势。详情和截图见 [触控与按钮诊断](display-bridge-prototype/README.md)。

新增网络底层实验：普通 UID 的真实 DNS 与默认系统证书校验 HTTPS 已通过，微信读书官网和协议 CDN 均返回 200；微信读书原进程的 DNS 查询也已通过，协议正文已进入 WebView 并完成布局；后续修复了 CPU 同步输出路径，关闭诊断后的实机运行已显示完整中文正文。默认网络状态查询与两类连接回调现已通过实机验证，联网变化仍待测试。详见 [DNS 与 HTTPS 实验](network-prototype/README.md)。

私有图形镜像现为 1280 MiB；首次登录后的 768 MiB 完整备份及第二次扩容前的最新应用数据备份保存在电脑私有工作目录，早期图形验证见 [图形实验说明](graphics-prototype/README.md)。设备保留了 192 MiB 的实验镜像 `/mnt/onboard/.adds/kobo-android-lab/runtime.img` 及测试入口；没有修改启动配置。入口 `sh /mnt/onboard/.adds/kobo-android-lab/run-java-test.sh` 仅用于重复运行 Java 测试，不是 APK 安装器。它在运行结束后卸载实验模块和镜像。

## 先采集设备能力

已经配置 SSH 时，在电脑终端执行下列命令，将 `KOBO_IP` 替换成阅读器地址。脚本通过 SSH 标准输入执行，不需要在阅读器上安装文件；输出报告保存在电脑当前目录。

```sh
ssh -p 2222 root@KOBO_IP 'sh -s' < probe.sh > forma-report.txt
```

若没有 SSH，先建立受控的设备访问方式。不要将本目录改名为 KoboRoot.tgz 刷入：这不是固件安装包，也不会自动配置 SSH。

脚本读取内核、内存、模块、文件系统、显示与输入设备元数据；不加载模块、不挂载分区、不更改系统、不停止进程，不读取书库、账号和触摸事件。报告可能包含设备路径与挂载位置；原始序列号字段不采集。

## 工程决策

1. 根据实机报告确认 ARM 指令集、内核版本、Binder、共享内存、命名空间、模块和文件系统能力。没有 `/proc/config.gz` 或设备节点并不等于内核不支持，需要继续检查匹配源码/模块。
2. 若可保留原内核并补齐模块，优先试验定制 ARMv7 Android 7.1 用户空间。若必须更换内核，先明确这已超出“只装普通软件”的实现方式；不得把它包装成已验证的兼容层。
3. 第一个运行里程碑是 Android 服务启动、包管理器可用和最小示例 Activity 显示，不直接以微信读书复杂界面排查底层。
4. 适配软件渲染、framebuffer/EPDC 刷新、触控坐标、旋转和翻页键。Android 自身的 gralloc、composer 和显示接口需要匹配，单纯 chroot 不足以启动图形应用。
5. 加入启动/退出管理和异常返回机制，实测休眠、网络、内存，再安装目标微信读书，测试登录、打开书籍、翻页、重启后数据保留。

现成 redroid 官方镜像支持 arm64/amd64，不能直接作为 Forma 的 ARMv7 镜像。Waydroid 的常规 Wayland 安装路径也不是可直接复制到 Kobo 的方案。后续必须使用适配后的用户空间和内核接口。

## APK 检查记录

文件：`2.1.2.10245900_900.apk`，包名 `com.tencent.weread.eink`，版本 `2.1.2`，versionCode `10245900`。

二进制 AndroidManifest.xml 显示 minSdkVersion=24（Android 7.0）、targetSdkVersion=31。ZIP 内 arm64-v8a 与 armeabi-v7a 各有 97 个 .so 文件，共 13 个 DEX。原始 APK 已在实机私有环境安装成功；这不代表应用已能正常运行。

## 参考资料

- https://github.com/kobolabs/Kobo-Reader
- https://docs.waydro.id/debugging/getting-essential-information
- https://github.com/remote-android/redroid-doc
- https://github.com/NiLuJe/FBInk


最新进展：原版微信读书的协议正文绘制已在 Forma 上通过验证。私有 WebView 原生组件使用可复现的 24 字节补丁，run32 在关闭调试及强制绘制后仍显示完整中文正文；保存画面的物理墨水屏显示与恢复也通过。APK 未改包、未重签。证据见 [WebView CPU 输出修复](framework-prototype/verified-webview-cpu-surface/README.md)。Android 联网状态接口、二维码登录和阅读仍待完成。


run38 已通过默认网络接入回归：真实 Network(100) DNS/HTTPS、联网信息与未知网络检查、默认和被动连接回调均通过；补齐只读 Wi-Fi 元数据查询后，原版微信读书正常显示主界面及完整协议正文。未启用 WebView 调试，未接受协议或登录；退出清理通过。见 [联网接入实机证据](network-prototype/verified-host-default-integration/README.md)。

第 91 轮已采集两次切换线程栈，看到首页组件重建和阅读页电池查询等待，并观察到较多并发 GC，尚未定位唯一瓶颈。第 92 轮堆空闲余量试验实测首次开书 28 秒、重复开书 15 秒、返回均约 8 秒，尚无明确加速证据，已关闭试验。见 [诊断记录](launcher-prototype/verified-transition91/README.md) 和 [堆余量试验](launcher-prototype/verified-heap92/README.md)。

第 93 轮方法采样发现控件反复查询缺失的 Android 无障碍服务。第 94 轮已启动真实平台服务，并通过独立客户端缓存校验；人工速度对比待完成。详见 [服务补齐记录](launcher-prototype/verified-accessibility94/README.md)。

第94轮人工反馈仍有波动。日志进一步定位到缺失 lowmemorykiller 通道引起的三次一秒重试及全局锁等待；第95轮已部署私有进程 OOM 优先级桥，真实内核写入和恢复通过，启动期重试日志消失，人工开书对比待完成。见 [第95轮记录](launcher-prototype/verified-priority95/README.md)。

第95轮整轮阅读已确认三秒重试消失，开书仍为27/13秒、返回5/9秒，未取得一致整体提速。第96轮正针对独立的系统服务持锁等待建立进程内采样；首次系统自带接口自测没有产出有效文件，未计为通过。见 [第96轮诊断](launcher-prototype/verified-system96/README.md)。
