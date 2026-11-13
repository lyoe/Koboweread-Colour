# 第 94 轮：补齐真实无障碍服务

第 93 轮发现控件反复查询缺失的 accessibility 服务。本轮在私有 Android 系统服务进程中创建匹配 API24 的真实 AccessibilityManagerService，通过正常名称发布到 ServiceManager。保留平台服务内部权限检查、设置读取、用户隔离及客户端注册；没有伪造状态、改写客户端缓存或强行关闭无障碍设置。

服务在包管理、用户管理、窗口、输入及通知服务就绪后，启动应用前创建。重复发布或缺少 WindowManagerInternal 时拒绝启动。仅修改自建引导 DEX，保留原 APK、odex、JIT、默认堆设置、动画、30Hz，以及原显示和触控策略。第 88 版窗口暂停检查与代次保护保留。

API24 javac/D8 构建及构建脚本语法检查通过。原引导 DEX 已私有备份，实机已确认真实服务发布。独立客户端实机检查通过：root 进程通过 Binder 接入，当前状态为 enabled=false，一万次缓存读取约 122ms，连接保持。人工书架速度验证待完成。新增 accessibility-client 模式从独立进程接入真实 Binder 服务，核对客户端连接保持，执行一万次 isEnabled 读取，仅打印状态及耗时，不读取页面内容。

源码包包含变更前的自建 FrameworkProbe 快照便于复核；不包含 Android 镜像、编译 DEX、APK、账号或原始采样文件。

人工反馈：启动时位于书内，先返回书架；之后两次开书为21秒和16秒，返回为4秒和10秒，仍有波动。初始页面与上一轮不同，不能据此宣称整体提速。真实服务保留作为兼容层完善。退出后严格清理审计通过。日志另发现 lowmemorykiller socket 失败、约3秒的 SLOW OOM ADJ 和全局锁等待，第95轮单独处理。
