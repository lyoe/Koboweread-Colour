# ART 即时编译：独立 Forma 探针

同一 UID 10000、同一算术工作量，两种执行模式的校验结果均为 -3769817。解释执行约 3.086 秒，JIT 约 0.044 秒；真实 ART 线程报告四次编译、四个代码条目，并确认可执行 JIT 缓存映射。两种进程均清空继承环境、降 UID/GID、设置 no_new_privs。代码缓存上限为 8 MiB。该结果只代表此算术探针，不能外推为微信读书整体提速或阅读成功。

实验默认属性关闭 JIT，AndroidRuntime 又在用户参数之后追加属性选项，因此单加 -Xusejit:true 不足以启用。jit-config.c 只对固定探针 UID 或明确启用的微信读书进程提供 ART 进程配置，其他属性继续使用原始 Bionic 查找/读取；不更改全局属性或应用业务返回值。微信读书仍需独立回归。

参考匹配版本的 [AndroidRuntime.cpp](https://android.googlesource.com/platform/frameworks/base/+/android-7.0.0_r1/core/jni/AndroidRuntime.cpp)、[ART 参数解析](https://android.googlesource.com/platform/art/+/android-7.0.0_r1/runtime/parsed_options.cc) 与 [JIT 缓存计数](https://android.googlesource.com/platform/art/+/android-7.0.0_r1/runtime/jit/jit_code_cache.cc)。构建使用上级 build-jit-probe.sh，运行需要已部署的私有实验镜像和内核模块。
