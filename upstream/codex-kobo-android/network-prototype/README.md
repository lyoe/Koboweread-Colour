# Kobo 宿主默认网络上的 DNS 与 HTTPS 实验

普通应用 UID 10000 已在 Forma 上完成真实 DNS 解析与 HTTPS 请求：`weread.qq.com` 返回 HTTP 200，使用系统默认 TLS 证书及域名校验，协商 `TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256`，采样读取 16384 字节。协议文件所在的 myqcloud HTTPS 地址也以相同系统校验返回 200，并读取 16384 字节，证据见 verified-dns-https/。保留无效域名失败及原生 IPv6 地址失败检查。这个独立探针通过，不等同于微信读书全部网络功能可用。

`dns-proxy.c` 在私有 Android 目录中发布 `dnsproxyd` UNIX socket，按 Android 7 原始 getaddrinfo 协议返回真实宿主 libc 解析结果。仅支持 netId=0 的宿主默认网络，不提供 gethostbyname/gethostbyaddr、VPN、按网络绑定、路由或防火墙管理。解析器发布端点后降为 UID/GID 1051、附加组 3003、CapEff=0，清空环境并设置 no_new_privs；每个客户端核对真实 UID（仅 0、1000、10000、10006、10007），最多 8 个并发子进程，每个 9 秒期限，整个实验最长 360 秒。socket 位于 root 管理目录，清理由外层 root 脚本负责。

Kobo 原内核未启用 CONFIG_IPV6。Android 7 的 IoBridge 即使连接 IPv4 目标也会先创建 AF_INET6 套接字。`ipv4-sockets.c` 仅在内核以 EAFNOSUPPORT 拒绝该创建时改用真实 AF_INET 套接字，并将 IPv4 映射地址及 bind 通配地址转换为对应的 IPv4 操作；转换流量类别到 IP_TOS。原生 IPv6 目标、scope ID 和无对应语义的选项继续失败。此适配没有 TLS、DNS 或应用成功回调钩子。

证书文件从当前使用的原始 Google API 24 系统镜像 `/etc/security/cacerts` 提取，共 148 个证书，逐文件哈希见 original-ca-sha256.json；它们仅存于私有工作区和实验镜像，不随本源码包分发。没有安装个人 CA、替换信任管理器或禁用证书/域名校验。

`network-uid-exec.c` 只允许固定私有根目录与固定 NetworkProbe 入口，清空继承环境，降为 UID/GID 10000、附加组 3003，设置 no_new_privs，并为该进程加载 VM 与 IPv4 适配。独立测试不暂停 KOReader、不更新屏幕、不更改 Kobo 网络配置，也不登录账号。

构建需设置 KOBO_ZIG_ROOT、KOBO_ANDROID_ROOT、KOBO_JAVA_HOME、KOBO_ANDROID_JAR、KOBO_D8_JAR、KOBO_BUILD_OUT，再运行 build.sh。run-network-test.sh 要求既有私有 graphics.img 及此前已部署的内核模块、运行库和辅助程序，不是独立安装包。

微信读书 runner 的实验开关为 `KOBO_WEREAD_HOST_NETWORK=1`，必须单独复用已有安装，不能与物理显示/触屏测试混用。它启动有界 DNS 服务，进程 broker 只为微信读书主进程附加 IPv4 适配。未启用 HOST_NETWORK_STATE 时，Android 联网状态查询仍由明确的离线服务处理；启用后的默认网络适配见下文。

原始协议和调用行为参考 Android 官方源码：[DnsProxyListener.cpp](https://android.googlesource.com/platform/system/netd/+/android-7.0.0_r1/server/DnsProxyListener.cpp)、[getaddrinfo.c](https://android.googlesource.com/platform/bionic/+/android-7.0.0_r1/libc/dns/net/getaddrinfo.c)、[netdb.h](https://android.googlesource.com/platform/bionic/+/android-7.0.0_r1/libc/include/netdb.h)、[IoBridge.java](https://android.googlesource.com/platform/libcore/+/android-7.0.0_r1/luni/src/main/java/libcore/io/IoBridge.java)。

接入原微信读书后，UID 10007 的多次真实 DNS 解析已通过，但协议页正文仍为空，不能计为 WebView 内容加载成功。证据见 weread-blank-agreement-with-dns/。


此前 run26 的 WebView 只读诊断确认：协议正文实际已加载完成，8346 个字符，布局与 CSS 可见性正常，但 Android 扫描输出中的正文仍为空白。当时重点转为绘制路径，navigator.onLine 为 false。后续绘制修复见下文。证据见 verified-webview-content-not-painted/。


最新进展：原版微信读书的协议正文绘制已在 Forma 上通过验证。私有 WebView 原生组件使用可复现的 24 字节补丁，run32 在关闭调试及强制绘制后仍显示完整中文正文；保存画面的物理墨水屏显示与恢复也通过。APK 未改包、未重签。证据见 [WebView CPU 输出修复](../framework-prototype/verified-webview-cpu-surface/README.md)。默认网络查询与连接回调已在后续 run38 通过验证；联网变化、二维码登录和阅读仍待完成。

新增默认网络适配：`host-default-network.c` 每秒从宿主接口、默认路由及 DNS 配置生成实际观察，写入仅 UID 1051 可更新的私有状态目录。Android 服务只接受 5 秒以内的观察，过期转为无活动网络。它把逻辑 netId 100 映射为现有的、SO_MARK 为 0 的宿主 IPv4 默认路由，不修改宿主路由，也不提供按网络隔离或 VPN。fwmarkd 子集仅支持 ON_ACCEPT、ON_CONNECT 和 SELECT_NETWORK(0/100)，检查实际传入的套接字及应用 UID，其他操作显式失败。原生组件与普通 UID 的九项正负检查已通过，见 verified-host-default-native/；Android Connectivity 服务集成已在 run38 通过验证，见 verified-host-default-integration/。

`KOBO_WEREAD_HOST_NETWORK_STATE=1` 要求同时设置 HOST_NETWORK=1，外层 runner 启动观察器并创建 root 所有的显式标记。无标记时仍使用原离线查询服务。开启时 DNS 支持别名 100，NetworkProbe 通过真实 Network(100).openConnection 进行 HTTPS 验证。没有把网络能力标为 VALIDATED，也不接受网络分配、移动网络或系统配置操作。源码协议参考 [FwmarkCommand.h](https://android.googlesource.com/platform/system/netd/+/android-7.0.0_r1/include/FwmarkCommand.h)、[FwmarkClient.cpp](https://android.googlesource.com/platform/system/netd/+/android-7.0.0_r1/client/FwmarkClient.cpp)。


run38 已通过默认网络接入回归：真实 Network(100) DNS/HTTPS、联网信息与未知网络检查、默认和被动连接回调均通过；补齐只读 Wi-Fi 元数据查询后，原版微信读书正常显示主界面及完整协议正文。未启用 WebView 调试，未接受协议或登录；退出清理通过。见 [联网接入实机证据](verified-host-default-integration/README.md)。
