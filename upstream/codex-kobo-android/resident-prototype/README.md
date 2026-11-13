# Libra 2 常驻 Android 启动器

目标：Android 只冷启动一次，此后在 KOReader 与 Android 之间热切换，不再每次重复约 100 秒的冷启动。从 KOReader 菜单进入 Android 启动器，再从启动器打开微信读书；返回 KOReader 时 Android 留在后台，再次进入直接回到离开时的界面。

本目录以“加速版”（设备 `kobo-android-lab/fast/`）为基础，只在会话期间通过绑定挂载替换组件，退出时全部撤销。原镜像、原 APK、其他菜单入口不变。实测数据与问题记录见 `verification.json`，设备上部署文件的摘要见 `device-artifact-sha256.json`。

## 使用

| 操作 | 结果 |
| --- | --- |
| KOReader 菜单 →“Android 启动器（常驻，热启动）” | 首次冷启动约 31 秒出现启动器；Android 在后台时约 1 秒切回 |
| 启动器“打开微信读书” | 每次常驻的首次打开走原有校验启动（约 26–34 秒），之后直接恢复 |
| 按住上翻页键 5 秒后松开 | 回到启动器（1–5 秒松开仍是全刷） |
| 双键长按 2 秒、长按左上角 2 秒、按电源键 | 返回 KOReader，Android 留在后台 |
| 启动器“关闭 Android”或菜单“关闭后台 Android” | 完整关闭，约 5 秒 |

后台时 KOReader 可正常休眠。退出或重启 KOReader、USB 存储、关机、重启前，插件会先完整关闭 Android（这几条路径**尚未实测**）。

## 工作方式

- **启动器**：没有新增应用包，而是升级镜像里原有的实验主页 `com.kobo.lab.smoke/.HomeActivity`（Android HOME）。只有存在常驻标记 `/data/local/tmp/kobo-resident/enabled` 时才显示按钮界面，原实验行为不变。启动器的请求写到自己的数据目录 `files/resident-request`，主机按属主 UID 10000 校验后读取。
- **前台**：显示桥暂停 KOReader 并接管屏幕；输入助手和触控转发接入，焦点服务新增第 4 类（启动器窗口）。
- **后台**：停止触控和显示桥（帧缓冲逐字节恢复，KOReader 继续运行）；Android 所有线程设为 `SCHED_IDLE`，CPU 调速器恢复原值，并执行 `sync`。没有使用 SIGSTOP：冻结期间单调时钟继续走，解冻后系统会把超时误判为应用无响应（ANR）。
- **输入助手常驻**：每次回到前台只重新接上触控转发，无需重启 Java 进程。
- **上键 5 秒回启动器**：通过 `IActivityManager.startActivityAsUser`（不带调用方，与原校验启动方式相同）启动 HOME。HOME 键不可用，因为本环境没有锁屏服务，`PhoneWindowManager` 处理 HOME 时会出现空指针。
- **时长上限**：显示桥和触控每次前台最长 12 小时；网络状态观察器的常驻模式上限为 30 天；DNS 代理改为常驻版，退出后由监督循环自动重启。
- **DNS 回退**：路由器 DNS 连续 3 次返回 `EAI_AGAIN` 后，先通过 UDP 查询公共 DNS（223.5.5.5，其次 119.29.29.29），每 60 秒再试一次路由器，成功一次即切回。结果经数字地址 `getaddrinfo` 转换，回复格式不变。日志记为 `HOST_DNS_FALLBACK_ACTIVE`、`HOST_DNS_PRIMARY_RECOVERED`，以及单条查询上的 `via=fallback:…`。启动时和每次回到前台，都在后台执行一次 `resident-dns-check`。
- **自动关闭**：以下情况会完整关闭 Android，并走原有的清理流程：KOReader 进程消失（连续 3 次检查）、系统进程退出、微信读书无响应、镜像剩余空间低于 64 MiB、内存保护触发，或收到关闭请求。

## 目录

| 路径 | 内容 |
| --- | --- |
| `device/launch-resident.sh` | 菜单启动器。与其他入口共用锁和 `weread-launch.state`，写 `resident-state` |
| `device/resident-ctl.sh` | `status` / `foreground` / `shutdown [秒]`，供插件调用；启动中收到关闭请求时写入标记文件 |
| `device/run-resident-session.sh` | 外层会话：加速版加常驻组件的绑定挂载，逐个核对 SHA-256 |
| `device/run-native-weread-resident.sh` | 内层会话：基于加速版内层，增加常驻模式、DNS 监督与常驻标记 |
| `device/resident-loop.sh` | 前台/后台/关闭状态机 |
| `device/plugin-main-resident.lua` | KOReader 插件：新菜单项、退出钩子（`UIManager.quit`、`PowerOff`、`Reboot`），后台时不阻止休眠 |
| `device/base-fast/` | 推导所用的加速版原文件，便于对比差异 |
| `src/framework/LiveFocusService.java` | 焦点服务增加第 4 类（启动器窗口） |
| `src/entry/` | 输入助手：`resident` 模式、多次连接、第 6 类数据包（打开启动器） |
| `src/launcher/` | 启动器（实验主页）源码与清单 |
| `src/native/` | 触控转发、显示桥、网络观察器、DNS 代理的常驻修改，以及 `resident-sched`、`resident-dns-check` |
| `build.sh` | 构建全部组件 |

## 构建

```sh
KOBO_BUILD_OUT=/path/to/empty-dir KOBO_PROBE_STORE_PASSWORD=… sh build.sh
```

所用工具链与已验证构建相同：`work/zigpy`（zig 0.13）、`work/jdk17`、`work/android-compile`、`work/aapt2`、`work/fbink`。启动器使用实验测试密钥 `work/activity-test-key.jks` 签名，与镜像中已安装的签名一致。头文件引用原型目录 `outputs/kobo-android/…`。

**可复现性**：

- **两个 dex 可逐字节复现**：`install-framework-probe-resident.dex`、`weread-process-entry-resident.dex`。
- **基线已核对**：改动前，Java 基线可逐字节复现已部署的 `install-framework-probe.dex` 和 `weread-process-entry-fast.dex`；`touch-forward`、`host-default-network` 的原版也能从原路径逐字节复现。
- **原生程序不能逐字节复现**：即使用同一份源码，产物也受构建路径影响，且已部署的加速版显示桥本身就无法复现。
- **APK 不能逐字节复现**：签名带时间戳。

因此以 `device-artifact-sha256.json` 记录的设备实际文件为准。本目录源码已确认与当时部署所用的源码完全相同。

## 部署（设备空闲时）

确认 `weread-launch.lock` 不存在，且没有挂载 `/tmp/kobo-android-runtime`。然后将构建产物、各自的 `.sha256` 文件和 `device/` 中的脚本复制到 `kobo-android-lab/resident/`；`run-native-weread-resident.sh`、`resident-loop.sh` 也需要 `.sha256` 文件。缺少任一摘要时，会话会以 105 退出，不做任何改动。

插件替换前，先备份现有的 `wereadlibra2.koplugin/main.lua`（当前备份：`resident/wereadlibra2-main.lua.before-resident`），然后重启 KOReader。

恢复原插件：把备份复制回 `main.lua`，并重启 KOReader。

## 已知问题与待验证

- **未实测**：自动关闭钩子（退出、重启 KOReader，USB 存储，关机，重启）。Libra 2 上 KOReader 关机执行的是 `poweroff -f`，不经过 init 卸载，所以这几条路径必须实测。
- **未测**：后台长时间停留的耗电和内存。
- **DNS 故障原因未查明**：某一时段的两次常驻会话，域名解析全部失败（`EAI_AGAIN`）；同一天的加速版会话正常，之后未做任何修改就自行恢复。
- **回退切换瞬间**：可能多 1 次失败（子进程尚未回收时下一个查询已到达）。
- **后台 ANR 处理较严**：Android 在后台时微信读书一旦无响应，整个 Android 会关闭（沿用原有策略），下次进入就是冷启动。
- **断电风险**：后台常驻期间如果电量耗尽，ext2 镜像得不到正常卸载。每次进入后台前会执行 `sync`。
