"""Checked, idempotent low-power integration into the isolated firmware copy."""
from pathlib import Path
try:
    from .integrate_background_power import integrate_background_power
except ImportError:
    from integrate_background_power import integrate_background_power


def replace_once(text, old, new, name):
    if new and new in text:
        return text
    if not new and old not in text:
        return text
    if text.count(old) != 1:
        raise ValueError(f'unsupported firmware integration anchor: {name}')
    return text.replace(old, new, 1)


def integrate_power(output: Path):
    patches = {}

    def edit(rel, old, new):
        path = output / rel
        text = patches.get(path, path.read_text())
        patches[path] = replace_once(text, old, new, rel)

    base = 'main/boards/common/power_policy/'
    edit(base+'power_policy.cc', 'static const char* TAG = \"PowerPolicy\";',
         'static const char* TAG = \"PowerPolicy\";\nstatic std::atomic<bool> s_codex_background_idle{false};')
    edit(base+'power_policy.cc', '#include \"power_policy.h\"', '#include \"power_policy.h\"\n#include \"power/idle_profile.h\"')
    edit(base+'power_policy.h', '    kCount\n',
         '    CodexMonitor, // Display stays visible; allow automatic light sleep only while idle.\n    kCount\n')
    edit(base+'power_policy.h', '    bool IsTouchAsleep() const;',
         '    bool IsTouchAsleep() const;\n    bool DebugFormatCodexPower(char* buf, size_t len);')
    edit(base+'power_policy.cc', 'NeedHeld(PowerNeed::StandbyInhibit)) {\n        return false;',
         'NeedHeld(PowerNeed::StandbyInhibit) || NeedHeld(PowerNeed::CodexMonitor)) {\n        return false;')
    edit(base+'power_policy.cc', '    const bool inhibit_before = NeedHeld(PowerNeed::StandbyInhibit);',
         '    const bool inhibit_before = NeedHeld(PowerNeed::StandbyInhibit) || NeedHeld(PowerNeed::CodexMonitor);')
    edit(base+'power_policy.cc', '(inhibit_before && !NeedHeld(PowerNeed::StandbyInhibit))) {',
         '(inhibit_before && !NeedHeld(PowerNeed::StandbyInhibit) && !NeedHeld(PowerNeed::CodexMonitor))) {')
    edit(base+'power_policy.cc', '\n        power_hw_cpu_freq_set(mhz);\n',
         '\n        power_hw_cpu_mode_set(mhz < 0 ? -mhz : mhz, mhz < 0);\n')
    # The second caller has a different indentation; replace_once intentionally
    # verifies a unique full line to reject changes in the upstream implementation.
    edit(base+'power_policy.cc', '                power_hw_cpu_freq_set(mhz);',
         '                power_hw_cpu_mode_set(mhz < 0 ? -mhz : mhz, mhz < 0);')
    # Upgrade previously generated copies of the first idle-sleep profile.
    policy_text = (output / (base+'power_policy.cc')).read_text()
    if 'A negative queue value encodes' in policy_text and 's_codex_background_idle.store(' not in policy_text:
        edit(base+'power_policy.cc', '    const int cpu_request = codex_idle ? -80 :',
             '    s_codex_background_idle.store(codex_idle, std::memory_order_relaxed);\n    const int cpu_request = codex_idle ? -80 :')
    edit(base+'power_policy.cc', '    const Mode prev = mode;', '''    // A negative queue value encodes the Codex-only automatic sleep profile.
    // Queue before the same-mode fast path: activity can change without a mode change.
    const bool codex_idle = codex_power::allow_auto_sleep(
        NeedHeld(PowerNeed::CodexMonitor), next == Mode::AppIdle,
        NeedHeld(PowerNeed::UsbVirtualDisk) || NeedHeld(PowerNeed::StandbyInhibit), InactiveUsLocked());
    s_codex_background_idle.store(codex_idle, std::memory_order_relaxed);
    const int cpu_request = codex_idle ? -80 :
        (next == Mode::AppIdle ? idle_cpu_mhz : CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ);
    QueueCpuMhzLocked(cpu_request);
    const Mode prev = mode;''')
    edit(base+'power_policy.cc',
         '    const int want_mhz = (next == Mode::AppIdle) ? idle_cpu_mhz : CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ;\n', '')
    edit(base+'power_policy.cc', '    QueueCpuMhzLocked(want_mhz);\n', '')
    edit(base+'power_policy.cc', '    pwr::last_user_activity_us = now;\n    pwr::ClearNetGraceLocked();\n    xSemaphoreGive(pwr::mu);',
         '''    pwr::last_user_activity_us = now;
    pwr::ClearNetGraceLocked();
    if (NeedHeld(PowerNeed::CodexMonitor)) pwr::ReevaluateLocked();
    xSemaphoreGive(pwr::mu);
    pwr::FlushCpuFreqOutsideLock();''')
    edit(base+'power_policy.cc', 'bool PowerPolicy::IsTouchAsleep() const', '''bool PowerPolicy::DebugFormatCodexPower(char* buf, size_t len)
{
    if (!pwr::mu || xSemaphoreTake(pwr::mu, pdMS_TO_TICKS(20)) != pdTRUE) return false;
    snprintf(buf, len, "mode=%s cpu_request=%d wifi_paused=%d touch_asleep=%d needs=",
             pwr::ModeName(pwr::mode), pwr::queued_cpu_mhz,
             pwr::wifi_stopped_by_policy, pwr::touch_asleep);
    size_t used = strlen(buf);
    for (int i = 0; i < static_cast<int>(PowerNeed::kCount) && used < len; ++i) {
        if (pwr::need_ref[i]) used += snprintf(buf + used, len - used, "%d:%u,", i, pwr::need_ref[i]);
    }
    xSemaphoreGive(pwr::mu);
    return true;
}

bool PowerPolicy::IsTouchAsleep() const''')
    edit(base+'power_hw.h', 'esp_err_t power_hw_cpu_freq_set(int mhz);',
         'esp_err_t power_hw_cpu_freq_set(int mhz);\n// Automatic sleep is opted into only by the Codex idle policy.\nesp_err_t power_hw_cpu_mode_set(int mhz, bool auto_sleep);')
    edit(base+'power_hw.cc', 'esp_err_t power_hw_cpu_freq_set(int mhz)\n{', '''esp_err_t power_hw_cpu_freq_set(int mhz)
{
    return power_hw_cpu_mode_set(mhz, false);
}

esp_err_t power_hw_cpu_mode_set(int mhz, bool auto_sleep)
{''')
    edit(base+'power_hw.cc', '    esp_pm_config_t cfg = {\n        .max_freq_mhz = mhz,\n        .min_freq_mhz = mhz,\n        .light_sleep_enable = false,\n    };', '''    // GPIO wake sources stay valid across automatic sleeps. Manual standby
    // resets the wake mask itself; re-arm these on every Codex profile entry.
    const gpio_num_t wake_pins[] = {TOUCH_INT_GPIO, BOOT_BUTTON_GPIO,
                                    POWER_BUTTON_GPIO, IO_EXPANDER_INT_GPIO};
    for (auto pin : wake_pins) {
        const esp_err_t rc = auto_sleep ? gpio_wakeup_enable(pin, GPIO_INTR_LOW_LEVEL)
                                       : gpio_wakeup_disable(pin);
        if (rc != ESP_OK) return rc;
    }
    if (auto_sleep) {
        // PM_SLP_DISABLE_GPIO would otherwise float EPD reset/chip-select and
        // remove wake-pin pullups. Retain these pads' ordinary configuration.
        const gpio_num_t retained[] = {EPD_PIN_RST, EPD_PIN_CS, EPD_PIN_DC,
            EPD_PIN_MOSI, EPD_PIN_SCLK, I2C_SDA_PIN, I2C_SCL_PIN,
            TOUCH_INT_GPIO, BOOT_BUTTON_GPIO, POWER_BUTTON_GPIO, IO_EXPANDER_INT_GPIO};
        for (auto pin : retained) {
            const esp_err_t rc = gpio_sleep_sel_dis(pin);
            if (rc != ESP_OK) return rc;
        }
        const esp_err_t rc = esp_sleep_enable_gpio_wakeup();
        if (rc != ESP_OK) return rc;
    }
    esp_pm_config_t cfg = {
        .max_freq_mhz = mhz,
        .min_freq_mhz = auto_sleep ? 40 : mhz,
        .light_sleep_enable = auto_sleep,
    };''')
    edit(base+'power_hw.cc', '    ESP_LOGD(TAG, "cpu %dMHz (no auto_ls): %s", mhz, esp_err_to_name(err));',
         '    ESP_LOGI(TAG, "cpu max=%d min=%d auto_ls=%d: %s", mhz, cfg.min_freq_mhz, auto_sleep, esp_err_to_name(err));')
    edit(base+'power_hw.cc', '    (void)mhz;\n    ESP_LOGW(TAG, "cpu freq skipped',
         '    (void)mhz;\n    (void)auto_sleep;\n    ESP_LOGW(TAG, "cpu freq skipped')

    rel = 'main/display/lv_adapter_display.cc'
    edit(rel, 'esp_lcd_touch_handle_t s_touch_tp = nullptr;', '''esp_lcd_touch_handle_t s_touch_tp = nullptr;
esp_lcd_touch_interrupt_callback_t s_touch_adapter_irq = nullptr;
bool s_touch_irq_ready = false;

void TouchFeedInterrupt(esp_lcd_touch_handle_t tp) {
    // Preserve the adapter's own semaphore/IRQ handling.
    if (s_touch_adapter_irq) s_touch_adapter_irq(tp);
    BaseType_t wake = pdFALSE;
    if (s_touch_feed_task) vTaskNotifyGiveFromISR(s_touch_feed_task, &wake);
    if (wake) portYIELD_FROM_ISR();
}''')
    edit(rel, '    while (s_touch_feed_run) {\n        uint8_t count = 0;', '''    TickType_t wait = portMAX_DELAY;
    while (s_touch_feed_run) {
        // No periodic I2C reads without a touch. Poll only during an active
        // gesture, including a final release sample for virtual-key long presses.
        ulTaskNotifyTake(pdTRUE, wait);
        if (!s_touch_feed_run) break;
        uint8_t count = 0;''')
    edit(rel, '        TouchFeedHandleSample(count, x, y);\n        vTaskDelay(pdMS_TO_TICKS(kTouchFeedPeriodMs));',
         '''        TouchFeedHandleSample(count, x, y);
        wait = (count || !s_touch_irq_ready) ? pdMS_TO_TICKS(kTouchFeedPeriodMs) : portMAX_DELAY;''')
    edit(rel, '        ESP_LOGE(TAG, "touch_feed create failed");\n    }\n}', '''        ESP_LOGE(TAG, "touch_feed create failed");
        return;
    }
    s_touch_adapter_irq = tp->config.interrupt_callback;
    s_touch_irq_ready = esp_lcd_touch_register_interrupt_callback(tp, TouchFeedInterrupt) == ESP_OK;
    if (!s_touch_irq_ready) {
        ESP_LOGE(TAG, "touch IRQ registration failed; falling back to polling");
    }
    xTaskNotifyGive(s_touch_feed_task); // Drain a touch already present at startup.
}''')
    edit(rel, '    s_touch_feed_run = false;\n    for (int i = 0;', '''    s_touch_feed_run = false;
    esp_lcd_touch_register_interrupt_callback(s_touch_tp, s_touch_adapter_irq);
    xTaskNotifyGive(s_touch_feed_task); // Release a task blocked indefinitely on IRQ.
    for (int i = 0;''')
    edit(rel, '    adapter_cfg.task_max_delay_ms = 1000;', '''    adapter_cfg.task_max_delay_ms = 1000;
    adapter_cfg.tick_period_ms = 1000; // Time comes from esp_timer, not a 1ms tick. app integration''')
    edit(rel, '    ESP_ERROR_CHECK(esp_lv_adapter_init(&adapter_cfg));', '''    ESP_ERROR_CHECK(esp_lv_adapter_init(&adapter_cfg));
    lv_tick_set_cb([]() -> uint32_t { return static_cast<uint32_t>(esp_timer_get_time() / 1000); });''')

    # Dependency sources appear after idf.py reconfigure. Sleep may skip the
    # button timer, but physical GPIOs wake the CPU immediately; normal awake
    # debounce/long-press handling retains the original scan period.
    dep = output / 'managed_components/espressif__button/iot_button.c'
    if dep.exists():
        edit('managed_components/espressif__button/iot_button.c',
             '        button_timer.name = "button_timer";',
             '        button_timer.name = "button_timer";\n        button_timer.skip_unhandled_events = true; // Codex automatic idle sleep')
    integrate_background_power(output, edit)
    for path, text in patches.items():
        if path.read_text() != text:
            path.write_text(text)
