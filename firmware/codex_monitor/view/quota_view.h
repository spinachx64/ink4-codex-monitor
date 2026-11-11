#pragma once
#include "lvgl.h"
#include "../model/quota_state.h"
namespace codex_ui {
// Production view shared by firmware and the native screenshot/idle-redraw checks.
class QuotaView {
public:
    void create(lv_obj_t* screen);
    void update(const quota::Store& store, bool connected, int64_t nowUs,
                int64_t lastPacketUs, bool refreshing = false, bool bleError = false);
    lv_obj_t* refresh_target() const { return footer_left_; }
    lv_obj_t* home_target() const { return brand_; }
private:
    lv_obj_t *root_, *brand_, *ring_, *five_, *five_pct_, *weekly_, *weekly_pct_, *bar_,
             *reset5_, *resetw_, *status_, *dot_, *updated_, *connection_,
             *footer_left_, *hint5_, *hintw_, *alert_, *alert_title_, *alert_help_, *progress_;
};
}
