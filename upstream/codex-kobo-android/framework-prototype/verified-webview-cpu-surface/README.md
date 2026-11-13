# WebView 正文绘制修复：Forma 实机验证

原始微信读书 APK 的协议页面此前已下载并布局，正文却没有进入 Android 合成输出。原因是 Chromium 51 在禁用 GPU 时选择通用软件输出路径，绕过 Android WebView 所需的同步输出表面。

修复仅作用于私有 Android 环境的 WebView 原生组件：固定 SHA256 输入，24 字节改动，选择真实 Android 同步输出表面并保持 CPU 绘制。微信读书 APK 和平台 WebView APK 均未改包或重签；Google 原生库不随源码包分发。补丁脚本和字节报告位于上级目录。

run31 用诊断验证了真实 WebView 的三种绘制输出：均有 77057 个深色像素。run32 关闭 WebView 调试及所有强制绘制诊断后，微信读书自行显示完整中文协议正文，退出码 0。clean-run-agreement-body.png 来自实际 Android 扫描输出。随后把该已保存画面送至 Forma 墨水屏显示 5 秒并恢复，屏幕原始字节校验通过；这次是保存画面验证，不等同于持续刷新或滚动测试。

实验后原 KOReader 两个进程恢复，实验模块、挂载及 Android 进程全部清理。尚未接受用户协议、获取有效登录二维码、登录或打开书籍。Android Connectivity API 仍报告离线，相关适配尚在开发。
