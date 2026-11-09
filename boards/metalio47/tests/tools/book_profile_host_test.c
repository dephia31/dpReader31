/* SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0 */
#define main legacy_settings_main
#include "settings_backup_host_test.c"
#undef main
#include <unistd.h>
int main(void) {
    mkdir("build",0775);mkdir("build/book-tests",0775);mkdir(APP_SETTINGS_BACKUP_ROOT,0775);
    assert(mkdir(BOOK_STATE_SD_ROOT,0775)==0);
    app_settings_init();
    uint8_t defaults=app_settings_book_px();
    bool footer=app_settings_book_footer_enabled();
    const char *a="/sdcard/books/a.epub",*b="/sdcard/books/b.epub";
    int before=commit_count;
    commit_fails=true;
    app_settings_book_profile_enter(a,900);
    app_settings_set_book_px(40);assert(app_settings_book_px()==40);
    assert(app_settings_set_book_weight_fine(12));
    assert(app_settings_set_book_word_spacing(-3));
    assert(app_settings_set_book_footer_enabled(!footer));
    app_settings_set_font_path("/sdcard/fonts/test.ttf");assert(!strcmp(app_settings_font_path(),"/sdcard/fonts/test.ttf"));
    app_settings_set_font_path("builtin");assert(!strcmp(app_settings_font_path(),"builtin"));
    assert(commit_count==before);
    app_settings_book_profile_leave();assert(app_settings_book_px()==defaults);
    assert(app_settings_book_footer_enabled()==footer);
    app_settings_book_profile_enter(b,900);assert(app_settings_book_px()==defaults);
    app_settings_set_book_px(36);assert(app_settings_book_px()==36);
    app_settings_book_profile_enter(a,900);
    assert(app_settings_book_px()==40&&app_settings_book_weight_fine()==12&&app_settings_book_word_spacing()==-3);
    assert(rename(BOOK_STATE_SD_ROOT,"removed-card")==0);
    assert(!app_settings_set_book_weight_fine(16));
    app_settings_set_book_px(48);assert(app_settings_book_px()==40);
    assert(app_settings_book_weight_fine()==12);
    assert(rename("removed-card",BOOK_STATE_SD_ROOT)==0);
    reader_backup_t backup;reader_backup_capture(&backup);profile_global_backup(&backup);
    assert(backup.values[0]==defaults);
    app_settings_book_profile_enter(a,901);assert(app_settings_book_px()==defaults);
    app_settings_book_profile_leave();
    assert(book_state_write(BOOK_STATE_SD_ROOT,b,"profile","x",1)==ESP_OK);
    app_settings_book_profile_enter(b,900);assert(app_settings_book_px()==defaults);
    app_settings_book_profile_leave();
    puts("PASS scoped appearance, separate books, full NVS, lost media rollback, size guard and global backup");
    return 0;
}
