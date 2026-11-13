"""Integrate verified charger and Codex idle background-work fixes."""
import hashlib


def integrate_background_power(output, edit):
    # request_wake previously notified only during the adapter's own pause-mode
    # sleep. Codex parks the input timer while leaving the worker running;
    # explicitly kick that worker too, rather than waiting for its 1s timeout.
    rel = 'managed_components/espressif__esp_lvgl_adapter/src/adapter/esp_lv_adapter.c'
    if (output / rel).exists():
        edit(rel, '''esp_err_t esp_lv_adapter_request_wake(void)
{
    ESP_RETURN_ON_FALSE(s_ctx.inited, ESP_ERR_INVALID_STATE, TAG, "Adapter not initialized");

    adapter_auto_sleep_mark_activity_internal();
    if (adapter_auto_sleep_should_latch_wake_request()) {
        s_ctx.auto_sleep.wake_requested = true;
        adapter_auto_sleep_notify_task();
    }

    return ESP_OK;
}''', '''esp_err_t esp_lv_adapter_request_wake(void)
{
    ESP_RETURN_ON_FALSE(s_ctx.inited, ESP_ERR_INVALID_STATE, TAG, "Adapter not initialized");

    adapter_auto_sleep_mark_activity_internal();
    if (adapter_auto_sleep_should_latch_wake_request()) {
        s_ctx.auto_sleep.wake_requested = true;
    }
    adapter_auto_sleep_notify_task(); // Also wake Codex's parked-input worker.
    return ESP_OK;
}''')

    rel = 'main/boards/common/audio/SimpleUart.hpp'
    original = (output / rel).read_text()
    marker = '#include "power/uart_receive_task.inc"'
    if marker not in original:
        start = original.index('    // UART接收任务\n')
        end = original.index('    // 常量定义', start)
        old = original[start:end]
        if old.count('pdMS_TO_TICKS(100)') != 1 or old.count('uart_read_bytes(') != 1:
            raise ValueError('unsupported firmware integration anchor: UART receive worker')
        edit(rel, old, '    ' + marker + '\n\n')
    edit(rel, '#include <freertos/task.h>', '#include <freertos/task.h>\n#include <freertos/queue.h>')

    rel = 'main/display/lv_adapter_display.h'
    edit(rel, '    void KickTouchInput();',
         '    void KickTouchInput();\n    void SetCodexInputIdle(bool idle);')
    rel = 'main/display/lv_adapter_display.cc'
    edit(rel, 'bool s_standby_background_paused = false;',
         'bool s_codex_input_idle = false; // Protected by the recursive LVGL lock.\nbool s_standby_background_paused = false;')
    edit(rel, '    if (kick_again) {\n        if (auto* disp = LVAdapterDisplay::Instance()) {',
         '''    if (auto* disp = LVAdapterDisplay::Instance()) {
        disp->SetCodexInputIdle(s_codex_input_idle);
    }
    if (kick_again) {
        if (auto* disp = LVAdapterDisplay::Instance()) {''')
    edit(rel, '''void LVAdapterDisplay::KickTouchInput() {
    if (touch_indev_ != nullptr) {
        esp_lv_adapter_touch_notify_interrupt(touch_indev_);
    }
}''', '#include "power/touch_input_idle.inc"')

    # The region digest rejects unexpected upstream charger changes rather than
    # silently replacing safety-critical logic. The original stays in upstream.
    rel = 'main/boards/common/power/cx25601n.cc'
    path = output / rel
    original = path.read_text()
    marker = '#include "power/charger_vreg_task.inc"'
    if marker not in original:
        start = original.index('/** 低于截止电压且已终止时')
        end = original.index('\nvoid unlock_private(', start)
        old = original[start:end]
        if hashlib.sha256(old.encode()).hexdigest() != CHARGER_REGION_SHA256:
            raise ValueError('unsupported firmware integration anchor: charger VREG worker')
        edit(rel, old, marker + '\n')
    edit(rel, '#include "cx25601n.h"',
         '#include "cx25601n.h"\n#include "power/background_policy.h"\n#include <esp_timer.h>')
    edit(rel, '    xSemaphoreTake(s_lock, portMAX_DELAY);\n    esp_err_t err = ESP_OK;\n    if (enable) {',
         '    xSemaphoreTake(s_lock, portMAX_DELAY);\n    ++s_charge_command_revision;\n    esp_err_t err = ESP_OK;\n    if (enable) {')
    edit(rel, '    xSemaphoreTake(s_lock, portMAX_DELAY);\n    esp_err_t err = ESP_OK;\n\n    if (enable) {',
         '    xSemaphoreTake(s_lock, portMAX_DELAY);\n    ++s_charge_command_revision;\n    esp_err_t err = ESP_OK;\n\n    if (enable) {')
    edit(rel, '/** ibat：battery_get_bat_current()/10，单位约 µA 标度（1mA→1000） */\nint32_t ibat_to_ma(int32_t ibat_scaled)\n{\n    int32_t ma = ibat_scaled / 1000;\n    return ma > 0 ? ma : 0;\n}\n\n', '')

    base = 'main/boards/common/power_policy/'
    edit(base+'power_policy.h', '    int GetIdleCpuMhz() const;',
         '    int GetIdleCpuMhz() const;\n    bool IsCodexIdle() const;')
    edit(base+'power_policy.cc', 'bool PowerPolicy::DebugFormatCodexPower',
         'bool PowerPolicy::IsCodexIdle() const\n{\n    return s_codex_background_idle.load(std::memory_order_relaxed);\n}\n\nbool PowerPolicy::DebugFormatCodexPower')

    rel = 'main/boards/metalio-e-ink-4/periph/metalio_board_periph.cc'
    edit(rel, '#include "metalio_board_periph.h"',
         '#include "metalio_board_periph.h"\n#include "power_policy.h"\n#include "power/background_policy.h"')
    edit(rel, '                vTaskDelay(pdMS_TO_TICKS(3000));',
         '                const unsigned monitor_period = codex_power::monitor_period_ms(\n                    PowerPolicy::GetInstance().IsCodexIdle());\n                vTaskDelay(pdMS_TO_TICKS(monitor_period));')
    edit(rel, '                    if (gap_us > 7500000) {',
         '                    if (gap_us > static_cast<uint64_t>(monitor_period) * 1000 + 4500000) {')

    rel = 'main/boards/common/bus/IOExpander.hpp'
    edit(rel, '#include <cstdint>',
         '#include <cstdint>\n#include <driver/gpio.h>\n#include "config.h"\n#include "power_policy.h"\n#include "power/background_policy.h"')
    edit(rel, '        if (input_cache_valid_ && (now - input_cache_tick_) < kInputCacheTtlTicks) {',
         '''        // Idle cache extends only while both volume keys are released
        // and the shared interrupt is high. Any input transition brings back
        // the original 20ms scan, including release and long-press handling.
        const bool volume_pressed = (input_cache_value_ & ((1U << 7) | (1U << 8))) != ((1U << 7) | (1U << 8));
        const auto ttl = pdMS_TO_TICKS(codex_power::input_cache_ms(
            PowerPolicy::GetInstance().IsCodexIdle(), gpio_get_level(IO_EXPANDER_INT_GPIO) == 0, volume_pressed));
        if (input_cache_valid_ && (now - input_cache_tick_) < ttl) {''')


CHARGER_REGION_SHA256 = '9947f06af14e851e6d3bf42007caa8daef6f88ee7296324918fd20d6a2bcbdfa'
