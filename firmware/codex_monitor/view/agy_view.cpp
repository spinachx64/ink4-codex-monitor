#include "agy_view.h"
#include <cstdio>
#include <cstring>

LV_IMAGE_DECLARE(agy_inner_track);
LV_IMAGE_DECLARE(agy_outer_track);
LV_IMAGE_DECLARE(codex_week_track);
LV_FONT_DECLARE(codex_inter_12);
LV_FONT_DECLARE(codex_inter_14);
LV_FONT_DECLARE(codex_inter_21);
LV_FONT_DECLARE(codex_inter_34);
LV_FONT_DECLARE(codex_inter_42);
LV_FONT_DECLARE(codex_inter_60);
LV_FONT_DECLARE(codex_inter_90);
LV_FONT_DECLARE(codex_inter_108);

namespace codex_ui {
namespace {

const lv_color_t ink = lv_color_black();
const lv_color_t muted = lv_color_hex(0x777777);

lv_obj_t* box(lv_obj_t* p, int x, int y, int w, int h) {
    auto* o = lv_obj_create(p);
    lv_obj_remove_style_all(o);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

lv_obj_t* text(lv_obj_t* p, const char* value, int x, int y, const lv_font_t* font,
               lv_color_t color = ink) {
    auto* o = lv_label_create(p);
    lv_obj_remove_style_all(o);
    lv_obj_set_style_text_font(o, font, 0);
    lv_obj_set_style_text_color(o, color, 0);
    lv_label_set_text(o, value);
    lv_obj_set_pos(o, x, y);
    return o;
}

void set(lv_obj_t* o, const char* value) {
    if (strcmp(lv_label_get_text(o), value)) lv_label_set_text(o, value);
}

void show(lv_obj_t* o, bool visible) {
    if (visible) lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
}

void rule(lv_obj_t* root, int y) {
    auto* o = box(root, 30, y, 420, 2);
    lv_obj_set_style_bg_color(o, ink, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
}

void right(lv_obj_t* o, int y) {
    lv_obj_set_pos(o, 32, y);
    lv_obj_set_width(o, 416);
    lv_obj_set_style_text_align(o, LV_TEXT_ALIGN_RIGHT, 0);
}

void timer_text(const quota::Window& w, int64_t now, bool week, char* out, size_t size) {
    if (w.status(now) == quota::Status::ResetPending) { snprintf(out, size, "PENDING"); return; }
    if (!w.clock.valid) {
        if (w.hasUsage && w.remaining() >= 99.5f) { snprintf(out, size, "AVAILABLE"); return; }
        snprintf(out, size, "--");
        return;
    }
    const int minutes = static_cast<int>((w.clock.remaining(now) + 59999) / 60000);
    if (week && minutes >= 1440) snprintf(out, size, "%02dd %02dh", minutes / 1440, minutes / 60 % 24);
    else if (minutes >= 1440) snprintf(out, size, "%dd %02dh", minutes / 1440, minutes / 60 % 24);
    else snprintf(out, size, "%02dh %02dm", minutes / 60, minutes % 60);
}

const char* window_hint(const quota::Window& w, int64_t now, const char* normal) {
    if (w.status(now) == quota::Status::ResetPending) return "AWAITING RESET CONFIRMATION";
    if (w.source == quota::Source::Estimated) return "ESTIMATED RESET / QUOTA UNKNOWN";
    if (!w.hasUsage) return "QUOTA UNKNOWN";
    if (w.dim(now)) return "CACHED / STALE";
    return normal;
}

lv_obj_t* make_arc(lv_obj_t* parent, int x, int y, int size, int width) {
    auto* a = lv_arc_create(parent);
    lv_obj_remove_style_all(a);
    lv_obj_set_pos(a, x, y);
    lv_obj_set_size(a, size, size);
    lv_arc_set_range(a, 0, 100);
    lv_arc_set_value(a, 0);
    lv_obj_set_style_arc_width(a, width, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(a, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_arc_width(a, width, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(a, ink, LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(a, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(a, false, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(a, false, LV_PART_INDICATOR);
    lv_obj_remove_flag(a, LV_OBJ_FLAG_CLICKABLE);
    return a;
}

}  // namespace

void AgyView::create(lv_obj_t* screen) {
    root_ = screen;
    lv_obj_set_style_bg_color(screen, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(screen, ink, 0);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    brand_ = box(screen, 24, 14, 260, 48);
    // Geometric Google 'G' mark, matching Codex's 25x25 open-ring brand mark.
    auto* mark = lv_arc_create(brand_);
    lv_obj_remove_style_all(mark);
    lv_obj_set_pos(mark, 6, 10);
    lv_obj_set_size(mark, 25, 25);
    lv_arc_set_bg_angles(mark, 0, 0);
    lv_arc_set_angles(mark, 0, 315);
    lv_obj_set_style_arc_width(mark, 5, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(mark, ink, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(mark, false, LV_PART_INDICATOR);
    lv_obj_remove_flag(mark, LV_OBJ_FLAG_CLICKABLE);
    auto* barG = box(brand_, 18, 20, 13, 5);
    lv_obj_set_style_bg_color(barG, ink, 0);
    lv_obj_set_style_bg_opa(barG, LV_OPA_COVER, 0);
    lv_obj_remove_flag(barG, LV_OBJ_FLAG_CLICKABLE);
    text(brand_, "GOOGLE  AGY", 43, 8, &codex_inter_21);

    dot_ = box(screen, 354, 27, 9, 9);
    lv_obj_set_style_radius(dot_, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_color(dot_, ink, 0);
    lv_obj_set_style_border_width(dot_, 2, 0);
    lv_obj_set_style_bg_color(dot_, ink, 0);
    status_ = text(screen, "NO DATA", 348, 27, &codex_inter_12);
    lv_obj_set_width(status_, 100);
    lv_obj_set_style_text_align(status_, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_set_x(dot_, 373);
    text(screen, "QUOTA MONITOR / LOCAL BRIDGE", 30, 62, &codex_inter_12, muted);
    rule(screen, 89);

    text(screen, "5H / SHORT WINDOW", 32, 121, &codex_inter_14);

    // Outer dual-semicircle (Claude/GPT): 300x300 centered at (240, 308), 14px stroke.
    // Left semicircle = Weekly (100..260, bottom->top); Right semicircle = 5H (280..80, reverse bottom->top).
    auto* outerTrack = lv_image_create(screen);
    lv_image_set_src(outerTrack, &agy_outer_track);
    lv_obj_set_pos(outerTrack, 90, 158);

    ext_wk_arc_ = make_arc(screen, 90, 158, 300, 14);
    lv_arc_set_mode(ext_wk_arc_, LV_ARC_MODE_NORMAL);
    lv_arc_set_rotation(ext_wk_arc_, 0);
    lv_arc_set_bg_angles(ext_wk_arc_, 100, 260);

    ext_5h_arc_ = make_arc(screen, 90, 158, 300, 14);
    lv_arc_set_mode(ext_5h_arc_, LV_ARC_MODE_REVERSE);
    lv_arc_set_rotation(ext_5h_arc_, 0);
    lv_arc_set_bg_angles(ext_5h_arc_, 280, 80);

    // Inner full ring (Gemini 5H): 252x252 centered at (240, 308), 24px stroke, 10px white moat.
    auto* innerTrack = lv_image_create(screen);
    lv_image_set_src(innerTrack, &agy_inner_track);
    lv_obj_set_pos(innerTrack, 114, 182);

    ring_ = make_arc(screen, 114, 182, 252, 24);
    lv_arc_set_rotation(ring_, 270);
    lv_arc_set_bg_angles(ring_, 0, 360);

    // Center big Gemini 5H percentage + REMAINING.
    auto* group = box(screen, 136, 246, 208, 108);
    lv_obj_set_flex_flow(group, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(group, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_column(group, 4, 0);
    five_ = text(group, "--", 0, 0, &codex_inter_60);
    lv_obj_set_style_translate_y(five_, 24, 0);
    five_pct_ = text(group, "%", 0, 0, &codex_inter_21);
    lv_obj_set_style_translate_y(five_pct_, 54, 0);
    auto* rem = text(screen, "R E M A I N I N G", 120, 355, &codex_inter_12);
    lv_obj_set_width(rem, 240);
    lv_obj_set_style_text_align(rem, LV_TEXT_ALIGN_CENTER, 0);

    // Single-line bottom corners matching outer semicircles: Left = WK, Right = 5H.
    auto* leftCorner = box(screen, 32, 438, 142, 30);
    lv_obj_set_flex_flow(leftCorner, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(leftCorner, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_set_style_pad_column(leftCorner, 7, 0);
    auto* wkTag = text(leftCorner, "WK", 0, 0, &codex_inter_12, muted);
    lv_obj_set_style_translate_y(wkTag, -3, 0);
    ext_wk_ = text(leftCorner, "--", 0, 0, &codex_inter_21);

    auto* rightCorner = box(screen, 306, 438, 142, 30);
    lv_obj_set_flex_flow(rightCorner, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(rightCorner, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_set_style_pad_column(rightCorner, 7, 0);
    auto* hTag = text(rightCorner, "5H", 0, 0, &codex_inter_12, muted);
    lv_obj_set_style_translate_y(hTag, -3, 0);
    ext_5h_ = text(rightCorner, "--", 0, 0, &codex_inter_21);

    // Lower half: pixel-identical to Codex QuotaView (Gemini 5H reset + Gemini Weekly + Footer).
    text(screen, "NEXT RESET", 32, 492, &codex_inter_12, muted);
    text(screen, "IN", 32, 513, &codex_inter_12, muted);
    reset5_ = text(screen, "--", 255, 492, &codex_inter_42);
    right(reset5_, 492);
    hint5_ = text(screen, "QUOTA UNKNOWN", 32, 543, &codex_inter_12, muted);
    rule(screen, 561);

    text(screen, "WEEKLY", 32, 586, &codex_inter_14);
    hintw_ = text(screen, "7-DAY WINDOW", 32, 609, &codex_inter_12, muted);
    weekly_ = text(screen, "--", 312, 575, &codex_inter_60);
    weekly_pct_ = text(screen, "%", 426, 610, &codex_inter_21);
    lv_obj_set_pos(weekly_, 200, 575);
    lv_obj_set_width(weekly_, 221);
    lv_obj_set_style_text_align(weekly_, LV_TEXT_ALIGN_RIGHT, 0);

    auto* weekTrack = lv_image_create(screen);
    lv_image_set_src(weekTrack, &codex_week_track);
    lv_obj_set_pos(weekTrack, 32, 648);
    bar_ = lv_bar_create(screen);
    lv_obj_remove_style_all(bar_);
    lv_obj_set_pos(bar_, 32, 648);
    lv_obj_set_size(bar_, 416, 18);
    lv_obj_set_style_bg_color(bar_, lv_color_hex(0xe1e1db), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar_, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar_, ink, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(bar_, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_bar_set_value(bar_, 0, LV_ANIM_OFF);

    text(screen, "RESET IN", 32, 681, &codex_inter_12, muted);
    resetw_ = text(screen, "--", 360, 680, &codex_inter_14);
    right(resetw_, 680);
    rule(screen, 729);

    footer_left_ = box(screen, 24, 738, 215, 55);
    text(footer_left_, "LAST UPDATED", 6, 6, &codex_inter_12, muted);
    updated_ = text(footer_left_, "NO SYNC", 6, 26, &codex_inter_14);
    auto* cap = text(screen, "CONNECTION", 0, 744, &codex_inter_12, muted);
    right(cap, 744);
    connection_ = text(screen, "BLE / OFFLINE", 0, 764, &codex_inter_14);
    right(connection_, 764);

    alert_ = box(screen, 30, 110, 420, 605);
    lv_obj_set_style_bg_color(alert_, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(alert_, LV_OPA_COVER, 0);
    auto* bang = text(alert_, "!", 0, 110, &codex_inter_108);
    lv_obj_set_width(bang, 420);
    lv_obj_set_style_text_align(bang, LV_TEXT_ALIGN_CENTER, 0);
    auto* title = alert_title_ = text(alert_, "DATA UNAVAILABLE", 0, 280, &codex_inter_21);
    lv_obj_set_width(title, 420);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    auto* help = alert_help_ = text(alert_,
        "WAITING FOR AGY QUOTA.\nTAP THE FOOTER TO REFRESH.\nPRESS RIGHT KEY FOR CODEX.",
        0, 333, &codex_inter_14, muted);
    lv_obj_set_width(help, 420);
    lv_obj_set_style_text_align(help, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_line_space(help, 10, 0);
    progress_ = text(alert_, "", 0, 439, &codex_inter_14, muted);
    lv_obj_set_width(progress_, 420);
    lv_obj_set_style_text_align(progress_, LV_TEXT_ALIGN_CENTER, 0);
}

void AgyView::update(const quota::AgyStore& store, bool connected, int64_t now,
                     int64_t lastPacketUs, bool refreshing, bool bleError) {
    const auto& gh = store.gemini[0];
    const auto& gw = store.gemini[1];
    const auto& ch = store.external[0];
    const auto& cw = store.external[1];

    const bool any = gh.hasUsage || gw.hasUsage || ch.hasUsage || cw.hasUsage ||
                     gh.clock.valid || gw.clock.valid;
    const bool stale = (gh.hasUsage && gh.dim(now)) || (gw.hasUsage && gw.dim(now)) ||
                       (ch.hasUsage && ch.dim(now)) || (cw.hasUsage && cw.dim(now));
    set(status_, bleError ? "BLE ERROR" : !any ? "NO DATA" : !connected ? "OFFLINE" : stale ? "STALE" : "SYNCED");

    lv_point_t statusSize;
    lv_text_get_size(&statusSize, lv_label_get_text(status_), &codex_inter_12, 0, 0, 480, LV_TEXT_FLAG_NONE);
    lv_obj_set_x(dot_, 448 - statusSize.x - 16);
    const lv_opa_t dotOpacity = connected && any ? LV_OPA_COVER : LV_OPA_TRANSP;
    if (lv_obj_get_style_bg_opa(dot_, LV_PART_MAIN) != dotOpacity)
        lv_obj_set_style_bg_opa(dot_, dotOpacity, 0);

    char value[40];
    // Center Gemini 5H number: use 90px for 1-2 digits (codex_inter_90 is digits-only),
    // and 60px for 3 digits ("100") or unknown ("--").
    const bool useLarge = gh.hasUsage && gh.remaining() < 99.5f;
    const auto* valueFont = useLarge ? &codex_inter_90 : &codex_inter_60;
    const auto* pctFont = useLarge ? &codex_inter_34 : &codex_inter_21;
    if (lv_obj_get_style_text_font(five_, LV_PART_MAIN) != valueFont ||
        lv_obj_get_style_text_font(five_pct_, LV_PART_MAIN) != pctFont) {
        lv_obj_set_style_text_font(five_, valueFont, 0);
        lv_obj_set_style_translate_y(five_, useLarge ? 4 : 24, 0);
        lv_obj_set_style_text_font(five_pct_, pctFont, 0);
        lv_obj_set_style_translate_y(five_pct_, useLarge ? 56 : 54, 0);
    }
    const int ghPct = gh.hasUsage ? static_cast<int>(gh.remaining() + 0.5f) : 0;
    snprintf(value, sizeof(value), gh.hasUsage ? "%d" : "--", ghPct);
    set(five_, value);
    lv_arc_set_value(ring_, ghPct);

    // Outer left semicircle = External Weekly (WK), Outer right semicircle = External 5H (5H).
    const int cwPct = cw.hasUsage ? static_cast<int>(cw.remaining() + 0.5f) : 0;
    const int chPct = ch.hasUsage ? static_cast<int>(ch.remaining() + 0.5f) : 0;
    lv_arc_set_value(ext_wk_arc_, cwPct);
    lv_arc_set_value(ext_5h_arc_, chPct);
    snprintf(value, sizeof(value), cw.hasUsage ? "%d%%" : "--", cwPct);
    set(ext_wk_, value);
    snprintf(value, sizeof(value), ch.hasUsage ? "%d%%" : "--", chPct);
    set(ext_5h_, value);

    // Lower half: Gemini 5H reset + Gemini Weekly.
    timer_text(gh, now, false, value, sizeof(value));
    const auto* resetFont = !strcmp(value, "AVAILABLE") ? &codex_inter_34 : &codex_inter_42;
    if (lv_obj_get_style_text_font(reset5_, LV_PART_MAIN) != resetFont) {
        lv_obj_set_style_text_font(reset5_, resetFont, 0);
        lv_obj_set_y(reset5_, !strcmp(value, "AVAILABLE") ? 498 : 492);
    }
    set(reset5_, value);

    const int gwPct = gw.hasUsage ? static_cast<int>(gw.remaining() + 0.5f) : 0;
    snprintf(value, sizeof(value), gw.hasUsage ? "%d" : "--", gwPct);
    set(weekly_, value);
    lv_bar_set_value(bar_, gwPct, LV_ANIM_OFF);
    timer_text(gw, now, true, value, sizeof(value));
    set(resetw_, value);

    set(hint5_, window_hint(gh, now, ""));
    set(hintw_, window_hint(gw, now, "7-DAY WINDOW"));

    const int64_t age = lastPacketUs ? std::max<int64_t>(0, now - lastPacketUs) / 60000000 : 0;
    if (refreshing) snprintf(value, sizeof(value), "REFRESHING...");
    else if (!lastPacketUs) snprintf(value, sizeof(value), "NO SYNC");
    else if (age == 0) snprintf(value, sizeof(value), "JUST NOW");
    else snprintf(value, sizeof(value), "%dm AGO", static_cast<int>(age));
    set(updated_, value);
    set(connection_, bleError ? "BLE / ERROR" : connected ? "BLE / LOCAL" : "BLE / OFFLINE");
    set(alert_title_, bleError ? "BLUETOOTH UNAVAILABLE" : "DATA UNAVAILABLE");
    set(alert_help_, bleError
        ? "RESTART DEVICE TO RETRY.\nTHE BRIDGE HAS NOT CONNECTED."
        : "WAITING FOR AGY QUOTA.\nTAP THE FOOTER TO REFRESH.\nPRESS RIGHT KEY FOR CODEX.");
    const bool waiting = !any && !bleError;
    if (waiting) {
        set(progress_, connected ? "SYNCING QUOTA" : "CONNECTING BLE");
    }
    show(progress_, waiting);
    show(alert_, !any);
}

}  // namespace codex_ui
