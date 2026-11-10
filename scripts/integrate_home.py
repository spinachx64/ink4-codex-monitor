"""Register Codex without replacing existing apps; paginate the official grids."""
from pathlib import Path


def integrate_home(source: Path, output: Path):
    base = Path('main/display/screen/home_screen')
    p = base / 'home_screen.cc'
    s = (source / p).read_text()
    entry = '        {&codex_app_icon, Home_AppNameCodex, CodexScreen::Create},'
    s = '#include "codex_screen.h"\nLV_IMAGE_DECLARE(codex_app_icon);\n' + s
    s = s.replace('    extern const AppEntry kApps[] = {',
                  '    const char* Home_AppNameCodex() { return "Codex"; }\n    extern const AppEntry kApps[] = {\n' + entry, 1)
    s = s.replace('    extern const AppEntry kSlashApps[] = {',
                  '    extern const AppEntry kSlashApps[] = {\n' + entry, 1)
    s = s.replace('if (Home_slash_layout)\n        {\n            return 1;',
                  'if (Home_slash_layout)\n        {\n            return (kSlashAppCount + HOME_SLASH_ROWS * 2 - 1) / (HOME_SLASH_ROWS * 2);', 1)
    # Reserve the bottom area for the page indicator in both layouts.
    s = s.replace('0, -HOME_SLASH_BOTTOM_PAD);',
                  '0, -(HOME_SLASH_BOTTOM_PAD + (pages > 1 ? HOME_PAGE_INDICATOR_H : 0)));', 1)
    s = s.replace('        Home_page_lbl = nullptr;\n        Home_FillAppsPage();', '''        Home_page_lbl = lv_label_create(scr);
        lv_obj_set_style_text_font(Home_page_lbl, fontpack_lv_font_ui(), 0);
        lv_obj_align(Home_page_lbl, LV_ALIGN_BOTTOM_MID, 0, -4);
        lv_obj_clear_flag(Home_page_lbl, LV_OBJ_FLAG_CLICKABLE);
        Home_FillAppsPage();''', 1)
    (output / p).write_text(s)
    p = base / 'home_screen_priv.h'
    s = (source / p).read_text().replace('constexpr int kRowsPerPage = 4;',
                                        'constexpr int kRowsPerPage = HOME_APPS_VISIBLE_ROWS;').replace('const char* icon_path;', 'const void* icon_path;')
    (output / p).write_text(s)
    p = base / 'home_apps_slash.cc'
    s = (source / p).read_text().replace('        lv_obj_clean(Home_apps);', '''        const int pages = Home_PageCount();
        if (Home_page < 0) Home_page = 0;
        if (Home_page >= pages) Home_page = pages - 1;
        lv_obj_clean(Home_apps);''', 1)
    s = s.replace('const int base = r * 2;', 'const int base = Home_page * HOME_SLASH_ROWS * 2 + r * 2;', 1)
    s = s.replace('            lv_obj_add_flag(Home_page_lbl, LV_OBJ_FLAG_HIDDEN);', '''            char text[32];
            std::snprintf(text, sizeof(text), "%d / %d", Home_page + 1, pages);
            lv_label_set_text(Home_page_lbl, text);
            if (pages > 1) lv_obj_clear_flag(Home_page_lbl, LV_OBJ_FLAG_HIDDEN);
            else lv_obj_add_flag(Home_page_lbl, LV_OBJ_FLAG_HIDDEN);''', 1)
    (output / p).write_text(s)
    p = Path('main/display/lv_adapter_display.cc')
    s = '#include "codex_monitor/ble/ble_nus.h"\n' + (source / p).read_text()
    s = s.replace('    ESP_ERROR_CHECK(esp_lv_adapter_start());',
                  '    // Prepare shared BLE memory before the first EPD DMA allocation.\n'
                  '    // No advertising or application launch on boot.\n'
                  '    ble_nus::prepare("Codex Ink");\n\n'
                  '    ESP_ERROR_CHECK(esp_lv_adapter_start());', 1)
    (output / p).write_text(s)
