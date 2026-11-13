# 窗口事务等待：独立服务端诊断

客户端采样发现 activityPaused 等待约4.6秒后，本轮先用最多两次标准 ART SIGQUIT 抓取私有 Android 服务进程线程。两份文件完整，但快照未命中持锁路径，不能据此判断锁的拥有者。

随后使用独立服务端引导 DEX，在已经验证的94版框架上仅加入 root 可调用的有界采样服务。原 `install-framework-probe.dex`、原进程启动器及普通菜单入口的摘要保持不变；独立入口才会选择该诊断框架。

## 实机结果

- 一秒自测生成5866字节、130条完整 ART v3 记录，通过启停与格式核验。
- 正式采样间隔100毫秒，请求120秒，实际约120.13秒；57471字节、1200条记录，无缓冲溢出、调用栈匹配错误为0。
- 用户完成开书和返回，服务端多个线程的等待同时得到记录。
- 窗口动画线程两段 `SurfaceControl.nativeCloseTransaction` 分别约4.91和8.52秒。
- ActivityManager 线程在 `resumeKeyDispatching` 内累计约11.73秒；另有 activityPaused、方向设置与 UI Handler 的等待。

这些是并发线程的采样区间，不能互相相加作为总耗时，也不是无诊断开销下的提速空间。数据将后续调查指向窗口管理与合成事务路径，尚未证明 SurfaceFlinger 内部的具体原因。

## 隔离与隐私

诊断服务只允许 root、固定文件、一次自测后一次正式采样，不接受调用者自定义目标或时限；绑定当前就绪文件的 stat 身份，不读取其 root-only 内容。输出目录 UID1000、0700，应用 UID 无法遍历。实际 ART 输出在完成时可能重设文件权限，采集控制器在读取前将文件改为0600；原始 trace 和线程快照只存私有目录，不进入源码包。

独立诊断会话已正常退出，显示精确恢复、KOReader 返回，以及进程/模块/挂载/loop 清理通过。它属于诊断，不是性能修复。后续 RAM scanout 对照恢复普通原框架，不运行方法采样器。

## 源码参考

平台事务处理参考 [AOSP Android 7 SurfaceFlinger](https://android.googlesource.com/platform/frameworks/native/+/android-7.0.0_r1/services/surfaceflinger/SurfaceFlinger.cpp) 和 [SurfaceComposerClient](https://android.googlesource.com/platform/frameworks/native/+/android-7.0.0_r1/libs/gui/SurfaceComposerClient.cpp)。实际运行库仍为项目已核验组件；源码分析不替代二进制实测。
