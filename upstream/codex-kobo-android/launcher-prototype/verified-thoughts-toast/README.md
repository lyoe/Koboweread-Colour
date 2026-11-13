# 书友想法：缺少提示消息服务导致崩溃

第 73 轮进入阅读并记录到 40 次真实手势。用户反馈其余操作正常，显示书友想法时退出。运行器退出 117，原应用主线程抛出空指针：Toast.show 调用空的 INotificationManager.enqueueToast；上层调用为 toggleReviewDiscuss。并非记录为定时正常退出，也未通过完整 30 分钟验证。

修复在平台引导阶段启动原有 NotificationManagerService 并核验 Binder 存活。保留应用原始提示消息、通知权限检查与消息队列，不修改微信读书 APK，不捕获后忽略异常。实机初始化及页面交互验证结果见 verification.json。原始含账户日志仅留在私有工作目录。

平台实现参考：[AOSP Android 7.0 NotificationManagerService](https://github.com/aosp-mirror/platform_frameworks_base/blob/android-7.0.0_r1/services/core/java/com/android/server/notification/NotificationManagerService.java)。

当前只执行平台服务 onStart 并确认 notification Binder 存活；没有声称通知栏、声音、振动或完整通知生命周期已支持。提示消息的显示、排队和消失使用原平台实现，本轮用户已确认书友想法和翻页均正常。

第 75 轮复测通过：用户确认各项操作正常；实际交互运行 428.74 秒，42 次手势、223 个输入事件，未发现 Java/原生崩溃或微信读书 ANR，输入拒绝为 0。用户长按左上角退出，控制器返回 0，画面恢复字节一致。退出后无 Android 进程、实验模块、镜像挂载或 loop 设备残留，原 KOReader 进程已恢复。本次属于约 7 分钟的故障复测，未验证完整 30 分钟或休眠。
