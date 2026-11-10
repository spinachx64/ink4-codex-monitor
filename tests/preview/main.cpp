#include "../../firmware/codex_monitor/view/quota_view.h"
#include <lvgl.h>
#include <cassert>
#include <cstdio>
#include <vector>
#include <string>
LV_IMAGE_DECLARE(codex_app_icon);
extern "C" {
volatile int epd_i1_glyph_aa_enabled=0;
void epd_gray_aa_plot_logical(int32_t,int32_t,uint8_t) {}
}
unsigned flushes=0;
void flush(lv_display_t* d,const lv_area_t*,uint8_t*) {++flushes;lv_display_flush_ready(d);}
void save(const char* name) {
    lv_obj_update_layout(lv_screen_active());
    auto* image=lv_snapshot_take(lv_screen_active(),LV_COLOR_FORMAT_RGB888);assert(image);
    FILE* f=fopen(name,"wb");assert(f);fprintf(f,"P6\n%u %u\n255\n",image->header.w,image->header.h);
    for(unsigned y=0;y<image->header.h;++y) for(unsigned x=0;x<image->header.w;++x) {
        auto* row=image->data+y*image->header.stride;
        uint8_t rgb[]={row[x*3+2],row[x*3+1],row[x*3]};fwrite(rgb,1,3,f);
    }
    fclose(f);lv_draw_buf_destroy(image);
}
int main(int argc, char**) {
    lv_init();auto* display=lv_display_create(480,800);
    if(argc>1) lv_display_set_color_format(display,LV_COLOR_FORMAT_I1);
    std::vector<uint8_t> buffer(480*800*2);
    lv_display_set_buffers(display,buffer.data(),nullptr,buffer.size(),LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(display,flush);
    codex_ui::QuotaView face;face.create(lv_screen_active());
    quota::Store store;constexpr int64_t now=1000000;
    for(auto& w:store.windows) {w.hasUsage=true;w.source=quota::Source::Api;w.updatedUs=now;}
    store.windows[0].used=26;store.windows[1].used=62;
    store.windows[0].clock.anchor(1000000,1000000000-134*60000,now);
    store.windows[1].clock.anchor(1000000,1000000000-(3*24+16)*3600000LL,now);
    face.update(store,true,now,now);save("docs/ui/layout-a.png.ppm");
    for(int i=0;i<3;++i){lv_tick_inc(40);lv_timer_handler();}flushes=0;
    for(int i=0;i<5;++i){face.update(store,true,now,now);lv_tick_inc(40);lv_timer_handler();}
    fprintf(stderr,"idle flushes=%u\n",flushes);
    assert(flushes==0); // Polling unchanged data must not refresh the e-paper.
    store.windows[0].used=0;store.windows[1].used=100;
    face.update(store,true,now,now);save("docs/ui/full-empty.png.ppm");
    face.update(store,false,now+1000000000,now);save("docs/ui/offline-stale.png.ppm");
    store.windows[0]=quota::Window{};
    face.update(store,true,now,now);save("docs/ui/partial-unknown.png.ppm");
    store.windows[0].source=quota::Source::Estimated;
    store.windows[0].clock.anchor(1000000,1000000000-18000000,now);
    face.update(store,true,now,now);save("docs/ui/estimated.png.ppm");
    store.windows[0].hasUsage=true;store.windows[0].used=50;
    store.windows[0].source=quota::Source::Api;
    store.windows[0].clock.anchor(1000000,1000000000,now);
    face.update(store,true,now,now);save("docs/ui/reset-pending.png.ppm");
    store=quota::Store{};face.update(store,false,now,0);save("docs/ui/no-data.png.ppm");
    face.update(store,false,now,0,false,true);save("docs/ui/ble-error.png.ppm");
    auto* screen=lv_obj_create(nullptr);lv_screen_load(screen);
    auto* black=lv_obj_create(screen);lv_obj_set_size(black,220,130);
    lv_obj_set_style_bg_color(black,lv_color_black(),0);lv_obj_set_style_bg_opa(black,LV_OPA_COVER,0);
    auto* icon=lv_image_create(black);lv_image_set_src(icon,&codex_app_icon);
    lv_obj_set_style_image_recolor(icon,lv_color_white(),0);
    lv_obj_set_style_image_recolor_opa(icon,LV_OPA_COVER,0);
    lv_obj_center(icon);save("docs/ui/icon-inverted.png.ppm");
    printf("Snapshots and zero-idle-redraw check passed.\n");
}
