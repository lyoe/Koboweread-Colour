# 书架加载与阅读子进程故障

第 50 轮中，原始 APK 的真实书架已显示内容。用户点击书籍后黑屏、回到测试 HOME，再返回 KOReader；不能记为阅读通过。

私有日志显示 ReaderFragmentActivity 请求 SegmentService，进程后端拒绝 com.tencent.weread.eink:seg，AMS 记录 start failure 并停止整个包；触控随后因焦点丢失关闭。APK 清单明确声明这个非导出服务的进程为 :seg。修复只接受此固定进程名，以主包的 PMS 注册身份启动；不放行任意后缀。实机修复验证尚待完成。

该轮退出 119，原画面与 KOReader 已恢复，未遗留 Android 进程、模块或镜像挂载。个人书架画面、账号数据和原始日志均未收入本目录。
