# 第 78 版性能优化：测量与验证

基线第 77 版从菜单控制器开始到可交互 151.34 秒，其中框架 28.88 秒、应用阶段 105.42 秒。此处是单轮记录，不代替此前第 70 版的十轮统计。

本轮替换每次触控/翻页时导出整份 WMS 窗口信息的路径，改为系统服务持有原 WMS 锁时查询真实 mCurrentFocus、mOwnerUid 与 mAttrs。服务只接受 root 调用，返回固定焦点分类，不缓存窗口归属，不返回私有窗口标题；原 APK UID 与签名核验保留。实体翻页仍仅作用于已验证阅读窗口，触控只作用于该应用所属窗口。初始化保留一次旧窗口导出，用于原有启动检查和计时对照。

显示提交间隔从 1 秒缩至 0.25 秒，并等待 EPDC 更新完成，避免简单加速后积压更新。该值是软件节流间隔，不代表墨水屏可以每秒完成四次刷新。周期与手动 GC16 全刷、画面恢复及清理保留。日志增加焦点查询、按键注入、面板完成耗时。

reading profile 为系统服务增加 root 只读标记，包装器核验标记后才加载单进程 JIT 属性适配器，并以 -Xusejit:true 启动。UID/GID、capabilities 和继承给子进程的环境规则保持原实现，普通应用仍使用原固定身份与 signer 核验；不修改全局 ART 属性或 stock 系统配置。JIT 缓存最多 8 MiB，实际映射、内存及稳定性需实机确认。

LiveFocusServiceTest 使用隔离的 Android API 夹具执行真实服务代码，覆盖空焦点、动态 UID/包名变化、弹窗、非 root 拒绝、错误接口 token 和多余参数；夹具只用于电脑测试，不编译进入生产 DEX。实测数值与尚待验证项见 verification.json。原始带账户日志仅留私有工作目录，不随源码分发。

平台字段及同步依据：[AOSP WindowManagerService](https://github.com/aosp-mirror/platform_frameworks_base/blob/android-7.0.0_r1/services/core/java/com/android/server/wm/WindowManagerService.java)、[AOSP WindowState](https://github.com/aosp-mirror/platform_frameworks_base/blob/android-7.0.0_r1/services/core/java/com/android/server/wm/WindowState.java)。
