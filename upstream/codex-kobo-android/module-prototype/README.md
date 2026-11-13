# 模块原型与构建记录

Binder/ashmem 基于 https://github.com/anbox/anbox-modules ，提交 `98f0f3b3b1eeb5a6954ca15ec43e150b76369086`。保留 GPLv2 许可文本。两个 Forma 补丁记录依赖查找、缺失符号预检查和函数名修复。Binder 卸载前还会 flush 延迟释放工作，避免待执行任务引用已卸载的代码。

内核来源：https://github.com/kobolabs/Kobo-Reader/tree/master/hw/imx6sll-forma 。构建配置来自实机 `/proc/config.gz`，保存于上一级 `forma-kernel.config`。

构建主机为 Apple Silicon macOS，Apple Clang 14 编译 ARM EABI 目标，Zig 0.13 的 LLD 进行 ELF 链接，旧内核宿主工具使用 Clang 和 musl 的 elf.h 构建。`build-tools/` 提供编译器和链接器包装器，可通过 `KOBO_CLANG`、`KOBO_ZIG` 指定工具路径。当前还没有整理为自动下载依赖的一键构建脚本。

`macos-clang-build.patch` 仅用于构建树：

- 将两个旧式 ARM 汇编助记符改写为 LLVM 接受的等价拼法。
- 为旧内核的 Clang 分支补齐 cold 注解，保留与原构建一致的 printk 符号签名。
- 修正旧 modpost 在现代 macOS 上将可重定位指针放入只读段的问题。

编译模块需要 `-fno-pic -fno-pie`；旧式 GCC 专用参数由编译器包装器过滤。`LOCALVERSION=-00286-g2a6a4cb08c66` 匹配实机版本。生成了覆盖依赖的部分符号版本表，并对照原厂 Wi-Fi 模块验证 76 个 CRC；该表不是完整内核构建产物。

Binder 默认构建协议 8。此次系统镜像使用的协议 7 变体另外传入 `KCFLAGS=-DCONFIG_ANDROID_BINDER_IPC_32BIT`。不能根据“Android 7”这一版本名自动选择 ABI。

`vm-helper/` 为针对 Linux 4.1 的有限 madvise 辅助接口；页丢弃与 VMA 标志更新逻辑依据同版本 `mm/madvise.c`，通过 ioctl 作用于调用者自身的 mm。没有 syscall table hook。其余 madvise 操作继续返回原有错误，尚未覆盖全部 Android 使用场景。

`kobo-vm-shim.c` 编译为 Android ARM32 PIC 共享库并链接此实验的 Bionic libc；测试启动器在 chroot 后仅为 ART 子进程设置 LD_PRELOAD。它不适用于 Kobo 的宿主 libc。
