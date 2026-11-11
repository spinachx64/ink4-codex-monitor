# A 版 · 480 × 800

按根目录 `metalio_codex_ui.html` 的 A 版实现。PNG 由实际生产 `QuotaView` 用 LVGL 原生渲染生成，数值均为模拟数据。

- `layout-a.png`：5h 74%、周 38%，大圆环、独立百分号、重置倒计时和底部同步信息。
- `full-empty.png`：5h 100%、周 0%；三位数缩小为 90px，完整留在圆环内部。
- `partial-unknown.png`：缺失的窗口保留 --，正常窗口继续显示。
- `offline-stale.png`：断线保留读数，显示 OFFLINE 与 STALE 提示。
- `estimated.png` / `reset-pending.png`：估算重置与等待服务端确认，均不自动回满。
- `ble-error.png`：底层初始化失败时显示明确的蓝牙错误，界面仍可返回主页。
- `no-data.png`：完全未知时显示 DATA UNAVAILABLE，不伪装为 0% 或 100%。
- `connecting.png` / `syncing.png`：保留原等待文案，末尾追加蓝牙连接 / 额度同步状态；只在连接状态变化时更新，不再持续刷新等待秒数。
- `codex-icon.png`：透明背景的独立 Codex 图标。
- `icon-inverted.png`：同一个图标在黑色卡片上反白；普通、斜线主页均使用它。

单色适配：圆环和周额度横条的空余区域使用 1-bit 点阵图案，避免浅灰量化为白色后消失。字体使用 SIL OFL 的 Inter 子集，许可证见 `firmware/codex_monitor/fonts/OFL.txt`。实际连接文案为 BLE，不采用设计稿中的演示 Wi-Fi 文案。

操作：点击页眉 CODEX / 盖板 HOME 返回；点击左下 LAST UPDATED / BOOT 单击刷新。无动画；同一读数反复轮询的重绘次数检查为零。

生成工具：`scripts/generate_assets.py` 创建矢量图标对应的 LVGL 位图及点阵底纹；`scripts/generate_fonts.sh` 创建字体子集。`scripts/preview.sh` 渲染截图并运行空闲重绘检查。
