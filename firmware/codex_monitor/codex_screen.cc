#include "codex_screen.h"
#include "ble/ble_nus.h"
#include "model/quota_state.h"
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
quota::Store store;
int64_t lastPacketUs=0, refreshRequestedUs=0;
struct View {
    codex_ui::QuotaView face;
    lv_timer_t* timer=nullptr;
    bool rendered=false, connected=false, refreshing=false, error=false;
    quota::Status status[2]{};
    int64_t nextDiagnosticUs=0;
};
void power_diagnostic(int64_t now) {
    char policy[192]="unavailable", ble[128];
    PowerPolicy::GetInstance().DebugFormatCodexPower(policy,sizeof(policy));
    ble_nus::debug_format(ble,sizeof(ble));
    esp_pm_config_t pm{};
    const auto pm_rc=esp_pm_get_configuration(&pm);
    int16_t ma=0;uint16_t mv=0;
    auto& gauge=Bq27220Gauge::GetInstance();
    const bool current_ok=gauge.ReadCurrentMa(ma), voltage_ok=gauge.ReadVoltageMv(mv);
    ESP_LOGI("CodexPower","uptime_s=%u current=%s%d mA (negative=discharge) voltage=%s%u mV %s",
             static_cast<unsigned>(now/1000000),current_ok?"":"unavailable/",ma,
             voltage_ok?"":"unavailable/",mv,policy);
    ESP_LOGI("CodexPower","pm=%s min=%d max=%d auto_ls=%d %s",
             esp_err_to_name(pm_rc),pm.min_freq_mhz,pm.max_freq_mhz,pm.light_sleep_enable,ble);
    // Includes active NO_LIGHT_SLEEP locks and accumulated light-sleep time.
    // Once a minute, only while this page is open; no flash writes or UI updates.
    char* dump=nullptr;size_t length=0;
    if(FILE* stream=open_memstream(&dump,&length)) {
        esp_pm_dump_locks(stream);fclose(stream);
        if(dump) {
            char* cursor=nullptr;
            for(char* line=strtok_r(dump,"\n",&cursor);line;line=strtok_r(nullptr,"\n",&cursor))
                ESP_LOGI("CodexPower","%s",line);
            free(dump);
        }
    }
}
bool refresh() {
    ble_nus::request_refresh(); refreshRequestedUs=esp_timer_get_time(); return true;
}
bool panel_key(const char* key) {
    if(key && (!strcmp(key,"vk_prev") || !strcmp(key,"vk_next"))) return refresh();
    return false;
}
void update(lv_timer_t* timer) {
    auto* v=static_cast<View*>(lv_timer_get_user_data(timer));
    char line[256];int64_t received;bool confirmed[2];
    const bool applied=ble_nus::poll_line(line,sizeof(line),&received) &&
       store.apply(line,strlen(line),received,confirmed);
    if(applied) {
        lastPacketUs=received;refreshRequestedUs=0;
        ESP_LOGI("CodexMonitor","Quota packet applied (5h=%s weekly=%s)",
                 store.windows[0].hasUsage?"known":"unknown",
                 store.windows[1].hasUsage?"known":"unknown");
    }
    const auto now=esp_timer_get_time();
    const bool connected=ble_nus::connected(), error=ble_nus::failed();
    const bool refreshing=refreshRequestedUs && now-refreshRequestedUs<5000000;
    const auto h=store.windows[0].status(now), w=store.windows[1].status(now);
    if(!v->rendered || applied || connected!=v->connected || error!=v->error ||
       refreshing!=v->refreshing || h!=v->status[0] || w!=v->status[1]) {
        v->face.update(store,connected,now,lastPacketUs,refreshing,error);
        v->rendered=true;v->connected=connected;v->error=error;v->refreshing=refreshing;
        v->status[0]=h;v->status[1]=w;
    }
    if(now>=v->nextDiagnosticUs) {
        power_diagnostic(now);v->nextDiagnosticUs=now+60000000;
    }
    static int64_t loggedPacket=0;
    if(lastPacketUs && loggedPacket!=lastPacketUs) {
        loggedPacket=lastPacketUs;ESP_LOGI("CodexMonitor","View update returned");
    }
}
}
lv_obj_t* CodexScreen::Create() {
    ESP_LOGI("CodexMonitor","Opening quota app (layout A)");
    // This page has its own header. Detach the previous page's system widgets
    // before navigation deletes them; background status updates otherwise use
    // freed LVGL objects. HomeScreen will bind its fresh widgets on return.
    if(auto* display=LVAdapterDisplay::Instance())
        display->BindStatusWidgets(nullptr,nullptr,nullptr,nullptr,nullptr,nullptr,nullptr);
    PowerPolicy::GetInstance().Acquire(PowerNeed::CodexMonitor);
    ble_nus::start("Codex Ink");
    auto* screen=lv_obj_create(nullptr);
    auto* v=new View{};v->face.create(screen);
    v->nextDiagnosticUs=esp_timer_get_time()+60000000; // Render the page before diagnostic I/O.
    lv_obj_add_flag(v->face.refresh_target(),LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(v->face.refresh_target(),[](lv_event_t*){refresh();},LV_EVENT_CLICKED,nullptr);
    lv_obj_add_flag(v->face.home_target(),LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(v->face.home_target(),[](lv_event_t*){ScreenRequestHome();},LV_EVENT_CLICKED,nullptr);
    VkKeyScreenDesc desc{};desc.factory=Create;desc.on_boot_click=refresh;desc.on_key=panel_key;
    VkKey_AttachScreen(screen,"codex",desc);
    refresh();  // Opening the page is equivalent to tapping LAST UPDATED once.
    v->timer=lv_timer_create(update,1000,v);update(v->timer);
    lv_obj_add_event_cb(screen,[](lv_event_t* e){
        auto* v=static_cast<View*>(lv_event_get_user_data(e));
        lv_timer_delete(v->timer);delete v;ble_nus::pause();
        PowerPolicy::GetInstance().Release(PowerNeed::CodexMonitor);
        ESP_LOGI("CodexMonitor","Quota app closed");
    },LV_EVENT_DELETE,v);
    return screen;
}
