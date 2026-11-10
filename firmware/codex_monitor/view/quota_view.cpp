#include "quota_view.h"
#include <cstdio>
#include <cstring>

LV_IMAGE_DECLARE(codex_ring_track);
LV_IMAGE_DECLARE(codex_week_track);
LV_FONT_DECLARE(codex_inter_12);
LV_FONT_DECLARE(codex_inter_14);
LV_FONT_DECLARE(codex_inter_21);
LV_FONT_DECLARE(codex_inter_34);
LV_FONT_DECLARE(codex_inter_42);
LV_FONT_DECLARE(codex_inter_60);
LV_FONT_DECLARE(codex_inter_108);
LV_FONT_DECLARE(codex_inter_90);
namespace codex_ui {
namespace {
const lv_color_t ink = lv_color_black();
const lv_color_t muted = lv_color_hex(0x777777);
lv_obj_t* box(lv_obj_t* p, int x,int y,int w,int h) {
    auto* o=lv_obj_create(p); lv_obj_remove_style_all(o);
    lv_obj_set_pos(o,x,y); lv_obj_set_size(o,w,h);
    lv_obj_remove_flag(o,LV_OBJ_FLAG_SCROLLABLE);
    return o;
}
lv_obj_t* text(lv_obj_t* p,const char* value,int x,int y,const lv_font_t* font,
               lv_color_t color=ink) {
    auto* o=lv_label_create(p); lv_obj_remove_style_all(o);
    lv_obj_set_style_text_font(o,font,0); lv_obj_set_style_text_color(o,color,0);
    lv_label_set_text(o,value); lv_obj_set_pos(o,x,y); return o;
}
void set(lv_obj_t* o,const char* value) {
    if(strcmp(lv_label_get_text(o),value)) lv_label_set_text(o,value);
}
void show(lv_obj_t* o,bool visible) {
    if(visible) lv_obj_remove_flag(o,LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(o,LV_OBJ_FLAG_HIDDEN);
}
void rule(lv_obj_t* root,int y) {
    auto* o=box(root,30,y,420,2);
    lv_obj_set_style_bg_color(o,ink,0); lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0);
}
void right(lv_obj_t* o,int y) {
    lv_obj_set_pos(o,32,y); lv_obj_set_width(o,416);
    lv_obj_set_style_text_align(o,LV_TEXT_ALIGN_RIGHT,0);
}
void timer_text(const quota::Window& w,int64_t now,bool week,char* out,size_t size) {
    if(w.status(now)==quota::Status::ResetPending) { snprintf(out,size,"PENDING"); return; }
    if(!w.clock.valid) { snprintf(out,size,"--"); return; }
    const int minutes=static_cast<int>((w.clock.remaining(now)+59999)/60000);
    if(week && minutes>=1440) snprintf(out,size,"%02dd %02dh",minutes/1440,minutes/60%24);
    else if(minutes>=1440) snprintf(out,size,"%dd %02dh",minutes/1440,minutes/60%24);
    else snprintf(out,size,"%02dh %02dm",minutes/60,minutes%60);
}
const char* window_hint(const quota::Window& w,int64_t now,const char* normal) {
    if(w.status(now)==quota::Status::ResetPending) return "AWAITING RESET CONFIRMATION";
    if(w.source==quota::Source::Estimated) return "ESTIMATED RESET / QUOTA UNKNOWN";
    if(!w.hasUsage) return "QUOTA UNKNOWN";
    if(w.dim(now)) return "CACHED / STALE";
    return normal;
}
}
void QuotaView::create(lv_obj_t* screen) {
    root_=screen;
    lv_obj_set_style_bg_color(screen,lv_color_white(),0);
    lv_obj_set_style_bg_opa(screen,LV_OPA_COVER,0);
    lv_obj_set_style_text_color(screen,ink,0);
    lv_obj_remove_flag(screen,LV_OBJ_FLAG_SCROLLABLE);
    brand_=box(screen,24,14,190,48);
    // Open ring brand mark, matching the standalone launcher icon.
    auto* mark=lv_arc_create(brand_);lv_obj_remove_style_all(mark);
    lv_obj_set_pos(mark,6,10);lv_obj_set_size(mark,25,25);
    lv_arc_set_bg_angles(mark,0,0);lv_arc_set_angles(mark,45,315);
    lv_obj_set_style_arc_width(mark,5,LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(mark,ink,LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(mark,false,LV_PART_INDICATOR);
    lv_obj_remove_flag(mark,LV_OBJ_FLAG_CLICKABLE);
    text(brand_,"CODEX",43,8,&codex_inter_21);
    // Inter 12 uppercase ink spans y=28..35; center the dot on that ink,
    // rather than on the font's line box (which includes descender space).
    dot_=box(screen,354,27,9,9);lv_obj_set_style_radius(dot_,LV_RADIUS_CIRCLE,0);
    lv_obj_set_style_border_color(dot_,ink,0);lv_obj_set_style_border_width(dot_,2,0);
    lv_obj_set_style_bg_color(dot_,ink,0);
    status_=text(screen,"NO DATA",348,27,&codex_inter_12);
    lv_obj_set_width(status_,100);lv_obj_set_style_text_align(status_,LV_TEXT_ALIGN_RIGHT,0);
    lv_obj_set_x(dot_,373);
    text(screen,"QUOTA MONITOR / LOCAL BRIDGE",30,62,&codex_inter_12,muted);
    rule(screen,89);
    text(screen,"5H / SHORT WINDOW",32,121,&codex_inter_14);

    auto* track=lv_image_create(screen);lv_image_set_src(track,&codex_ring_track);
    lv_obj_set_pos(track,95,168);
    ring_=lv_arc_create(screen);lv_obj_remove_style_all(ring_);
    lv_obj_set_pos(ring_,95,168);lv_obj_set_size(ring_,290,290);
    lv_arc_set_rotation(ring_,270);lv_arc_set_bg_angles(ring_,0,360);
    lv_arc_set_range(ring_,0,100);lv_arc_set_value(ring_,0);
    lv_obj_set_style_arc_width(ring_,26,LV_PART_MAIN);
    lv_obj_set_style_arc_color(ring_,lv_color_hex(0xe1e1db),LV_PART_MAIN);
    lv_obj_set_style_arc_opa(ring_,LV_OPA_TRANSP,LV_PART_MAIN);
    lv_obj_set_style_arc_width(ring_,26,LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(ring_,ink,LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(ring_,LV_OPA_COVER,LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(ring_,false,LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(ring_,false,LV_PART_INDICATOR);
    lv_obj_remove_flag(ring_,LV_OBJ_FLAG_CLICKABLE);
    auto* group=box(screen,110,250,260,132);
    lv_obj_set_flex_flow(group,LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(group,LV_FLEX_ALIGN_CENTER,LV_FLEX_ALIGN_START,LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_column(group,5,0);
    five_=text(group,"--",0,0,&codex_inter_108);
    five_pct_=text(group,"%",0,0,&codex_inter_34);
    lv_obj_set_style_translate_y(five_pct_,67,0);
    text(screen,"R E M A I N I N G",181,367,&codex_inter_12);
    text(screen,"NEXT RESET",32,492,&codex_inter_12,muted);
    text(screen,"IN",32,513,&codex_inter_12,muted);
    reset5_=text(screen,"--",255,492,&codex_inter_42);right(reset5_,492);
    hint5_=text(screen,"QUOTA UNKNOWN",32,543,&codex_inter_12,muted);
    rule(screen,561);
    text(screen,"WEEKLY",32,586,&codex_inter_14);
    hintw_=text(screen,"7-DAY WINDOW",32,609,&codex_inter_12,muted);
    weekly_=text(screen,"--",312,575,&codex_inter_60);
    weekly_pct_=text(screen,"%",426,610,&codex_inter_21);
    lv_obj_set_pos(weekly_,200,575);lv_obj_set_width(weekly_,221);
    lv_obj_set_style_text_align(weekly_,LV_TEXT_ALIGN_RIGHT,0);
    auto* weekTrack=lv_image_create(screen);lv_image_set_src(weekTrack,&codex_week_track);
    lv_obj_set_pos(weekTrack,32,648);
    bar_=lv_bar_create(screen);lv_obj_remove_style_all(bar_);
    lv_obj_set_pos(bar_,32,648);lv_obj_set_size(bar_,416,18);
    lv_obj_set_style_bg_color(bar_,lv_color_hex(0xe1e1db),LV_PART_MAIN);
    lv_obj_set_style_bg_opa(bar_,LV_OPA_TRANSP,LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar_,ink,LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(bar_,LV_OPA_COVER,LV_PART_INDICATOR);
    lv_bar_set_value(bar_,0,LV_ANIM_OFF);
    text(screen,"RESET IN",32,681,&codex_inter_12,muted);
    resetw_=text(screen,"--",360,680,&codex_inter_14);right(resetw_,680);
    rule(screen,729);
    footer_left_=box(screen,24,738,215,55);
    text(footer_left_,"LAST UPDATED",6,6,&codex_inter_12,muted);
    updated_=text(footer_left_,"NO SYNC",6,26,&codex_inter_14);
    auto* cap=text(screen,"CONNECTION",0,744,&codex_inter_12,muted);right(cap,744);
    connection_=text(screen,"BLE / OFFLINE",0,764,&codex_inter_14);right(connection_,764);
    // Show the reference's empty state only if neither window has any information.
    alert_=box(screen,30,110,420,605);
    lv_obj_set_style_bg_color(alert_,lv_color_white(),0);
    lv_obj_set_style_bg_opa(alert_,LV_OPA_COVER,0);
    auto* bang=text(alert_,"!",0,110,&codex_inter_108);
    lv_obj_set_width(bang,420);lv_obj_set_style_text_align(bang,LV_TEXT_ALIGN_CENTER,0);
    auto* title=alert_title_=text(alert_,"DATA UNAVAILABLE",0,280,&codex_inter_21);
    lv_obj_set_width(title,420);lv_obj_set_style_text_align(title,LV_TEXT_ALIGN_CENTER,0);
    auto* help=alert_help_=text(alert_,"WAITING FOR QUOTA DATA.\nTAP THE FOOTER TO REFRESH.\nPRESS HOME TO RETURN.",0,333,&codex_inter_14,muted);
    lv_obj_set_width(help,420);lv_obj_set_style_text_align(help,LV_TEXT_ALIGN_CENTER,0);
    lv_obj_set_style_text_line_space(help,10,0);
    progress_=text(alert_,"",0,439,&codex_inter_14,muted);
    lv_obj_set_width(progress_,420);
    lv_obj_set_style_text_align(progress_,LV_TEXT_ALIGN_CENTER,0);
}
void QuotaView::update(const quota::Store& store,bool connected,int64_t now,
                       int64_t lastPacketUs,bool refreshing,bool bleError) {
    const auto& h=store.windows[0];const auto& w=store.windows[1];
    const bool any=h.hasUsage||w.hasUsage||h.clock.valid||w.clock.valid;
    const bool stale=(h.hasUsage&&h.dim(now))||(w.hasUsage&&w.dim(now));
    set(status_,bleError?"BLE ERROR":!any?"NO DATA":!connected?"OFFLINE":stale?"STALE":"SYNCED");

    lv_point_t statusSize;
    lv_text_get_size(&statusSize,lv_label_get_text(status_),&codex_inter_12,0,0,480,LV_TEXT_FLAG_NONE);
    lv_obj_set_x(dot_,448-statusSize.x-16);
    const lv_opa_t dotOpacity=connected&&any?LV_OPA_COVER:LV_OPA_TRANSP;
    if(lv_obj_get_style_bg_opa(dot_,LV_PART_MAIN)!=dotOpacity) lv_obj_set_style_bg_opa(dot_,dotOpacity,0);
    char value[40];
    const bool threeDigits=h.hasUsage && h.remaining()>=99.5f;
    const auto* valueFont=threeDigits?&codex_inter_90:&codex_inter_108;
    if(lv_obj_get_style_text_font(five_,LV_PART_MAIN)!=valueFont)
    {
        lv_obj_set_style_text_font(five_,valueFont,0);
        lv_obj_set_style_translate_y(five_,threeDigits?12:0,0);
    }
    snprintf(value,sizeof(value),h.hasUsage?"%d":"--",static_cast<int>(h.remaining()+0.5f));set(five_,value);
    lv_arc_set_value(ring_,h.hasUsage?static_cast<int>(h.remaining()+0.5f):0);
    snprintf(value,sizeof(value),w.hasUsage?"%d":"--",static_cast<int>(w.remaining()+0.5f));
    set(weekly_,value);
    lv_bar_set_value(bar_,w.hasUsage?static_cast<int>(w.remaining()+0.5f):0,LV_ANIM_OFF);
    timer_text(h,now,false,value,sizeof(value));set(reset5_,value);
    timer_text(w,now,true,value,sizeof(value));set(resetw_,value);
    set(hint5_,window_hint(h,now,""));set(hintw_,window_hint(w,now,"7-DAY WINDOW"));
    const int64_t age=lastPacketUs?std::max<int64_t>(0,now-lastPacketUs)/60000000:0;
    if(refreshing) snprintf(value,sizeof(value),"REFRESHING...");
    else if(!lastPacketUs) snprintf(value,sizeof(value),"NO SYNC");
    else if(age==0) snprintf(value,sizeof(value),"JUST NOW");
    else snprintf(value,sizeof(value),"%dm AGO",static_cast<int>(age));
    set(updated_,value);
    set(connection_,bleError?"BLE / ERROR":connected?"BLE / LOCAL":"BLE / OFFLINE");
    set(alert_title_,bleError?"BLUETOOTH UNAVAILABLE":"DATA UNAVAILABLE");

    set(alert_help_,bleError?"RESTART DEVICE TO RETRY.\nTHE BRIDGE HAS NOT CONNECTED.":
        "WAITING FOR QUOTA DATA.\nTAP THE FOOTER TO REFRESH.\nPRESS HOME TO RETURN.");
    const bool waiting=!any && !bleError;
    if(waiting && !waiting_) waiting_since_us_=now;
    waiting_=waiting;
    if(waiting) {
        // Only this additional line changes, in five-second steps. Connection
        // transitions still show immediately; the existing help stays intact.
        const auto seconds=std::max<int64_t>(0,now-waiting_since_us_)/5000000*5;
        snprintf(value,sizeof(value),"%s / %dS",connected?"SYNCING QUOTA":"CONNECTING BLE",
                 static_cast<int>(std::min<int64_t>(seconds,99999)));
        set(progress_,value);
    }
    show(progress_,waiting);
    show(alert_,!any);
}
}
