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
#include <cstring>

namespace {
quota::Store store;
int64_t lastPacketUs=0, refreshRequestedUs=0;
struct View { codex_ui::QuotaView face; lv_timer_t* timer=nullptr; };
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
    if(ble_nus::poll_line(line,sizeof(line),&received) &&
       store.apply(line,strlen(line),received,confirmed)) {
        lastPacketUs=received;refreshRequestedUs=0;
        ESP_LOGI("CodexMonitor","Quota packet applied (5h=%s weekly=%s)",
                 store.windows[0].hasUsage?"known":"unknown",
                 store.windows[1].hasUsage?"known":"unknown");
    }
    const auto now=esp_timer_get_time();
    v->face.update(store,ble_nus::connected(),now,lastPacketUs,
                   refreshRequestedUs && now-refreshRequestedUs<5000000,ble_nus::failed());
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
    PowerPolicy::GetInstance().Acquire(PowerNeed::StandbyInhibit);
    ble_nus::start("Codex Ink");
    auto* screen=lv_obj_create(nullptr);
    auto* v=new View{};v->face.create(screen);
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
        PowerPolicy::GetInstance().Release(PowerNeed::StandbyInhibit);
        ESP_LOGI("CodexMonitor","Quota app closed");
    },LV_EVENT_DELETE,v);
    return screen;
}
