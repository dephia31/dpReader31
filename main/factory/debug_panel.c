/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 调试面板。入口：开机时按住音量键（FCA9555 P1.0/P1.1 低有效）。
 * 面板提供：
 *   - VCOM 校准（进现有标定页，值存 NVS，写运行寄存器）
 *   - 防褪色重刷间隔（关 / 1 / 3 / 5 / 10 / 30 分钟，静置到点整屏 GC16 重推）
 *   - 立即整屏重刷（强光下变淡后一键把画面推回来）
 *   - 继续开机
 *
 * Debug panel, entered by holding a volume key while powering on. It exposes VCOM
 * calibration, the anti-fade refresh interval, a manual full-frame refresh, and
 * continue-to-boot. This page never runs in normal operation.
 */

#include "debug_panel.h"

#include <stdio.h>

#include "anti_fade.h"
#include "display.h"
#include "e0470_epaper_waveform.h"
#include "epdiy.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "metalio_hw.h"
#include "ui_kit.h"
#include "vcom_setup.h"

#define TAG "debug_panel"

#define DEBUG_ROWS 4
#define DEBUG_ROW_VCOM 0
#define DEBUG_ROW_ANTI_FADE 1
#define DEBUG_ROW_REFRESH 2
#define DEBUG_ROW_CONTINUE 3

static const unsigned k_anti_fade_steps[] = {0, 1, 3, 5, 10, 30};
#define ANTI_FADE_STEP_COUNT ((int)(sizeof(k_anti_fade_steps) / sizeof(k_anti_fade_steps[0])))

static EpdRect debug_row_rect(int index) {
    const int x = UI_MARGIN;
    const int width = UI_LOCK_WIDTH - 2 * UI_MARGIN;
    const int y = UI_CONTENT_TOP + 16 + index * (UI_BTN_H + UI_GAP);
    return (EpdRect){ .x = x, .y = y, .width = width, .height = UI_BTN_H };
}

static void debug_format_vcom(char* out, size_t size, int mv) {
    if (mv >= 0) {
        snprintf(out, size, "-%d.%02d V", mv / 1000, (mv % 1000) / 10);
    } else {
        snprintf(out, size, "读取失败");
    }
}

static void debug_labels(char labels[DEBUG_ROWS][72]) {
    char chip[24];
    debug_format_vcom(chip, sizeof(chip), metalio_tps_vcom_mv());
    snprintf(labels[DEBUG_ROW_VCOM], 72, "VCOM 校准  %s", chip);

    const unsigned anti_fade = anti_fade_minutes();
    if (anti_fade == 0) {
        snprintf(labels[DEBUG_ROW_ANTI_FADE], 72, "防褪色重刷  关闭");
    } else {
        snprintf(labels[DEBUG_ROW_ANTI_FADE], 72, "防褪色重刷  %u 分钟", anti_fade);
    }

    snprintf(labels[DEBUG_ROW_REFRESH], 72, "立即整屏重刷");
    snprintf(labels[DEBUG_ROW_CONTINUE], 72, "继续开机 Continue");
}

static void debug_draw(uint8_t* fb) {
    ui_clear_page(fb);
    char chip[24];
    debug_format_vcom(chip, sizeof(chip), metalio_tps_vcom_mv());
    char subtitle[96];
    snprintf(
        subtitle, sizeof(subtitle), "芯片 VCOM %s；点按下方执行，Continue 退出", chip
    );
    ui_draw_header(fb, "调试 Debug", subtitle);
    char labels[DEBUG_ROWS][72];
    debug_labels(labels);
    for (int i = 0; i < DEBUG_ROWS; i++) {
        ui_draw_button(fb, debug_row_rect(i), labels[i], false);
    }
}

static void debug_present(EpdiyHighlevelState* hl) {
    guard_draw_result(hl, update_display_full(hl));
}

static void debug_wait_release(cst836u_handle_t tp) {
    for (;;) {
        cst836u_touch_t touch;
        if (cst836u_read(tp, &touch) == ESP_OK && !touch.touched) return;
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// 间隔在 关 / 1 / 3 / 5 / 10 / 30 分钟之间循环。
// Cycle the anti-fade interval through off / 1 / 3 / 5 / 10 / 30 minutes.
static void debug_cycle_anti_fade(void) {
    const unsigned current = anti_fade_minutes();
    int index = 0;
    for (int i = 0; i < ANTI_FADE_STEP_COUNT; i++) {
        if (k_anti_fade_steps[i] == current) {
            index = i;
            break;
        }
    }
    const unsigned next = k_anti_fade_steps[(index + 1) % ANTI_FADE_STEP_COUNT];
    anti_fade_set_minutes(next);
    ESP_LOGI(TAG, "anti-fade interval -> %u min", next);
}

void debug_panel_run(EpdiyHighlevelState* hl, uint8_t* fb, cst836u_handle_t tp) {
    // 只在需要时才重画+整屏推；空闲时只读触摸，不能每轮都整屏刷新。
    // Redraw/push only when something changed; idle iterations must not refresh.
    bool redraw = true;
    for (;;) {
        if (redraw) {
            debug_draw(fb);
            debug_present(hl);
            redraw = false;
        }

        // 等一次按下再处理，避免同一次触摸连触两下。
        // Wait for one press, then handle it; ignore the held state afterwards.
        cst836u_touch_t touch;
        if (cst836u_read(tp, &touch) != ESP_OK || !touch.touched) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }
        if (cst836u_read(tp, &touch) != ESP_OK || !touch.touched) continue;

        if (ui_rect_hit(debug_row_rect(DEBUG_ROW_VCOM), touch.x, touch.y)) {
            debug_wait_release(tp);
            vcom_setup_run(hl, fb, tp);
            debug_wait_release(tp);
            redraw = true;
            continue;
        }
        if (ui_rect_hit(debug_row_rect(DEBUG_ROW_ANTI_FADE), touch.x, touch.y)) {
            debug_wait_release(tp);
            debug_cycle_anti_fade();
            redraw = true;
            continue;
        }
        if (ui_rect_hit(debug_row_rect(DEBUG_ROW_REFRESH), touch.x, touch.y)) {
            // 整屏 GC16 全像素重刷：强光照射后点一下，检验画面能否推回。
            // Full-pixel GC16 of the current page: after strong light exposure this
            // checks whether the frame can be driven back.
            debug_wait_release(tp);
            debug_present(hl);
            continue;
        }
        if (ui_rect_hit(debug_row_rect(DEBUG_ROW_CONTINUE), touch.x, touch.y)) {
            debug_wait_release(tp);
            return;
        }
    }
}
