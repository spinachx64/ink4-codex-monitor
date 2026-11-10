# Ink4 Codex Monitor

## 项目用途

将 Codex 的 **5 小时额度和周额度**显示在墨水屏上，方便随时查看剩余用量和重置倒计时。

电脑上的 Python Bridge 读取已登录 Codex 账号的额度，通过蓝牙发送给设备。设备以大圆环展示 5h 剩余额度，以横条展示周剩余额度；未知或过期数据会明确标识。

- 默认每 **5 分钟**查询一次最新额度。
- 开机进入设备主页，点击 **Codex** 图标手动启动。
- Codex 页面中，面板按钮 **1 返回主页，2、3 手动同步**。
- 点击左上 CODEX 可返回主页，点击左下 LAST UPDATED 可同步。
- 保留官方应用，主页支持分页；退出 Codex 后停止蓝牙广播和连接。

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

脚本只修改本项目的 `build-firmware/` 副本，保留官方源码目录。已有构建目录不会被覆盖；需要重新准备时可用 `--output` 指定新的目录，并在该目录中执行 `idf.py build`。

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
