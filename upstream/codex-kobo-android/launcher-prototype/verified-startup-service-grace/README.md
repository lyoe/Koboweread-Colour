# 初次绘制时的数据库服务超时适配

第 72 次尝试尚未进入阅读窗口，原 Android 因 DBDataService 超时生成 ANR 并结束主进程，最终入口退出 61。实际 ANR 栈显示主线程仍在 Canvas.native_drawText → CharElement/Paragraph/Page 绘制路径。该次不计为三十分钟运行验证。

reading-server 增加有限的启动宽限：仅针对 UID 10007、主进程 com.tencent.weread.eink、实际过期的 DBDataService、前台服务执行，以及存在 root 所有且权限 0400 的启动标记时生效。时间以真实 Activity 启动请求的 Android uptime 为起点，固定截止于 120 秒。若另有其他服务过期，不应用宽限。首次实际 Activity 显示后及退出清理时删除标记，后续正常阅读使用原服务超时。

实现保留同一个服务超时 message.what 和 ProcessRecord，使原服务完成路径 removeMessages 仍可撤销等待。宽限结束则回到原 ActiveServices.serviceTimeout；没有设置调试应用、伪造服务完成、修改 APK 或关闭全局 ANR。原回调处理其他消息的路径保留。反射字段及原始常量必须匹配当前 API 24，否则引导失败，不继续运行。

边界测试已通过：固定 UID/进程、有限截止、缺少标记、后台服务、其他过期服务以及无效时钟均检查。实机安装及完整长会话结果待补充。

实现依据为同版本原始 [ActiveServices](https://github.com/aosp-mirror/platform_frameworks_base/blob/android-7.0.0_r1/services/core/java/com/android/server/am/ActiveServices.java)、[ActivityManagerService](https://github.com/aosp-mirror/platform_frameworks_base/blob/android-7.0.0_r1/services/core/java/com/android/server/am/ActivityManagerService.java) 和 [Handler](https://github.com/aosp-mirror/platform_frameworks_base/blob/android-7.0.0_r1/core/java/android/os/Handler.java) 的排程、完成撤销与消息分发路径。原始服务时间阈值保持不变，适配层只延后符合条件的初次启动消息。


## 回调契约与收尾自检

主机上的隔离 API 模型已验证真实调用此适配类的消息分发、原 ProcessRecord/message.what 保持、原撤销键匹配、过期后返回原处理、其他过期服务不延期，以及先前回调的转交。还模拟了元数据读取耗时，确认按固定绝对 uptime 截止点排程，读取耗时不会叠加成新宽限。该模型不运行 Android、不调用应用，也不计作实机超时分支通过。

run-host-tests.sh 使用显式 JDK 和中间目录重跑上述测试。host-stubs 仅是主机测试模型，生产 DEX 的类定义已经核对，没有任何这些模拟 android/com.android.server.am 类；设备只接收自制适配类。

收尾版本另向实际 AMS Handler 发送一个私有控制探针，收到自身 token 后才确认消息回调已接入；阅读引导等待此确认。探针不表示应用或服务已完成。完整三十分钟运行保持第 73 版组件不变，结束后才部署此收尾版本并做单独启动复核。
