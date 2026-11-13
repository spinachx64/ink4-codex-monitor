// Included in lv_adapter_display.cc, after its shared touch snapshot helpers.
void LVAdapterDisplay::SetCodexInputIdle(bool idle) {
    // Called on the LVGL thread. Snapshot and timer changes serialize with
    // KickTouchInput(), so a new down/up edge cannot be lost while parking.
    s_codex_input_idle = idle;
    if (!touch_indev_) return;
    auto* timer = lv_indev_get_read_timer(touch_indev_);
    if (!timer) return;
    portENTER_CRITICAL(&s_touch_snap_mux);
    const bool busy = s_touch_snap.live_count || s_touch_snap.lvgl_down ||
                      s_touch_snap.pending_down || s_touch_snap.pending_up;
    portEXIT_CRITICAL(&s_touch_snap_mux);
    if (idle && !busy && s_touch_irq_ready) lv_timer_pause(timer);
    else if (lv_timer_get_paused(timer)) {
        lv_timer_resume(timer);
        lv_timer_ready(timer);
    }
}

void LVAdapterDisplay::KickTouchInput() {
    // Feed task never holds the snapshot spinlock here. Serialize timer access
    // with LVGL; its lock is recursive for queued down/up delivery callbacks.
    if (esp_lv_adapter_lock(-1) != ESP_OK) return;
    if (touch_indev_ != nullptr) {
        esp_lv_adapter_touch_notify_interrupt(touch_indev_);
        auto* timer = lv_indev_get_read_timer(touch_indev_);
        if (timer) {
            lv_timer_resume(timer);
            lv_timer_ready(timer);
        }
    }
    esp_lv_adapter_unlock();
    esp_lv_adapter_request_wake();
}
