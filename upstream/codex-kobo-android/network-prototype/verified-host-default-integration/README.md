# 宿主默认网络接入：run38 实机验证

Forma 原厂系统和内核保持不变，所有 Android 组件均在私有实验环境内运行。此轮启用了默认网络观察器、fwmarkd 子集、DNS 的 netId 100 别名，以及只读 Connectivity/Wi-Fi 查询桥接。

普通 UID 10000 使用实际 Network(100) DNS 和 openConnection(URL, Proxy.NO_PROXY)，完成官网及协议 CDN 两个 HTTPS 200 请求，保持系统默认证书及域名校验。随后真实 Binder 客户端通过活动网络、网络信息、链路属性、能力和未知网络拒绝检查，并收到默认和被动连接监听回调。未声明 VALIDATED；逻辑网络只代表现有宿主 IPv4 默认路由，不提供网络隔离或 VPN。

微信读书原始 APK 以 UID 10007 正常显示主界面，进入自己的协议页面，完整中文正文仍正常绘制。此轮未启用 WebView 调试或强制绘制。Wi-Fi 查询服务对原始应用 UID 的 getConnectionInfo 明确返回不可用，消除了此前服务缺失造成的空指针异常；未伪造 SSID、速率、DHCP 或成功回调。

runner 退出码 0；退出后没有实验 Android 进程、模块或挂载，原 KOReader 进程仍运行。尚未接受协议、获取有效登录二维码、登录或阅读。离线/重新连接事件、休眠、持续物理交互及日常菜单入口仍需后续验证。此目录中的 PNG 是 Android 实际扫描输出，物理保存画面验证另见 framework-prototype/verified-webview-cpu-surface/。
