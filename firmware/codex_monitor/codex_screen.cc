#include "codex_screen.h"
#include "ble/ble_nus.h"
#include "model/agy_state.h"
#include "model/quota_state.h"
#include "view/agy_view.h"
#include "view/quota_view.h"
#include "screen_common.h"
#include "lv_adapter_display.h"
#include "power_policy.h"
#include "vk_key_handler.h"
#include <esp_timer.h>
#include <esp_log.h>
#include <esp_pm.h>
#include "bq27220_gauge.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
enum class Tab { Codex, Agy };
quota::Store store;
quota::AgyStore agyStore;
int64_t lastPacketUs = 0, lastAgyPacketUs = 0, refreshRequestedUs = 0;

struct View {
    lv_obj_t* codexBox = nullptr;
    lv_obj_t* agyBox = nullptr;
    codex_ui::QuotaView face;
    codex_ui::AgyView agyFace;
    lv_timer_t* timer = nullptr;
    Tab tab = Tab::Codex, renderedTab = Tab::Codex;
    bool rendered = false, connected = false, refreshing = false, error = false;
    quota::Status status[2]{};
    quota::Status agyStatus[4]{};
    int64_t nextDiagnosticUs = 0;
};

View* g_activeView = nullptr;

void update(lv_timer_t* timer);

void power_diagnostic(int64_t now) {
    char policy[192] = "unavailable", ble[128];
    PowerPolicy::GetInstance().DebugFormatCodexPower(policy, sizeof(policy));
    ble_nus::debug_format(ble, sizeof(ble));
    esp_pm_config_t pm{};
    const auto pm_rc = esp_pm_get_configuration(&pm);
    int16_t ma = 0; uint16_t mv = 0;
    auto& gauge = Bq27220Gauge::GetInstance();
    const bool current_ok = gauge.ReadCurrentMa(ma), voltage_ok = gauge.ReadVoltageMv(mv);
    ESP_LOGI("CodexPower", "uptime_s=%u current=%s%d mA (negative=discharge) voltage=%s%u mV %s",
             static_cast<unsigned>(now / 1000000), current_ok ? "" : "unavailable/", ma,
             voltage_ok ? "" : "unavailable/", mv, policy);
    ESP_LOGI("CodexPower", "pm=%s min=%d max=%d auto_ls=%d %s",
             esp_err_to_name(pm_rc), pm.min_freq_mhz, pm.max_freq_mhz, pm.light_sleep_enable, ble);
    char* dump = nullptr; size_t length = 0;
    if (FILE* stream = open_memstream(&dump, &length)) {
        esp_pm_dump_locks(stream); fclose(stream);
        if (dump) {
            char* cursor = nullptr;
            for (char* line = strtok_r(dump, "\n", &cursor); line; line = strtok_r(nullptr, "\n", &cursor))
                ESP_LOGI("CodexPower", "%s", line);
            free(dump);
        }
    }
}

bool refresh() {
    ble_nus::request_refresh();
    refreshRequestedUs = esp_timer_get_time();
    return true;
}

void apply_toggle_tab(void*) {
    if (!g_activeView) return;
    g_activeView->tab = g_activeView->tab == Tab::Codex ? Tab::Agy : Tab::Codex;
    if (g_activeView->tab == Tab::Codex) {
        lv_obj_add_flag(g_activeView->agyBox, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(g_activeView->codexBox, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(g_activeView->codexBox, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(g_activeView->agyBox, LV_OBJ_FLAG_HIDDEN);
    }
    ESP_LOGI("CodexMonitor", "Switched quota view to %s",
             g_activeView->tab == Tab::Codex ? "CODEX" : "GOOGLE AGY");
    update(g_activeView->timer);
    if (auto* scr = lv_screen_active()) lv_obj_invalidate(scr);
}

bool toggle_tab() {
    // panel_key runs on touch_feed; dispatch LVGL mutations onto the LVGL task.
    if (!ScreenLvAsyncUrgent(apply_toggle_tab, nullptr)) {
        return ScreenLvAsync(apply_toggle_tab, nullptr);
    }
    return true;
}

bool panel_key(const char* key) {
    if (!key) return false;
    ESP_LOGI("CodexMonitor", "panel_key=%s", key);
    if (!strcmp(key, "vk_prev")) return refresh();
    if (!strcmp(key, "vk_next")) return toggle_tab();
    return false;
}

lv_obj_t* make_page_container(lv_obj_t* screen) {
    auto* c = lv_obj_create(screen);
    lv_obj_remove_style_all(c);
    lv_obj_set_pos(c, 0, 0);
    lv_obj_set_size(c, 480, 800);
    lv_obj_remove_flag(c, LV_OBJ_FLAG_SCROLLABLE);
    return c;
}

void update(lv_timer_t* timer) {
    auto* v = static_cast<View*>(lv_timer_get_user_data(timer));
    if (auto* display = LVAdapterDisplay::Instance())
        display->SetCodexInputIdle(PowerPolicy::GetInstance().IsCodexIdle());
    char line[512]; int64_t received; bool confirmed[2];
    bool codexApplied = false, agyApplied = false;
    while (ble_nus::poll_line(line, sizeof(line), &received)) {
        const size_t len = strlen(line);
        if (store.apply(line, len, received, confirmed)) {
            lastPacketUs = received; refreshRequestedUs = 0; codexApplied = true;
            ESP_LOGI("CodexMonitor", "Quota packet applied (5h=%s weekly=%s)",
                     store.windows[0].hasUsage ? "known" : "unknown",
                     store.windows[1].hasUsage ? "known" : "unknown");
        } else if (agyStore.apply(line, len, received, confirmed)) {
            lastAgyPacketUs = received; refreshRequestedUs = 0; agyApplied = true;
            ESP_LOGI("CodexMonitor", "AGY quota packet applied (gem5h=%s gemw=%s ext5h=%s extw=%s)",
                     agyStore.gemini[0].hasUsage ? "known" : "unknown",
                     agyStore.gemini[1].hasUsage ? "known" : "unknown",
                     agyStore.external[0].hasUsage ? "known" : "unknown",
                     agyStore.external[1].hasUsage ? "known" : "unknown");
        }
    }
    const auto now = esp_timer_get_time();
    const bool connected = ble_nus::connected(), error = ble_nus::failed();
    const bool refreshing = refreshRequestedUs && now - refreshRequestedUs < 5000000;
    const bool tabChanged = !v->rendered || v->tab != v->renderedTab;
    const bool commonChanged = tabChanged || connected != v->connected ||
                               error != v->error || refreshing != v->refreshing;

    if (v->tab == Tab::Codex) {
        const auto h = store.windows[0].status(now), w = store.windows[1].status(now);
        if (commonChanged || codexApplied || h != v->status[0] || w != v->status[1]) {
            v->face.update(store, connected, now, lastPacketUs, refreshing, error);
            v->rendered = true; v->renderedTab = Tab::Codex;
            v->connected = connected; v->error = error; v->refreshing = refreshing;
            v->status[0] = h; v->status[1] = w;
        }
    } else {
        const quota::Status cur[4] = {
            agyStore.gemini[0].status(now), agyStore.gemini[1].status(now),
            agyStore.external[0].status(now), agyStore.external[1].status(now),
        };
        const bool statusChanged = cur[0] != v->agyStatus[0] || cur[1] != v->agyStatus[1] ||
                                   cur[2] != v->agyStatus[2] || cur[3] != v->agyStatus[3];
        if (commonChanged || agyApplied || statusChanged) {
            v->agyFace.update(agyStore, connected, now, lastAgyPacketUs, refreshing, error);
            v->rendered = true; v->renderedTab = Tab::Agy;
            v->connected = connected; v->error = error; v->refreshing = refreshing;
            for (int i = 0; i < 4; ++i) v->agyStatus[i] = cur[i];
        }
    }

    if (now >= v->nextDiagnosticUs) {
        power_diagnostic(now); v->nextDiagnosticUs = now + 300000000;
    }
    static int64_t loggedPacket = 0;
    const int64_t latestPacket = std::max(lastPacketUs, lastAgyPacketUs);
    if (latestPacket && loggedPacket != latestPacket) {
        loggedPacket = latestPacket; ESP_LOGI("CodexMonitor", "View update returned");
    }
}
}  // namespace

lv_obj_t* CodexScreen::Create() {
    ESP_LOGI("CodexMonitor", "Opening quota app (layout A + AGY dual-ring)");
    if (auto* display = LVAdapterDisplay::Instance())
        display->BindStatusWidgets(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    PowerPolicy::GetInstance().Acquire(PowerNeed::CodexMonitor);
    ble_nus::start("Codex Ink");
    auto* screen = lv_obj_create(nullptr);
    lv_obj_remove_style_all(screen);
    lv_obj_set_style_bg_color(screen, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    auto* v = new View{};
    v->codexBox = make_page_container(screen);
    v->agyBox = make_page_container(screen);
    v->face.create(v->codexBox);
    v->agyFace.create(v->agyBox);
    lv_obj_add_flag(v->agyBox, LV_OBJ_FLAG_HIDDEN);
    g_activeView = v;

    v->nextDiagnosticUs = esp_timer_get_time() + 60000000;
    for (auto* target : {v->face.refresh_target(), v->agyFace.refresh_target()}) {
        lv_obj_add_flag(target, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(target, [](lv_event_t*) { refresh(); }, LV_EVENT_CLICKED, nullptr);
    }
    for (auto* target : {v->face.home_target(), v->agyFace.home_target()}) {
        lv_obj_add_flag(target, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(target, [](lv_event_t*) { ScreenRequestHome(); }, LV_EVENT_CLICKED, nullptr);
    }

    VkKeyScreenDesc desc{}; desc.factory = Create; desc.on_boot_click = refresh; desc.on_key = panel_key;
    VkKey_AttachScreen(screen, "codex", desc);
    refresh();
    v->timer = lv_timer_create(update, 1000, v); update(v->timer);
    lv_obj_add_event_cb(screen, [](lv_event_t* e) {
        if (auto* display = LVAdapterDisplay::Instance()) display->SetCodexInputIdle(false);
        auto* v = static_cast<View*>(lv_event_get_user_data(e));
        if (g_activeView == v) g_activeView = nullptr;
        lv_timer_delete(v->timer); delete v; ble_nus::pause();
        PowerPolicy::GetInstance().Release(PowerNeed::CodexMonitor);
        ESP_LOGI("CodexMonitor", "Quota app closed");
    }, LV_EVENT_DELETE, v);
    return screen;
}
