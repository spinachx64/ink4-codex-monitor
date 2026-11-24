#pragma once
#include "lvgl.h"
#include "../model/agy_state.h"

namespace codex_ui {

class AgyView {
public:
    void create(lv_obj_t* parent);
    void update(const quota::AgyStore& store, bool connected, int64_t nowUs,
                int64_t lastPacketUs, bool refreshing = false, bool bleError = false);
    lv_obj_t* refresh_target() const { return footer_left_; }
    lv_obj_t* home_target() const { return brand_; }

private:
    lv_obj_t *root_, *brand_, *ring_, *ext_wk_arc_, *ext_5h_arc_,
             *five_, *five_pct_, *ext_wk_, *ext_5h_,
             *weekly_, *weekly_pct_, *bar_,
             *reset5_, *resetw_, *status_, *dot_, *updated_, *connection_,
             *footer_left_, *hint5_, *hintw_, *alert_, *alert_title_, *alert_help_, *progress_;
};

}  // namespace codex_ui
