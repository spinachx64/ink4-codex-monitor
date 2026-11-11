// Included inside the charger driver's anonymous namespace. Keep all charging
// control and undervoltage handling on its existing internal-RAM worker task.
struct ChargerInput {
    bool valid = false;
    uint8_t vbus = 0;
    uint8_t control = 0;
};

ChargerInput charger_input_locked()
{
    uint8_t status = 0, otg = 0, control = 0;
    const bool ok = read_byte(REG_STATUS1, &status) == ESP_OK &&
        read_byte(REG_CHG_CTRL3, &otg) == ESP_OK &&
        read_byte(REG_CHG_CTRL1, &control) == ESP_OK;
    return {codex_power::charger_input_usable(ok, status & 7, otg & (1 << 6),
                control & (1 << 5), control & (1 << 4)),
            static_cast<uint8_t>(status & 7), control};
}

// All external charge/OTG controls update this under s_lock. A user command
// during the 200ms pulse must take precedence over the worker's restoration.
uint32_t s_charge_command_revision = 0;

bool kick_recharge(int32_t vbat, uint32_t vreg_target)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    const auto input = charger_input_locked(); // Recheck immediately before writes.
    if (!input.valid) {
        xSemaphoreGive(s_lock);
        return false;
    }
    const uint32_t revision = s_charge_command_revision;
    esp_err_t err = update_bits(REG_CHG_CTRL0, 0x01, 2, 0); // EN_TERM off
    if (err == ESP_OK) err = update_bits(REG_CHG_CTRL1, 0x01, 5, 0);
    xSemaphoreGive(s_lock);
    if (err != ESP_OK) return false;

    vTaskDelay(pdMS_TO_TICKS(200));

    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (revision == s_charge_command_revision) {
        // Restore the previous enable bit even if USB was unplugged during
        // the pulse; otherwise the next plug-in could remain unable to charge.
        uint8_t otg = 0;
        err = read_byte(REG_CHG_CTRL3, &otg);
        if (err == ESP_OK && !(otg & (1 << 6)))
            err = update_bits(REG_CHG_CTRL1, 0x01, 5, 1);
    }
    xSemaphoreGive(s_lock);
    ESP_LOGI(TAG, "recharge pulse: vbat=%ldmV target=%lumV restore=%s",
             static_cast<long>(vbat), static_cast<unsigned long>(vreg_target), esp_err_to_name(err));
    return true;
}

void charger_vreg_task(void *arg)
{
    (void)arg;
    int64_t last_attempt_us = 0;
    for (;;) {
        const uint32_t target = s_vreg_vol_mv;
        const int32_t vbat = battery_get_bat_voltage();
        uint8_t vbus = 0;
        const bool have_vbus = cx25601n_get_vbus_stat(&vbus) == ESP_OK;

        // Keep the original no-input undervoltage protection, independent of
        // whether charging is enabled. Failed reads never authorize a shutdown.
        if (!s_uv_shutdown_done && vbat >= 2500 && vbat <= kUvBatfetShutdownMv &&
            have_vbus && vbus == 0) {
            ESP_LOGW(TAG, "UV protect: vbat=%ldmV -> BATFET shutdown", static_cast<long>(vbat));
            if (cx25601n_enter_batfet_shutdown() == ESP_OK) s_uv_shutdown_done = true;
        } else if (s_uv_shutdown_done && vbat > kUvBatfetShutdownMv + 100) {
            s_uv_shutdown_done = false;
        }

        bool input_usable = false;
        if (have_vbus && vbus >= 1 && vbus <= 5 &&
            target >= CX25601N_VREG_MIN_MV && target <= CX25601N_VREG_MAX_MV) {
            xSemaphoreTake(s_lock, portMAX_DELAY);
            input_usable = charger_input_locked().valid;
            xSemaphoreGive(s_lock);
        }
        if (input_usable) {
            apply_vreg_fixed(target);
            // Do not write charger configuration while running on battery,
            // in OTG, after an I2C failure, or with charging manually disabled.
            xSemaphoreTake(s_lock, portMAX_DELAY);
            input_usable = charger_input_locked().valid;
            if (input_usable) set_en_term(vbat >= 2500 && static_cast<uint32_t>(vbat) >= target);
            xSemaphoreGive(s_lock);
            int16_t ma = 0;
            const bool current_ok = Bq27220Gauge::GetInstance().ReadCurrentMa(ma);
            uint8_t stat = 0;
            const bool idle = cx25601n_get_chrg_stat(&stat) == ESP_OK && stat == CX25601N_CHG_STAT_NOT;
            const int64_t now = esp_timer_get_time();
            if (codex_power::should_recharge(input_usable, current_ok, ma, idle, vbat,
                                             target, now, last_attempt_us)) {
                last_attempt_us = now; // Bound retries even if a write fails.
                kick_recharge(vbat, target);
            }
        }
        // Hardware detects plug-in and charges independently. Poll at most
        // every 10s offline, retaining the original 2s cadence near undervoltage.
        const bool near_uv = vbat >= 2500 && vbat <= kUvBatfetShutdownMv + 100;
        vTaskDelay(pdMS_TO_TICKS(input_usable || near_uv ? 2000 : 10000));
    }
}
