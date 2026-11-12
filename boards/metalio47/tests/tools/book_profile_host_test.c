/* SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0 */
#define main legacy_settings_main
#include "settings_backup_host_test.c"
#undef main

static void check_reading_settings(uint8_t margin, uint8_t paragraph, const char *font) {
    assert(app_settings_book_px() == 40);
    assert(app_settings_book_weight_fine() == 12);
    assert(!app_settings_book_antialias());
    assert(app_settings_book_word_spacing() == -3);
    assert(app_settings_book_line_spacing() == (uint8_t)-60);
    assert(app_settings_book_paragraph_spacing() == paragraph);
    assert(app_settings_book_margin() == margin);
    assert(app_settings_book_tracking() == 3 - APP_BOOK_TRACK_MIN);
    assert(!strcmp(app_settings_font_path(), font));
}

int main(void) {
    app_settings_init();
    app_settings_set_book_px(40);
    assert(app_settings_set_book_weight_fine(12));
    assert(app_settings_set_book_antialias(false));
    assert(app_settings_set_book_word_spacing(-3));
    app_settings_set_book_line_spacing((uint8_t)-60);
    app_settings_set_book_paragraph_spacing((uint8_t)-70);
    app_settings_set_book_margin(6);
    app_settings_set_book_tracking(3 - APP_BOOK_TRACK_MIN);
    app_settings_set_font_path("/sdcard/fonts/test.ttf");
    check_reading_settings(6, (uint8_t)-70, "/sdcard/fonts/test.ttf");

    app_settings_init();
    check_reading_settings(6, (uint8_t)-70, "/sdcard/fonts/test.ttf");
    app_settings_set_book_margin(24);
    app_settings_set_book_paragraph_spacing(50);
    app_settings_set_font_path("builtin");
    app_settings_init();
    check_reading_settings(24, 50, "builtin");

    reader_backup_t backup;
    reader_backup_capture(&backup);
    assert(reader_backup_valid(&backup));
    app_settings_set_book_margin(0);
    app_settings_set_book_paragraph_spacing(0);
    reader_backup_apply(&backup);
    check_reading_settings(24, 50, "builtin");

    puts("PASS: shared reading settings survive reboot and subsequent changes, and are captured by backup");
    return 0;
}
