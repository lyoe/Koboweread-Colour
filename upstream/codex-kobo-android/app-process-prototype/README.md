# 普通应用 UID 运行实验

已在 Forma 原厂内核运行原版 Android app_process32，进程 UID/GID 均为 10000、附加组为空、有效 capabilities 为 0。启动器先 chroot，随后真实放弃权限，并设置 no_new_privs；没有安装 setuid 可执行文件。

## 已验证

- ART 成功执行自有 DEX，Java 检查 UID，并读取 /proc/self/status 核实权限。
- 通过独立 root 服务和原始 Binder 驱动完成往返，服务端核实 Binder.getCallingUid() 为 10000。
- 读写 UID 10000 所有的 0700 测试目录，并验证写 root 所有的 lab 目录被拒绝；测试文件和目录均已移除。
- 8 轮每轮 16 MiB Java 对象分配、内容校验和 GC 通过，日志中可见实际回收 32 MiB 大对象的记录。
- 运行时堆属性通过真实 SystemProperties 协议设置：最大堆 64 MiB、增长上限 48 MiB；进程报告 maxMemory=50,331,648 字节。第一次测试触发 AndroidRuntime 默认 16 MiB 堆的 OOM，已保存失败日志。命令行 -Xmx 会被随后添加的产品堆属性覆盖，应配置正式属性。
- `ordinary-uid-result.txt` 最终 `UID_RUN_EXIT=0`。编译脚本与设备 DEX/启动器 SHA256 对应关系见 `build-artifacts.json` 和 `final-cleanup.txt`。

只对实验镜像内的 binder、ashmem、kobo_madvise 设备节点开放普通 UID 访问。内存辅助模块只操作 current->mm；内核源码并没有 root/capability 硬编码检查，之前的 root 限制来自节点权限。此测试不意味着具备 Android 完整安全隔离：原内核没有命名空间、cgroups 和 SELinux 强制执行。

## 与 APK 启动的距离

此测试直接启动 app_process 执行自有类，未安装 APK、未注册 AMS ApplicationThread，也未创建 Activity。另一个 root 窗口探针已通过绘制与合成点击，这两条链路还没有合并为普通 APK 的生命周期。

原版 Android 7 Zygote 的 MountEmulatedStorage 无条件调用 unshare(CLONE_NEWNS)，即使没有外部存储也会调用；当前内核不支持该功能。不能简单伪造 unshare 成功并继续挂载。后续需实现明确不使用挂载命名空间的应用启动适配，或受控的逐进程 exec 启动后端，并接入 AMS 的真实进程记录、ApplicationThread、包管理和数据目录。

参考源码：[Zygote 原生实现](https://android.googlesource.com/platform/frameworks/base/+/android-7.0.0_r1/core/jni/com_android_internal_os_Zygote.cpp)、[AndroidRuntime 堆配置](https://android.googlesource.com/platform/frameworks/base/+/android-7.0.0_r1/core/jni/AndroidRuntime.cpp)。这是源码分析结论，尚未运行原版 Zygote fork 测试。

## 编译与复测

`build.sh` 使用 Zig 0.13、JDK 17、API 24 android.jar 和 D8；环境变量与相邻 framework-prototype/build.sh 一致，不需要 KOBO_ANDROID_ROOT。只编译自有启动器和 DEX。

设备已部署入口：

```sh
sh /mnt/onboard/.adds/kobo-android-lab/run-uid-test.sh
```

入口复用 graphics.img；临时加载模块和只读 proc，启动测试属性服务、Binder 服务及普通 UID 子进程，完成后清理。未设置自启动项，不改变原厂界面，不运行用户 APK。
