# Ink4 Codex Monitor

## 项目用途

将 Codex 的 **5 小时额度和周额度**显示在墨水屏上，方便随时查看剩余用量和重置倒计时。

电脑上的 Python Bridge 读取已登录 Codex 账号的额度，通过蓝牙发送给设备。设备以大圆环展示 5h 剩余额度，以横条展示周剩余额度；未知或过期数据会明确标识。

- 设备连接并完成通知订阅后，默认每 **5 分钟**查询一次最新额度；断开时暂停后台查询。
- 默认自动激活结束的 5H 周期：执行 `codex exec "Reply with OK only."`，后续通过 `codex exec resume <SESSION_ID> "Reply with OK only."` 复用同一个专用会话，再重新查询额度确认实际重置时间。
- 开机进入设备主页，点击 **Codex** 图标手动启动。
- Codex 页面中，面板按钮 **1 返回主页，2、3 手动同步**。
- 点击左上 CODEX 可返回主页，点击左下 LAST UPDATED 可同步。
- 手动同步会打断网络失败后的退避等待，仍保留至少 5 秒查询间隔；自动重试继续使用指数退避，避免持续失败时频繁请求。
- 每次进入 Codex 页面自动触发一次同步，相当于点击 LAST UPDATED；未连接时保留请求，蓝牙连接并完成通知订阅后立即发送。
- 保留官方应用，主页支持分页；退出 Codex 后停止蓝牙广播和连接。
- 额度查询完成或状态变化时发送数据，不再每分钟重发缓存。重连先显示缓存（API 缓存标记为 stale），新查询成功后恢复正常状态。
- 固件开启蓝牙 Modem Sleep，并请求省电连接参数；Codex 页面保持常驻显示，空闲时允许自动 Light Sleep，保留 BLE 连接。
- 触摸空闲时等待 GPIO 中断，只在按住期间轮询；等待页只在连接状态变化时刷新。额度及倒计时主要随新数据更新，过期或重置状态切换仍会更新。
- 音频蓝牙 UART 接收等待驱动事件，无数据时不再每 100ms 唤醒；Codex 空闲且触摸边沿已交付时暂停 LVGL 输入定时器，触摸到来立即唤醒处理，按住、松开和退出页面时恢复检查。

## 安装说明

### 1. 下载项目并启动电脑后端

需要 macOS、Python **3.11 或更新版本**、已安装的 Codex CLI，以及已登录的 ChatGPT 账号。开启 Mac 蓝牙，并在系统提示时允许蓝牙权限。

```bash
git clone https://github.com/spinachx64/ink4-codex-monitor.git
cd ink4-codex-monitor
python3 -m venv .venv
.venv/bin/python -m pip install -r requirements.txt
codex login
```

先验证额度读取，此命令不连接蓝牙，也不执行推理请求：

```bash
mkdir -p .state
bash scripts/run_bridge.sh --once --provider app-server --state .state/quota-check.json
```

设备刷入固件并打开 Codex 应用后，启动后端：

```bash
bash scripts/run_bridge.sh
```

默认寻找广播名为 `Codex Ink` 的设备。需要指定设备时，使用 `--device YOUR_DEVICE_UUID`；需要指定 Codex CLI 时，使用 `--codex /path/to/codex`。

设备不在时，每次扫描最多 10 秒，重试等待按 3、6、10 秒增长并封顶。重新打开设备，发现等待按约 **20 秒**预留，再加系统连接和通知订阅时间；日志会显示下次重试时间。`--once` 始终直接查询，不依赖设备连接。

设置登录自启动前，先停止前台后端，避免同时运行两个实例：

```bash
bash scripts/setup_autostart.sh
```

日常管理：

```bash
# 查看日志
tail -f ~/Library/Logs/CodexInkMonitor/bridge.log

# 重启后台服务
launchctl kickstart -k gui/$(id -u)/com.spinach.ink4-codex-monitor

# 停止后台服务
launchctl bootout gui/$(id -u)/com.spinach.ink4-codex-monitor
```

状态缓存位于 `~/Library/Application Support/CodexInkMonitor/state.json`。移除登录自启动时，停止服务后将 `~/Library/LaunchAgents/com.spinach.ink4-codex-monitor.plist` 移出该目录。项目路径发生变化后，需要重新运行自启动脚本。

自动激活的会话 ID、触发时间和结果单独保存在同目录的 `state.activation.json`（权限 `0600`）；使用自定义 `--state NAME.json` 时，对应 `NAME.activation.json`。首次触发创建会话，读到 `thread.started` 立即保存 ID，后续即使服务重启或某次探活失败，也只 resume 这个 ID，不使用 `--last`，不自动另建会话。探活在独立的 `activation-workspace` 目录运行，使用只读沙箱。

周期结束判断使用新查询的 API 数据：5H 重置时间已过，或使用量为 0 且原始剩余秒数接近完整 18000 秒（容差 1 秒）。后者是启发式判断，并非服务端提供的明确结束标志；屏幕的 `05h 00m` 按分钟向上取整，不能直接作为触发条件。未知、估算、缓存数据不会触发。每 5 小时至多尝试一次，同一个过期时间只尝试一次；失败也保留触发记录，避免反复消耗额度。新重置时间始终以重新查询的 API 结果为准。

自动激活会产生真实推理请求，可能消耗少量 5H/周额度。使用 `--no-auto-activate` 可关闭；`--once` 始终只读，不触发探活。自动激活沿用当前查询机制，仅在设备 BLE 连接期间检测；断开期间暂停，重连后重新判断。原有 `--enable-probe` 是额度缺失时的一次性探测开关，与周期自动激活独立。

### 2. 构建设备固件

需要 Git、ESP-IDF **5.5.4** 及 ESP32-S3 工具链。先按 ESP-IDF 安装说明准备环境，并在终端激活其 `export.sh`。

下载官方固件基线，准备独立构建副本：

```bash
git clone https://github.com/CloudZao/Metalio-E-INK4.git ../Metalio-E-INK4
git -C ../Metalio-E-INK4 checkout d12f481f4a42420634bb56a928f3959a977367d4
python3 scripts/prepare_firmware.py --source ../Metalio-E-INK4

# 在已激活 ESP-IDF 的 Python 环境中安装资源打包依赖
python -m pip install qoi-conv Pillow
bash scripts/build.sh
```

`prepare_firmware.py` 创建独立副本，不覆盖已有目录，也不修改官方源码。需要重新准备时可用 `--output` 指定新目录。

**升级已有构建副本：** 更新项目后直接执行 `bash scripts/build.sh`。构建前会从 `firmware/codex_monitor/` 更新生成的应用文件，应用受管配置及电源、触摸集成补丁；应用目录中的本地修改会被同名源文件覆盖。集成补丁验证官方代码锚点，遇到不兼容版本会报错，避免静默漏掉省电改动。依赖下载后还会为按键定时器设置浅睡跳过属性。

自定义构建目录可手动同步后构建：

```bash
python3 scripts/update_firmware.py --output /path/to/build-copy
idf.py -C /path/to/build-copy reconfigure
python3 scripts/update_firmware.py --output /path/to/build-copy
idf.py -C /path/to/build-copy build
```

Bridge 更新后需要重启正在运行的进程或后台服务；设备端省电配置需要重新构建、刷入固件才生效。Bridge 与旧固件继续使用同一 v1 协议。

### 功耗诊断

仅 Codex 页面处于 AppIdle、无 USB 虚拟磁盘或其他禁止待机需求且至少 5 秒无操作时，策略允许自动 Light Sleep，CPU 动态范围为 40–80MHz；首次进入仍遵守官方断网暂留时间（默认 30 秒）。触摸、退出页面、音频或 OTA 等需求会恢复普通电源策略。保留主晶振供 BLE 在浅睡期间计时，不使用不稳定的内部 RC 时钟维持连接，也不执行 Deep Sleep 或周期断连。

Codex 页面打开 60 秒后首次输出，之后每 5 分钟输出 `CodexPower` 日志：BQ27220 电流（负值代表放电）、电压、电源档位、实际 `esp_pm_get_configuration()`、BLE 间隔 / latency / timeout，以及 PM 锁与累计浅睡时间。固件关闭 `CONFIG_NEWLIB_NANO_FORMAT`，确保 SDK 功耗统计的 64 位格式正常工作。诊断不写 Flash、不刷新屏幕，关闭页面即停止。Codex 空闲时系统监控降为每 60 秒，减少 SD 日志和电池采样；扩展器按键无变化且释放时延长缓存到 1 秒，中断脚拉低或按住时恢复 20ms 读取。电量百分比来自官方电压估算，优化效果需看放电电流和实际续航。

验证时确认最终 `sdkconfig` 中 `CONFIG_PM_ENABLE`、`CONFIG_FREERTOS_USE_TICKLESS_IDLE`、`CONFIG_PM_PROFILING`、`CONFIG_BT_CTRL_MODEM_SLEEP` 和 `CONFIG_BT_CTRL_MAIN_XTAL_PU_DURING_LIGHT_SLEEP` 均为 `y`。日志应显示 `auto_ls=1`、40–80MHz，并且 PM 统计中的 Light Sleep 累计时间增加；若没有增加，检查活跃的 `NO_LIGHT_SLEEP` 锁。USB 调试连接可能持有 `usb_serial_jtag` 锁，电流对比应断开 USB 并用电池供电或功耗仪测量，同时验证 BLE 每 5 分钟更新和按键、触摸唤醒正常。

### 3. 备份并刷写设备

使用支持数据传输的 USB 线连接设备，关闭串口监视器。将下方串口占位符替换为实际端口；macOS 可用 `ls /dev/cu.*` 查看。设备需要保持唤醒，虚拟 USB 磁盘应关闭。

刷写会替换当前固件，先在已激活的 ESP-IDF 环境中备份整片 16MB Flash：

```bash
mkdir -p backups
python -m esptool --chip esp32s3 --port /dev/cu.usbmodemXXXX \
  --baud 921600 read_flash 0 0x1000000 backups/original.bin
shasum -a 256 backups/original.bin > backups/original.bin.sha256
```

编译通过后刷入固件及配套资源：

```bash
cd build-firmware
idf.py -p /dev/cu.usbmodemXXXX flash
```

重启后点击主页 Codex 图标，再运行电脑后端。首次连接需要等待蓝牙发现和同步；再次进入会先展示缓存数据。

## 适配设备

| 部分 | 支持情况 |
| --- | --- |
| 墨水屏设备 | **Metalio E-Ink 4**，3.97 英寸、480×800 竖屏、SSD1677、ESP32-S3、16MB Flash；已真机验证 |
| 面板操作 | 三枚面板按键及触屏；保留官方主页与其他应用 |
| 电脑后端 | **macOS / Apple Silicon** 已验证，使用 BLE 连接设备 |
| 其他系统或设备 | macOS Intel、Linux、Windows 及其他墨水屏设备尚未适配验证；后端当前包含 macOS 路径、LaunchAgent 与文件锁实现 |

仅使用 API Key 登录时，不应期待获得 ChatGPT 套餐的 5h 和周额度。设备不保存账号令牌；账号登录与额度查询均在电脑端完成。

#### 充电管理的空闲修复

充电管理仅在确认有效输入电源（VBUS 1–5）、非 OTG、非 HIZ 且充电使能时调整终止或尝试再充电。电池供电、读失败及手动关闭充电时均跳过这些操作；保留无输入欠压保护，接近欠压时每 2 秒检查，其他无输入情况每 10 秒检查。真实再充电尝试至少间隔 60 秒，并保留电流的正负号。200ms 再充电脉冲期间，用户的禁充或 OTG 命令优先；中途拔线会恢复原充电使能，便于下次插线充电。

外置蓝牙音频模块、屏幕及共享电源轨本轮不主动断电；其独立控制协议和供电依赖仍需确认，避免影响触摸和官方应用。优化收益以电池供电测量为准。
