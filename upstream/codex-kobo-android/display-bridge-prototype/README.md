# Android 画面到 Forma 墨水屏的持续桥接实验

`live-display.c` 在宿主端读取 SurfaceFlinger 的实验文件 framebuffer 和发布状态，将稳定的 720×960 RGBA 画面转成灰度并放大两倍，通过 FBInk 更新 1440×1920 的物理屏幕。每秒最多更新一次；序列变化但像素未变时不刷新。当前只支持这台 Forma 的竖屏，其他方向明确拒绝。

程序查找并核对当前两个 KOReader reader.lua 进程的可执行路径、父子关系与启动时间，暂时停止它们并确认停止状态。原画面仅保存在 RAM。正常结束、超时或捕获到 SIGINT/SIGTERM/SIGHUP 后，恢复原画面、逐字节检查并恢复相同身份的 KOReader 进程。SIGKILL、内核崩溃或掉电无法执行进程内清理。最长运行 240 秒，不提供常驻入口；没有开放物理输入设备到 Android。

5 秒单帧真机检查已通过：成功绘制一次、恢复 2,764,800 字节原 framebuffer 并恢复两个 KOReader 进程。完整页面切换验证也已通过，退出码 0：持续输出 8 帧，真实 InputManager 返回 HOME 后重新打开微信读书。登录页、HOME、重新打开后的三张 Android 截图均与桥接实际输出记录中的灰度 FNV 校验值匹配；原 framebuffer 字节恢复一致，两个 KOReader 进程恢复。用户同时确认能看到页面，方向和清晰度正常。证据见 verified-live-display/。

编译依赖 Zig 0.13 和未修改的 FBInk 1.25.0 源码（含子模块文件），设置 KOBO_ZIG_ROOT、KOBO_FBINK_ROOT、KOBO_BUILD_OUT 后执行 build.sh。FBInk 源码压缩包和 GPL-3.0 许可证保存在 ../graphics-prototype/；本程序也使用 GPL-3.0-or-later。

run-weread.sh 仅在 KOBO_WEREAD_LIVE_DISPLAY=1 且 KOBO_WEREAD_INPUT_TEST=1 时启动桥接；测试结束后检查 framebuffer 恢复结果。它不是日常启动器，也不提供触控、联网或休眠适配。


触屏元数据由 input-metadata.c 读取，不读取事件、不调用 EVIOCGRAB。真机设备名 cyttsp5_mt，ABS_MT_POSITION_X 范围 0..1919，ABS_MT_POSITION_Y 为 0..1439；原始轴与竖屏显示轴不同，尚未确定反向关系、接入手势或转发真实触摸。绝对轴 ioctl 对未启用轴也可能返回全零结构，不能将全零视为该轴可用的证据。


## 四点真实触控实验（已通过）

`touch-forward.c` 只在核对的两个 KOReader 进程均已停止时运行。它打开固定的 cyttsp5_mt 设备，检查原始坐标范围，在测试期间调用 EVIOCGRAB，防止触摸同时积压到 KOReader。转换为 Android 坐标的公式为 x=(1439-rawY)/2、y=rawX/2，与设备上 KOReader 的交换轴、镜像横轴规则一致。

当前只处理 Forma snow 协议的单指 DOWN/MOVE/UP，使用 BTN_TOUCH=0 结束接触；多指取消并等待全部抬手，收到 SYN_DROPPED 则停止。转发器不写入 evdev，也不把宿主设备节点开放给 Android。它通过 root:root 0600 的固定 UNIX socket，向 root Java 测试进程提交带序号的有限大小消息；双方核对真实 peer UID。

WeReadTouchProbe.java 创建覆盖整个显示区域的不透明四按钮窗口，用户必须依序点击 1、2、3、4。真实 InputManager 接受输入、Android Button 回调依序触发后才写入完成标志。测试不自动点击、不确认协议、不登录，不把这些触摸交给下面的微信读书页面。完成、超时或中断后释放触屏，再恢复原画面和 KOReader。

此阶段的输入链路仅用于独立测试窗口。尚未完成微信读书实际页面的触控接入、物理翻页键、多指、旋转或日常运行验证。启用方式是既有测试标志再加 KOBO_WEREAD_TOUCH_TEST=1；它不会作为常驻服务启动。


四点实验完整退出码 0，原生转发器记录 4 次完整手势、20 条 MotionEvent 消息；真实 Android Button 回调顺序为 1、2、3、4。用户回复“完成”，最终画面包含四个勾选按钮。原 framebuffer 字节恢复一致，KOReader 的原进程恢复，运行时检查确认 Android 进程没有宿主 evdev 文件描述符。证据见 verified-physical-touch/。

另新增 WeReadPageTouchProbe.java，用于不创建覆盖窗口、向真实微信读书窗口转发一次人工手势。它核对实际 PMS 的 UID 10007 和原签名，起始要求微信读书主窗口聚焦；手势 DOWN 前再次核对窗口包名。原生转发器的 --one-gesture 模式在一次 UP 后立即退出。该路径第一次实测仅确认系统输入分发成功，用户报告文字链接没有跳转或提示，截图仍是未勾选协议的登录页；证据见 weread-page-no-visible-response/。不能将这次分发成功计为应用触控可用。


## 手指事件信息与应用按钮诊断

TouchEvents.java 补全单指事件的 toolType=FINGER、pointer ID、压力、大小和来源；抬手/取消时压力为 0。采用该事件格式的第二轮物理页面测试超时，转发计数为 0；当时未统计原始输入事件，不能据此判断用户没有操作还是事件被协议过滤。证据见 finger-metadata-test-timeout/。

随后使用 LoginTapDiagnostic.java 做一次程序化点击诊断，完整退出码 0。它先核对窗口，再核对整个登录页的灰度像素指纹与已确认的未勾选页面相同，才点击坐标 (360,505) 的“获取二维码”。真实微信读书进程 UID 10007 随后创建了额外的真实应用窗口，截图显示“用户协议与隐私政策”确认窗；没有点击“同意并继续”。程序在 0.5 秒及之后再次取样保存画面，证据见 verified-app-button-diagnostic/。

这证明应用的按钮和弹窗路径可以运行。该诊断是程序化输入，不能替代真实触屏在微信读书页面中的响应验证，也不能据此确定此前文字链接无响应的原因。原始 APK 未修改，仍然离线。KOBO_WEREAD_DIAGNOSTIC_TAP=1 仅允许单独使用已有安装，不与物理输入、显示或返回键测试混用。

触控/点击诊断模式等待真实主 Activity 显示后再进行定向检查，最长等待 60 秒；不再重复此前已经通过的 75 秒观察和返回重开套件。普通验证模式仍保留原有完整流程。


## 真实微信读书页面的物理触摸跳转（已通过一次）

最新一轮捕获一次落点 (379,707) 的人工手势，打开了微信读书自己的“用户协议”页面。原生输入共 25 条，转发 3 条 MotionEvent，无多指或阻挡帧；最终截图显示真实协议页标题及底部导航，正文仍为空。最终画面与墨水屏桥接输出的灰度指纹匹配，原画面和 KOReader 恢复。证据见 verified-weread-physical-navigation/。

这确认了原始 APK 页面上的一次真实触控导航；没有确认协议、获取有效二维码或登录。网络与 DNS、正文加载、登录、阅读和日常交互仍需完成。此前失败原因不能仅据此归结为手指元数据。
