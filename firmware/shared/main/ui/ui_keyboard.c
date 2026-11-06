/* SPDX-License-Identifier: Apache-2.0
 * Tiếng Việt: Bàn phím QWERTY tiếng Anh chuẩn cho máy đọc sách, hỗ trợ chữ hoa/thường, số, ký tự và lặp phím xóa.
 * English: Standard English QWERTY keyboard with uppercase/lowercase, numbers, symbols, and repeating backspace.
 */
#include "ui_keyboard.h"
#include <ui_kit.h>
#include "read_pico_search.h"
#if KIIKO_BOARD_METALIO47
#include "metalio_hw.h"
#endif
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static ui_text_edit_t *s_edit;
static char s_notice[80];
static bool s_ascii, s_upper;
static unsigned s_panel;
static bool s_pressed, s_probe, s_keys_only;
static int s_probe_x, s_probe_y;
static EpdRect s_pressed_area, s_feedback_area, s_key_clip;
enum { DIRTY_FIELD = 1, DIRTY_WORDS = 2, DIRTY_LAYOUT = 16, DIRTY_FEEDBACK = 32 };
static unsigned s_dirty;
static bool s_delete_hold;
static EpdRect s_delete_area;
static int s_delete_top;
static int64_t s_delete_next_ms, s_delete_last_ms;

static uint32_t stamp(const char *text) {
    uint32_t value = 2166136261u;
    for (const unsigned char *p = (const unsigned char *)text; *p; ++p) value = (value ^ *p) * 16777619u;
    return value;
}

static const char *const ascii_symbols[] = {
    "!","@","#","$","%","^","&","*","(",")",
    "-","_","+","=","/","\\",":",";","\"","'",
    ".",",","?","<",">","[","]","{","}","|","`","~"
};

static EpdRect rect(int x, int y, int w, int h, int top) { return (EpdRect){x, y + top, w, h}; }

static void refresh(void) {
    s_notice[0] = 0;
}

void ui_keyboard_end(void) {
    ui_keyboard_hold_cancel();
    s_pressed = s_probe = s_keys_only = false;
    s_feedback_area = (EpdRect){0};
    s_edit = NULL;
    s_dirty = 0;
    memset(s_notice, 0, sizeof(s_notice));
}

void ui_keyboard_begin(ui_text_edit_t *edit, bool ascii_only) {
    ui_keyboard_end();
    s_edit = edit;
    s_ascii = ascii_only;
    s_upper = false;
    s_panel = 0;
    refresh();
}

bool ui_keyboard_pending(void) { return false; }
bool ui_keyboard_page(int direction) { (void)direction; return false; }

static bool insert(const char *text) {
    if (s_edit && ui_text_edit_insert(s_edit, text)) { s_notice[0] = 0; return true; }
    snprintf(s_notice, sizeof(s_notice), "Đã đạt giới hạn độ dài văn bản");
    return false;
}

static bool same_rect(EpdRect a, EpdRect b) {
    return a.x == b.x && a.y == b.y && a.width == b.width && a.height == b.height;
}

static bool intersects(EpdRect a, EpdRect b) {
    return a.x < b.x + b.width && b.x < a.x + a.width && a.y < b.y + b.height && b.y < a.y + a.height;
}

static void key_paint(uint8_t *fb, EpdRect r, const char *text, int px, bool dark, bool utility) {
    if (s_probe) { if (ui_rect_hit(r, s_probe_x, s_probe_y)) s_key_clip = r; return; }
    if (s_keys_only && !intersects(r, s_key_clip)) return;
    bool pressed = s_pressed && same_rect(r, s_pressed_area);
    ui_fill_round_rect(fb, r, 12, dark ? (pressed ? 0x50 : UI_GRAY_BLACK) : pressed ? 0xb0 : utility ? 0xd8 : UI_GRAY_WHITE);
    ui_draw_round_rect(fb, r, 12, pressed ? 0x20 : 0x60);
    if (!strcmp(text, "⌫")) {
        int cx = r.x + r.width / 2, cy = r.y + r.height / 2 + (pressed ? 2 : 0);
        for (int d = 0; d < 2; ++d) {
            epd_draw_line(cx - 17, cy, cx - 7, cy - 11 + d, UI_GRAY_BLACK, fb);
            epd_draw_line(cx - 17, cy, cx - 7, cy + 11 - d, UI_GRAY_BLACK, fb);
            epd_draw_line(cx - 7, cy - 11 + d, cx + 17, cy - 11 + d, UI_GRAY_BLACK, fb);
            epd_draw_line(cx - 7, cy + 11 - d, cx + 17, cy + 11 - d, UI_GRAY_BLACK, fb);
            epd_draw_line(cx + 17 - d, cy - 11, cx + 17 - d, cy + 11, UI_GRAY_BLACK, fb);
            epd_draw_line(cx - 2, cy - 5 + d, cx + 8, cy + 5 + d, UI_GRAY_BLACK, fb);
            epd_draw_line(cx - 2, cy + 5 + d, cx + 8, cy - 5 + d, UI_GRAY_BLACK, fb);
        }
#if KIIKO_BOARD_METALIO47
    } else if (!strcmp(text, "⇧")) {
        const int cx = r.x + r.width / 2, cy = r.y + r.height / 2 + (pressed ? 2 : 0);
        const int points[][2] = {{0,-16},{-18,1},{-7,1},{-7,15},{7,15},{7,1},{18,1},{0,-16}};
        for (int d = 0; d < 2; d++) {
            for (unsigned i = 1; i < sizeof(points) / sizeof(points[0]); i++) {
                epd_draw_line(cx + points[i-1][0] + d, cy + points[i-1][1],
                              cx + points[i][0] + d, cy + points[i][1],
                              dark ? UI_GRAY_WHITE : UI_GRAY_BLACK, fb);
            }
        }
#else
#endif
    } else {
        ui_text_fixed_vc(fb, r.x + r.width / 2, r.y + r.height / 2 + (pressed ? 2 : 0), px, text, EPD_DRAW_ALIGN_CENTER, dark);
    }
}

static void paint_words(uint8_t *fb, int top) {
    if (!s_probe && !s_keys_only) {
        if (s_notice[0]) {
            ui_text_fixed_vc(fb, 28, top + 75, 22, s_notice, EPD_DRAW_ALIGN_LEFT, false);
        } else {
            ui_text_fixed_vc(fb, 28, top + 75, 22, "Chạm để nhập, có thể đổi chữ hoa/thường", EPD_DRAW_ALIGN_LEFT, false);
        }
    }
}

void ui_keyboard_draw(uint8_t *fb, int top) {
    if (!s_probe && !s_keys_only) {
        ui_fill_round_rect(fb, rect(16, -12, 652, 534, top), 26, 0xe8);
        ui_draw_round_rect(fb, rect(16, -12, 652, 534, top), 26, 0x60);
    }
    key_paint(fb, rect(24, 0, 144, 44, top), "QWERTY", 24, true, false);
    if (!s_probe && !s_keys_only) {
        ui_text_fixed_vc(fb, 651, top + 22, 24, s_ascii ? "Mật khẩu" : s_upper ? "CHỮ HOA" : "chữ thường", EPD_DRAW_ALIGN_RIGHT, false);
    }
    paint_words(fb, top);

    static const char *const rows[] = {"qwertyuiop", "asdfghjkl", "zxcvbnm"};
    for (int row = 0; row < 3; ++row) {
        int len = s_panel ? 10 : (int)strlen(rows[row]);
        int left = s_panel || !row ? 24 : row == 1 ? 55 : 117;
        for (int col = 0; col < len; ++col) {
            char letter[2] = {0};
            const char *label;
            if (s_panel == 1) {
                letter[0] = row == 0 ? "1234567890"[col] : ascii_symbols[(row - 1) * 10 + col][0];
                label = letter;
            } else if (s_panel >= 2) {
                label = ascii_symbols[s_panel == 3 && row == 0 && col < 6 ? (const int[]){30,31,28,29,26,27}[col] : row * 10 + col];
            } else {
                letter[0] = s_upper ? (char)(rows[row][col] - 32) : rows[row][col];
                label = letter;
            }
            key_paint(fb, rect(left + col * 64, 180 + row * 78, 56, 68, top), label, 28, false, false);
        }
    }
    if (s_panel) {
        key_paint(fb, rect(536, 336, 56, 68, top), "Xóa", 21, false, true);
        key_paint(fb, rect(600, 336, 56, 68, top), "⌫", 28, false, true);
    } else {
#if KIIKO_BOARD_METALIO47
        key_paint(fb, rect(24, 336, 80, 68, top), "⇧", 30, s_upper, true);
#else
        key_paint(fb, rect(24, 336, 80, 68, top), s_upper ? "A" : "a", 30, s_upper, true);
#endif
        key_paint(fb, rect(578, 336, 82, 68, top), "⌫", 31, false, true);
    }

    key_paint(fb, rect(24, 430, 96, 72, top), s_panel ? "ABC" : "123", 25, false, true);
    key_paint(fb, rect(128, 430, 96, 72, top), s_panel == 2 ? "Thêm" : "Ký tự", 24, false, true);
    key_paint(fb, rect(232, 430, 280, 72, top), "Dấu cách", 26, false, false);
    key_paint(fb, rect(520, 430, 140, 72, top), "Xong", 28, true, false);
}

static void backspace(void) {
    if (s_edit) { ui_text_edit_backspace(s_edit); refresh(); }
    s_notice[0] = 0;
}

static void letter_key(char letter) {
    char text[2] = {s_upper ? (char)(letter - 32) : letter, 0};
    insert(text);
}

static ui_keyboard_result_t tap(int x, int y, int top) {
    if (!s_edit || y < top || y >= top + 510 || x < 24 || x >= 660) return UI_KEYBOARD_NONE;
    if (ui_rect_hit(rect(24, 0, 144, 44, top), x, y)) {
        return UI_KEYBOARD_NONE;
    }
    if (y >= top + 430 && y < top + 502) {
        if (x >= 520) {
            return UI_KEYBOARD_DONE;
        } else if (x >= 232) {
            insert(" ");
            return UI_KEYBOARD_CHANGED;
        } else if (x >= 128) {
            s_panel = (s_panel == 2 ? 3 : 2);
            return UI_KEYBOARD_CHANGED;
        } else if (x >= 24) {
            s_panel = (s_panel ? 0 : 1);
            return UI_KEYBOARD_CHANGED;
        }
        return UI_KEYBOARD_NONE;
    }
    static const char *const rows[] = {"qwertyuiop", "asdfghjkl", "zxcvbnm"};
    for (int row = 0; row < 3; ++row) {
        if (s_panel && row == 2 && ui_rect_hit(rect(600, 336, 56, 68, top), x, y)) { backspace(); return UI_KEYBOARD_CHANGED; }
        if (s_panel && row == 2 && ui_rect_hit(rect(536, 336, 56, 68, top), x, y)) {
            s_edit->text[0] = 0; ui_text_edit_place(s_edit, 0); refresh(); return UI_KEYBOARD_CHANGED;
        }
        if (!s_panel && row == 2 && ui_rect_hit(rect(24, 336, 80, 68, top), x, y)) { s_upper = !s_upper; return UI_KEYBOARD_CHANGED; }
        if (!s_panel && row == 2 && ui_rect_hit(rect(578, 336, 82, 68, top), x, y)) { backspace(); return UI_KEYBOARD_CHANGED; }
        int left = s_panel || !row ? 24 : row == 1 ? 55 : 117;
        int len = s_panel ? 10 : (int)strlen(rows[row]);
        for (int col = 0; col < len; ++col) {
            if (ui_rect_hit(rect(left + col * 64, 180 + row * 78, 56, 68, top), x, y)) {
                if (!s_panel) letter_key(rows[row][col]);
                else if (s_panel == 1) {
                    char value[2] = {row == 0 ? "1234567890"[col] : ascii_symbols[(row - 1) * 10 + col][0], 0};
                    insert(value);
                } else {
                    insert(ascii_symbols[s_panel == 3 && row == 0 && col < 6 ? (const int[]){30,31,28,29,26,27}[col] : row * 10 + col]);
                }
                return UI_KEYBOARD_CHANGED;
            }
        }
    }
    return UI_KEYBOARD_NONE;
}

static void feedback_dirty(EpdRect area) {
    s_feedback_area = s_feedback_area.width ? ui_rect_union(s_feedback_area, area) : area;
    s_dirty |= DIRTY_FEEDBACK;
}

bool ui_keyboard_release(void) {
    ui_keyboard_hold_cancel();
    if (!s_pressed) return false;
    feedback_dirty(s_pressed_area);
    s_pressed = false;
    return true;
}

bool ui_keyboard_press(int x, int y, int top, int64_t now_ms) {
    ui_keyboard_release();
    ui_keyboard_hold_start(x, y, top, now_ms);
    if (!s_edit || x < 0 || y < 0) return false;
    s_probe = true; s_probe_x = x; s_probe_y = y; s_key_clip = (EpdRect){0};
    ui_keyboard_draw(NULL, top); s_probe = false;
    if (!s_key_clip.width) return false;
    s_pressed_area = s_key_clip; s_pressed = true;
#if KIIKO_BOARD_METALIO47
    (void)metalio_haptic_click();
#endif
    feedback_dirty(s_pressed_area);
    return true;
}

void ui_keyboard_hold_cancel(void) { s_delete_hold = false; }

void ui_keyboard_hold_start(int x, int y, int top, int64_t now_ms) {
    ui_keyboard_hold_cancel();
    if (!s_edit || now_ms < 0) return;
    EpdRect area = s_panel ? rect(600, 336, 56, 68, top) : rect(578, 336, 82, 68, top);
    if (!ui_rect_hit(area, x, y)) return;
    s_delete_area = area; s_delete_top = top;
    s_delete_hold = true; s_delete_last_ms = now_ms; s_delete_next_ms = now_ms + UI_LONG_PRESS_MS;
}

ui_keyboard_result_t ui_keyboard_hold_tick(bool held, int x, int y, int64_t now_ms) {
    if (!s_delete_hold) return UI_KEYBOARD_NONE;
    if (!s_edit || !held || now_ms < s_delete_last_ms || !ui_rect_hit(s_delete_area, x, y)) {
        ui_keyboard_hold_cancel(); return UI_KEYBOARD_NONE;
    }
    s_delete_last_ms = now_ms;
    if (now_ms < s_delete_next_ms) return UI_KEYBOARD_NONE;
    if (!s_edit->cursor) { ui_keyboard_hold_cancel(); return UI_KEYBOARD_NONE; }
    s_delete_next_ms = now_ms + 120;
    ui_keyboard_result_t result = ui_keyboard_tap(s_delete_area.x + s_delete_area.width / 2,
        s_delete_area.y + s_delete_area.height / 2, s_delete_top, now_ms);
    if (result == UI_KEYBOARD_NONE) ui_keyboard_hold_cancel();
#if KIIKO_BOARD_METALIO47
    else (void)metalio_haptic_click();
#endif
    return result;
}

ui_keyboard_result_t ui_keyboard_idle_tick(bool touching, int64_t now_ms) {
    (void)touching; (void)now_ms; return UI_KEYBOARD_NONE;
}

ui_keyboard_result_t ui_keyboard_tap(int x, int y, int top, int64_t now_ms) {
    (void)now_ms;
    if (!s_edit) return UI_KEYBOARD_NONE;
    uint32_t text = stamp(s_edit->text), notice = stamp(s_notice);
    size_t cursor = s_edit->cursor;
    bool upper = s_upper;
    unsigned panel = s_panel;
    ui_keyboard_result_t result = tap(x, y, top);
    if (result != UI_KEYBOARD_CHANGED) return result == UI_KEYBOARD_NONE && s_dirty ? UI_KEYBOARD_CHANGED : result;
    if (text != stamp(s_edit->text) || cursor != s_edit->cursor) s_dirty |= DIRTY_FIELD;
    if (upper != s_upper || panel != s_panel) s_dirty |= DIRTY_LAYOUT;
    else {
        if (notice != stamp(s_notice)) s_dirty |= DIRTY_WORDS;
    }
    return s_dirty ? result : UI_KEYBOARD_NONE;
}

static void include(ui_keyboard_update_t *update, EpdRect area) {
    update->area = update->area.width ? ui_rect_union(update->area, area) : area;
}

ui_keyboard_update_t ui_keyboard_update(uint8_t *fb, int top, EpdRect field, int px,
                                        bool masked, const char *suffix, bool force_field) {
    ui_keyboard_update_t update = {.layout = (s_dirty & DIRTY_LAYOUT) != 0,
        .settle = false};
    if (!s_edit) return update;
    if (force_field || (s_dirty & DIRTY_FIELD)) {
        ui_clear_rect_fast(fb, field);
        ui_text_input_draw(fb, s_edit, field, px, masked, suffix);
        include(&update, field); update.field = true;
    }
    if (update.layout) {
        ui_keyboard_draw(fb, top);
        include(&update, rect(16, -12, 652, 534, top));
    } else {
        if (s_dirty & DIRTY_WORDS) {
            EpdRect area = rect(24, 52, 636, 114, top);
            epd_fill_rect(area, 0xe8, fb); paint_words(fb, top); include(&update, area);
        }
    }
    if (!update.layout && (s_dirty & DIRTY_FEEDBACK)) {
        s_keys_only = true; s_key_clip = s_feedback_area;
        ui_keyboard_draw(fb, top); s_keys_only = false;
        include(&update, s_feedback_area);
    }
    s_feedback_area = (EpdRect){0}; s_dirty = 0;
    return update;
}
