# 实验模块

仅针对实测 Forma 内核 `4.1.15-00286-g2a6a4cb08c66`。这些文件不是通用固件，也不是微信读书安装包。

- `binder_protocol7.ko`：此次 Android 7 ARM 实验使用；内部模块名为 `binder_linux`。
- `binder_protocol8.ko`：仅完成基础 IPC 测试的另一 ABI 版本；内部模块名也为 `binder_linux`。两者不能同时加载。
- `ashmem_linux.ko`：共享内存模块。
- `kobo_madvise.ko`：进程自身页丢弃及部分访问提示接口，设备名 `kobo_madvise`。
- `libkobo_vm.so`：Bionic ARM32 的显式预加载兼容库，只转发所支持的内存操作，不应加入宿主的全局环境变量。

优先通过设备上已经验证的 `run-java-test.sh` 复测，不要把模块设置为开机自动加载。源码位于旁边的 `module-prototype/`，内核来源与构建说明见其 README。
