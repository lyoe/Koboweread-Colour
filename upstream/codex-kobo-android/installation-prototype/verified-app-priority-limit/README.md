# 普通应用线程优先级限制

第 55 轮已显示正文并由用户确认翻页，但稍后 beacon-thread-3 的 HandlerThread 因 Process.setThreadPriority 抛出 SecurityException 而退出。进程后端缺少 Android init 向子进程传递的 RLIMIT_NICE。

[AOSP Android 7 init.rc](https://android.googlesource.com/platform/system/core/+/android-7.0.0_r1/rootdir/init.rc) 设置该限制为 40/40。本后端现在只在注册应用的子进程中设置相同限制，再按 PMS 设置补充组并降低 UID；未更改宿主全局限制，也未向应用授予 CAP_SYS_NICE。

设备上的自制探针以真实 UID 10000、零有效能力运行：旧限制下提高优先级被内核拒绝；新限制下实际完成 -8、0、10、-2、0 的转换并逐次读取验证。随后真实微信读书主进程与 :seg 子进程的 /proc/limits 均为 40/40，CapEff 均为 0。完整应用回归结果见后续记录。

第 57 轮真实应用期间未记录到 Java 致命异常或优先级异常，但触控焦点过渡导致测试退出 119，因此整轮稳定性仍不计为通过。
