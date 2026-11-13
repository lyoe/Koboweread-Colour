# 第95轮：私有进程 OOM 优先级桥

第94轮日志反复出现 lowmemorykiller socket 连接失败、约3000ms的 SLOW OOM ADJ，以及 AMS 全局锁等待。匹配 Android 7 ProcessList 源码确认连接失败会等待一秒、重试三次。本轮为私有 Android 进程补齐优先级通道。

原生桥接收 API24 SOCK_SEQPACKET 协议，仅允许私有 chroot 中的 system app_process32 或 root 诊断客户端。套接字属主root、组system、模式0660。目标必须匹配真实UID、私有chroot、app_process32可执行路径，优先级范围为-1000至1000。通过已经验证的 proc 目录FD打开 oom_score_adj，写入后读回核对，避免PID复用将写入转向其他进程。Kobo原系统进程不在目标范围。

这不是完整的 Android lowmemorykiller：阈值配置只作为校验后的 advisory 记录；删除命令不维护主动杀进程名单。桥不监测内存压力、不主动杀进程，保留Kobo内核的OOM机制，不宣称实现了Android内存压力回收。

协议长度、字节序、UID、PID、优先级、阈值顺序和未知命令拒绝测试通过。真实设备已验证system、smoke和微信读书的内核优先级写入；smoke测试从-800改为-799并恢复-800，写入和恢复均读回通过。启动阶段未再记录连接失败或SLOW OOM ADJ；人工开书/书架速度仍待验证。

根运行器在框架启动前启动桥，在应用和框架结束后终止并等待回收。只删除自身创建且inode仍匹配的套接字，拒绝覆盖旧套接字。原根运行器已备份。原APK、odex、JIT、默认堆设置、动画、30Hz及显示触控策略保留。

依据：[AOSP ProcessList.java](https://android.googlesource.com/platform/frameworks/base/+/android-7.0.0_r1/services/core/java/com/android/server/am/ProcessList.java)。原始日志、Android文件和二进制不包含在源码包中。

人工反馈：开书27/13秒，返回5/9秒，首次开书仍黑屏约21秒；没有一致的整体提速证据。整轮设备日志中连接失败与 SLOW OOM ADJ 均为0，13次内核优先级写入通过，桥正常关闭，套接字消失，严格清理审计通过。仍有独立的 ActivityStack 约5.4秒持锁，第96轮针对系统服务采样，不放宽超时。
