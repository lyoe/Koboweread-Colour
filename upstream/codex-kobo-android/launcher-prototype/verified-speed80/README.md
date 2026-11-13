# 第 80 版：校验缓存、统计接口修复与刷新调度

第 78 版用户反馈测试正常，105 次实体翻页的适配处理耗时中位数 15 ms，实际焦点查询中位数 1 ms，面板刷新中位数约 545 ms；启动 140.53 秒，较第 77 版单轮 151.34 秒改善约 7%。但该轮后段系统服务在 batterystats-sync 抛出异常并退出 117，因此不计完整长测通过。

Wi-Fi 修复按 API 24 IWifiManager 的 requestActivityInfo(in ResultReceiver) 解析 presence 标记和回调参数；oneway 调用不写 reply，使用空 Bundle 明确表示没有控制器能耗记录，绝不伪造零能耗或联网成功。reportActivityInfo 返回 null，getSupportedFeatures 返回 0，与没有 supplicant/控制器统计桥接的现状一致。系统 UID 1000 的同进程 Proxy 调用、root 远程 Binder 调用和 null receiver 均已实机通过。

校验缓存仅用于固定原微信读书 APK 的 reading 复用启动，记录 root 私有只读文件，精确绑定原 SHA-256、签名、UID、两个固定文件的位置及设备号、inode、大小、mtime、ctime、属主、组和模式。仅信任 root 的源 APK 和 system 的已安装 APK，均须普通文件且组/其他用户不可写。命中仍向 PMS 核对唯一签名和 UID；缺失、属性变化、记录损坏或权限不符时走完整 SHA-256。完整校验前后再次比对属性，随后原子写入缓存。诊断/新安装/其他包继续完整校验。这里假设实验运行环境与 root 维护流程可信；此缓存不是对抗 root 篡改的容器完整性机制。

两份 APK 实际合计 254,935,284 字节（约 243 MiB）；原生 sha256sum 两份耗时 13.75 秒，并未优于原 Java 校验，故未采用。首轮完整校验阶段 10.694 秒，命中缓存 0.139 秒；首轮启动 145.45 秒、第二轮 119.46 秒。均为单轮测量，缓存、网络与页面会影响启动时间，不视为多轮稳定性统计。原 APK、签名、账号与书籍数据未更改。

缓存失效测试只创建独立合成文件，覆盖正常记录、权限错误、源文件变化、符号链接和损坏记录，不修改真实 APK。首个独立诊断进程漏带 Android 环境变量而在进入 main 前退出；已补齐与正式运行器一致的环境并重跑通过，不计作应用自身故障。

刷新调度继续等待面板完成，0.25 秒节流从提交开始计时，避免面板已完成后再额外等待 0.25 秒。菜单、按键映射和手动全刷不变。

最新人工确认开书、翻页、全刷和注释均正常，但开书及返回书架刷新仍较慢。该轮实际交互运行 727.48 秒，由用户实体双键主动退出，返回 KOReader、原画面恢复与运行组件清理均通过。没有运行满 30 分钟，不计完整 30 分钟验证通过。原始认证日志只保留在私有工作目录。接口依据：[AOSP API 24 IWifiManager](https://android.googlesource.com/platform/frameworks/base/+/android-7.0.0_r1/wifi/java/android/net/wifi/IWifiManager.aidl)。
