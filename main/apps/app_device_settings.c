/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：分组设置页；网络短时对时后 PMU 持续走时，开机恢复系统时钟。
 * English: Grouped settings; temporary WiFi sync seeds the PMU, whose RTC restores time at boot.
 * Revision2026-10-30: root shows five parent categories; detail Back retains its
 * category and scroll, then category Back returns to the root profile/categories.
 * 用户修订：设置列表滑动仅差分刷新内容，滑动过程中不周期插入黑白清屏。
 * User revision: settings lists scroll with content-only differentials and no periodic black/white wipe during swipes.
 * 用户授权新输入法：资料卡和签名共用九宫格/全键盘及离线词语候选，保存仍由设置页负责。
 * Authorized keyboard revision: profile and signature share T9/QWERTY and offline phrases; this page still owns saving.
 * 用户修订：资料卡与签名输入只画变化区域，不因输入次数触发整屏黑白清屏。
 * User revision: profile/signature input paints changed regions without whole-screen wipes based on typing counts.
 * 用户修订：快刷说明标注棋盘格亚克力，普通与水波纹继续保留灰阶；三种主页面刷新模式均固定底栏。
 * User revision: the fast-mode description identifies checkerboard acrylic; ordinary/water keep grays and all main-screen modes retain navigation.
 * 用户修订：联网升级先画反馈再启动任务；进度只追加黑色段，不擦除已完成段，不累计整页清屏。升级前释放不在显示的封面缓存。
 * User revision: present feedback before starting online work; append black progress without erasing completed segments or counting page cleanup. Release off-screen cover caches before updating.
 * 用户确认：字号并入系统字体并预览；选择字体后询问同步阅读字体，设置按连接/显示/个性化/升级分组。
 * Approved revision: size and preview live with the UI face; ask before syncing the reading face, and group connection/display/personalization/maintenance.
 * 用户修订：设置题头与列表周围留白保持纯白，普通主页切入/周期整理复用书架同轮分区输出；题头柔和，内容白像素只向白擦除，灰阶保留。普通列表滑动使用柔和差分；手动全刷、离散滚动和快刷/水波纹节奏保持。
 * User revision: keep setting chrome/list whitespace white. Ordinary root entry/periodic cleanup reuse simultaneous shelf regions with soft chrome, directional white content and preserved grays. Scroll ordinary lists with soft differences; retain manual full refresh, discrete gestures and fast/ripple pacing.
 * 用户2026-10-20修订：设置二级页常驻灰底栏，内容止于底栏上方，返回清理和不可中断任务守卫保持。
 * User revision2026-10-20: settings children retain gray navigation; content stops above it, reusing back cleanup and noninterruptible-job guards.
 * S31用户修订：设置主列表内容规整为黑白并使用DU20，固定底栏不变；子页的字体/壁纸预览仍保留灰阶。
 * S31 user revision: normalize root settings content to BW and use DU20, retaining fixed navigation and gray font/wallpaper previews in subpages.
 * 用户撤回跟手滚动：三种主页模式统一恢复一次手势推进一段列表并差分刷新的原逻辑，无惯性、浅墨或停稳补刷。
 * User rollback: all main modes return to one discrete list step per gesture with differential updates, without inertia, light motion ink or idle restoration.
 * 用户2026-10-21修订：设置列表滚动保留文字抗锯齿和灰阶头像，撤掉书架额外四相补白，列表仍一次输出且不影响固定底栏。
 * User revision2026-10-21: keep AA text and gray avatars during list scrolls, omit the shelf's four added white phases, and retain one output with fixed navigation.
 * 用户2026-10-29要求修复大字号遮挡：题头保留Back/右侧操作，主列表大字号上下排列，固定提示与按钮必须按真实字形限制在各自区域内；命中区共享布局尺寸。
 * User requests large-text overlap fixes on2026-10-29: reserve Back/right actions, stack large main rows, fit captions/buttons by actual glyph bounds, and share layout sizes with hit testing.
 */
#if KIIKO_BOARD_METALIO47
#include "app_content_open.h"
#endif
#include <stdio.h>
#include <stdlib.h>
#include "esp_heap_caps.h"
#include <dirent.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>

#include "app.h"
#include "app_content_open.h"
#include "app_registry.h"
#include "app_transfer_mode.h"
#include "settings.h"
#include "factory_reset.h"
#include "book_cover_jobs.h"
#if KIIKO_BOARD_METALIO47
#include "sleep.h"
#include "sd_recovery.h"
#else
#endif
#include "lock_pin_flow.h"
#include "ui_pinpad.h"
#include "book_lock_collage.h"
#include "book_lock_bookmark.h"
#include "boot_state.h"
#include "log_export.h"
#include "read_pico_sd.h"
#include "read_pico_crashlog.h"
#include "ble_page_turner.h"
#include "read_pico_pmu.h"
#include "read_pico_pmu_protocol.h"
#include "read_pico_transfer.h"
#include "esp_log.h"
#include "ttf_font.h"
#if KIIKO_BOARD_METALIO47
#include "font_filename.h"
#endif
#include "app_font_context.h"
#if KIIKO_BOARD_METALIO47
#include "ui_online_fonts.h"
static ui_online_fonts_t s_online_fonts;
#endif
#include "ui_gesture.h"
#include "ui_kit.h"
#include "ui_text_input.h"
#include "ui_keyboard.h"
#include "ui_nav.h"
#include "ui_wallpaper.h"
#include "ui_avatar.h"
#include "read_pico_search.h"
#include "book_store.h"
#include "book_cover.h"
#include "ota_update.h"
#include "ota_online.h"
#include "esp_app_desc.h"
#include "esp_attr.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "display.h"
#include "display_main_waveform.h"
#include "e0470_epaper_waveform.h"
#include "pmu_selftest.h"
#include "kosync_client.h"
#include "opds_client.h"
#if KIIKO_BOARD_METALIO47
#include "metalio_hw.h"
#else
#include "soc/rtc_cntl_reg.h"
#endif

static void fit_value(char *value, int width);
static void settings_card(uint8_t *fb, EpdRect rect, int radius, uint8_t fill, uint8_t edge);
static char s_notice[96];
static char s_kosync_notice[96];
static char s_opds_notice[96];
typedef enum { SETTINGS_MAIN, SETTINGS_WIFI, SETTINGS_WIFI_TIMEOUT, SETTINGS_BLE_TIMEOUT, SETTINGS_TIME,
               SETTINGS_KOSYNC, SETTINGS_OPDS,
               SETTINGS_TIME_EDIT, SETTINGS_SHELF_STYLE, SETTINGS_SYSTEM_FONT,
#if KIIKO_BOARD_METALIO47
               SETTINGS_ONLINE_FONTS,
               SETTINGS_SD_RECOVERY,
#endif
               SETTINGS_SYSTEM_SIZE, SETTINGS_SYSTEM_CONTRAST, SETTINGS_MAIN_REFRESH, SETTINGS_LOCK_STYLE,
               SETTINGS_WALLPAPER, SETTINGS_WALLPAPER_PREVIEW,
               SETTINGS_CONFIG, SETTINGS_UPGRADE, SETTINGS_BOOT, SETTINGS_FACTORY_RESET,
               SETTINGS_LOG_EXPORT,
#if KIIKO_BOARD_METALIO47
               SETTINGS_POWER_SLEEP, SETTINGS_AUTO_LOCK, SETTINGS_PROFILE, SETTINGS_AVATAR,
#else
               SETTINGS_POWER_SLEEP, SETTINGS_AUTO_LOCK, SETTINGS_PROFILE, SETTINGS_AVATAR,
#endif
               SETTINGS_BLUETOOTH, SETTINGS_BLE_SCAN,
#if KIIKO_BOARD_METALIO47
               SETTINGS_LOCK_PIN, SETTINGS_PIN_ENTRY, SETTINGS_TEXT_EDIT, SETTINGS_FRONTLIGHT,
               SETTINGS_CONTROLS } settings_page_t;
#else
               SETTINGS_LOCK_PIN, SETTINGS_PIN_ENTRY, SETTINGS_TEXT_EDIT,
               SETTINGS_CONTROLS } settings_page_t;
#endif
static settings_page_t s_page;
// Keep the selected parent while visiting its detail pages.
static int s_settings_group = -1;
static int s_settings_root_scroll;
static const char *const settings_group_titles[] = {
    "Kết nối", "Hiển thị", "Cá nhân hóa", "Điều khiển", "Cập nhật & Khôi phục"
};
#if KIIKO_BOARD_METALIO47
static char s_sd_notice[96];
static EpdRect sd_recovery_button(void) { return (EpdRect){54,850,576,78}; }
#endif
static int settings_content_end(void) {
    return UI_NAV_REFRESH_END;
}
static int settings_viewport_end(void) {
    return UI_NAV_TOP;
}
static void settings_navigation(uint8_t *fb) {
#if KIIKO_BOARD_METALIO47
    if(s_page==SETTINGS_ONLINE_FONTS)return;
#endif
    ui_nav_draw(fb,3);
}
static int settings_nav_hit(uint16_t x,uint16_t y) {
    return ui_nav_hit(x,y);
}

static bool s_reset_confirm, s_reset_pending;
static char s_reset_notice[96];
static ui_pinpad_t s_pinpad;
static lock_pin_flow_t s_pinflow;
static bool s_pin_pending;
static unsigned s_pin_failures;
static int64_t s_pin_retry_at;
static void pin_setup_close(void) {
    ui_pinpad_end(&s_pinpad); lock_pin_flow_end(&s_pinflow); s_pin_pending = false;
}
static void pin_setup_open(app_ctx_t *ctx, bool disable) {
    pin_setup_close(); lock_pin_flow_begin(&s_pinflow, disable);
    ui_pinpad_begin(&s_pinpad, ctx->fb, lock_pin_flow_title(&s_pinflow));
    s_pinpad.navigation=true;
    s_page = SETTINGS_PIN_ENTRY;
}
static app_redraw_t pin_paint(app_ctx_t *ctx, EpdRect area) {
    ui_pinpad_paint(ctx->fb, &s_pinpad, area);
#if KIIKO_BOARD_METALIO47
    bool previous=display_system_binary_set(true);
    bool previous_nav=display_system_navigation_set(true);
#endif
    guard_draw_result(ctx->hl, update_display_area_diff_with(ctx->hl, &E0470_WAVEFORM, MODE_GL16, area));
#if KIIKO_BOARD_METALIO47
    display_system_navigation_set(previous_nav);
    display_system_binary_set(previous);
#endif
    return APP_REDRAW_DONE;
}
static app_redraw_t pin_setup_tick(app_ctx_t *ctx) {
    if (s_pinpad.blocked && ctx->now_ms >= s_pin_retry_at) {
        ui_pinpad_reset(&s_pinpad, NULL, "Vui lòng nhập lại");
        return pin_paint(ctx, ui_pinpad_entry_area());
    }
    if (!s_pin_pending) return APP_REDRAW_NONE;
    s_pin_pending = false;
    lock_pin_step_t prior = s_pinflow.step;
    esp_err_t error = lock_pin_flow_input(&s_pinflow, s_pinpad.digits);
    if (error == ESP_OK && s_pinflow.step == PIN_FLOW_DONE) {
        pin_setup_close(); s_page = SETTINGS_LOCK_PIN;
        snprintf(s_notice, sizeof(s_notice), "%s", lock_pin_enabled() ? "Đã lưu mật khẩu khóa màn hình" : "Đã tắt mật khẩu khóa màn hình");
        return APP_REDRAW_PAGE;
    }
    const char *notice = error == ESP_OK ? "" : error == ESP_ERR_INVALID_RESPONSE ? "Hai lần nhập không khớp, vui lòng nhập lại" :
        error == ESP_ERR_INVALID_ARG ? "Mật khẩu hiện tại không đúng, vui lòng thử lại" : "Lưu thất bại, vui lòng thử lại";
    ui_pinpad_reset(&s_pinpad, lock_pin_flow_title(&s_pinflow), notice);
    if (error == ESP_ERR_INVALID_ARG && prior == PIN_FLOW_OLD && ++s_pin_failures >= 3) {
        s_pin_failures = 0; s_pin_retry_at = ctx->now_ms + 10000;
        snprintf(s_pinpad.notice, sizeof(s_pinpad.notice), "Vui lòng đợi 10 giây rồi thử lại"); s_pinpad.blocked = true;
    }
    return pin_paint(ctx, (EpdRect){122, 218, 440, 247});
}
static int s_style_scroll, s_main_scroll;
static bool s_collage_prepare_pending;
static bool s_scroll_drag_consumed, s_scroll_present_pending;
// 蓝牙翻页器子页：滚动位置、正在学习哪个动作（0 未学，1 上一页，2 下一页）、提示行。
// Bluetooth sub-page: scroll offset, which action is being learned (0 idle, 1 prev, 2 next),
// and a notice line.
static int s_ble_scroll, s_ble_learning;
static char s_ble_notice[96];
static char s_ble_feedback[96];
static int s_font_page, s_wallpaper_page;
static bool s_sync_pending;
static bool s_font_sync_confirm;
static char s_font_sync_path[TTF_FONT_PATH_MAX];
static bool s_config_confirm;
// 屏幕修复：点一下就开始。整屏盲刷要几秒，活儿交给 tick——不能在触摸回调里做。
// Panel repair: one tap starts it. The blind full-screen drive takes seconds, so the work goes to the
// tick rather than a touch callback.
static bool s_panel_repair_pending;
static bool s_boot_pending;
#if KIIKO_BOARD_METALIO47
static bool s_boot_connected;
static char s_boot_notice[96];
#else
#endif
static bool s_upgrade_confirm, s_upgrade_online, s_local_pending, s_upgrade_restart;
static pico_ota_info_t s_local_update;
static EXT_RAM_BSS_ATTR pico_update_status_t s_update;
static char s_upgrade_notice[96];
static uint32_t s_update_drawn_percent, s_update_drawn_at;
static pico_update_state_t s_update_drawn_state;
static bool s_update_drawn_busy, s_upgrade_offer_pending;
typedef enum { UPGRADE_JOB_NONE, UPGRADE_JOB_CHECK, UPGRADE_JOB_DOWNLOAD } upgrade_job_t;
static upgrade_job_t s_upgrade_job;
static int s_upgrade_bar_width;
static uint32_t upgrade_percent(void) {
    uint32_t percent = s_update.release.size ? (uint64_t)s_update.received * 100 / s_update.release.size : 0;
    return percent > 100 ? 100 : percent;
}
static void upgrade_remember(uint32_t now) {
    s_update_drawn_state = s_update.state;
    s_update_drawn_busy = s_update.busy;
    s_update_drawn_percent = upgrade_percent();
    s_update_drawn_at = now;
}
// 结果等任务收尾后一次显示；进度只在五秒且增长五个百分点后补画新增像素。
// Show results once after worker cleanup; append progress after five seconds and five percentage points.
static app_redraw_t upgrade_status_redraw(uint32_t now) {
    bool downloading = s_update.state == PICO_UPDATE_DOWNLOADING;
    if (s_update.busy && s_update.state != PICO_UPDATE_CHECKING && !downloading)
        return APP_REDRAW_NONE;
    bool offer = s_upgrade_offer_pending && !s_update.busy && s_update.state == PICO_UPDATE_AVAILABLE;
    if (offer) {
        s_upgrade_offer_pending = false;
        s_upgrade_confirm = s_upgrade_online = true;
    } else if (!s_update.busy) s_upgrade_offer_pending = false;
    if (offer || s_update.state != s_update_drawn_state || s_update.busy != s_update_drawn_busy) {
        upgrade_remember(now);
        return APP_REDRAW_PAGE;
    }
    uint32_t percent = upgrade_percent();
    if (downloading && percent >= s_update_drawn_percent + 5 && now - s_update_drawn_at >= 5000) {
        upgrade_remember(now);
        return APP_REDRAW_AREA;
    }
    return APP_REDRAW_NONE;
}
static EpdRect upgrade_progress_area(void) { return (EpdRect){52, 468, 580, 64}; }
static void draw_upgrade_percent(uint8_t *fb) {
    uint32_t percent = upgrade_percent();
    EpdRect number = {496, 472, 132, 42};
    ui_fill_rect_fast(number, UI_GRAY_WHITE, fb);
    char progress[32]; snprintf(progress, sizeof(progress), "%lu%%", (unsigned long)percent);
    ui_text_fixed(fb, 624, 477, 26, progress, EPD_DRAW_ALIGN_RIGHT, false);
    ui_image_bw_rect(fb, number);
}
static void draw_upgrade_progress(uint8_t *fb) {
    ui_fill_rect_fast(upgrade_progress_area(), UI_GRAY_WHITE, fb);
    draw_upgrade_percent(fb);
    ui_fill_rect_fast((EpdRect){60, 518, 564, 10}, UI_GRAY_BLACK, fb);
    ui_fill_rect_fast((EpdRect){61, 519, 562, 8}, UI_GRAY_WHITE, fb);
    s_upgrade_bar_width = (int)(562 * upgrade_percent() / 100);
    if (s_upgrade_bar_width)
        ui_fill_rect_fast((EpdRect){61, 519, s_upgrade_bar_width, 8}, UI_GRAY_BLACK, fb);
}
static void draw_upgrade_progress_delta(uint8_t *fb) {
    int width = (int)(562 * upgrade_percent() / 100);
    if (width <= s_upgrade_bar_width) return;
    draw_upgrade_percent(fb);
    ui_fill_rect_fast((EpdRect){61 + s_upgrade_bar_width, 519, width - s_upgrade_bar_width, 8}, UI_GRAY_BLACK, fb);
    s_upgrade_bar_width = width;
}
// 排队时只改 UI 快照；本次画面推送结束后的 tick 才启动 TLS/擦写。
// Scheduling changes only the UI snapshot; start TLS/flash on the tick after its presentation.
static void upgrade_schedule(bool downloading, uint32_t now) {
    s_upgrade_job = downloading ? UPGRADE_JOB_DOWNLOAD : UPGRADE_JOB_CHECK;
    s_update.state = downloading ? PICO_UPDATE_DOWNLOADING : PICO_UPDATE_CHECKING;
    s_update.busy = true;
    s_update.received = 0;
    snprintf(s_update.message, sizeof(s_update.message), "%s", downloading
        ? "Đang tải xuống, sẽ tự kết nối lại nếu mạng chập chờn" : "Đang kết nối và kiểm tra bản cập nhật");
    s_upgrade_offer_pending = !downloading;
    upgrade_remember(now);
}
static app_redraw_t upgrade_start_pending(void) {
    bool downloading = s_upgrade_job == UPGRADE_JOB_DOWNLOAD;
    s_upgrade_job = UPGRADE_JOB_NONE;
    app_home_cover_mode_changed();
    app_book_cover_mode_changed();
    esp_err_t error = downloading ? pico_online_download() : pico_online_check();
    pico_online_get_status(&s_update);
    if (error == ESP_OK) return APP_REDRAW_NONE;
    s_upgrade_offer_pending = false;
    s_update.busy = false;
    if (s_update.state != PICO_UPDATE_FAILED) s_update.message[0] = 0;
    s_update.state = PICO_UPDATE_FAILED;
    if (!s_update.message[0]) snprintf(s_update.message, sizeof(s_update.message), "Không thể cập nhật lúc này, hãy thoát truyền sách rồi thử lại");
    snprintf(s_upgrade_notice, sizeof(s_upgrade_notice), "%s", s_update.message);
    return APP_REDRAW_PAGE;
}
// 日志按 UTF-8 字符和实际字宽换行，限制六行，不影响下载时的局部刷新。
// Wrap notes on UTF-8 boundaries using measured widths; cap at six lines without changing download refreshes.
static bool upgrade_note_line(const char **cursor, char *line, size_t capacity) {
    if (!cursor || !*cursor || !line || capacity < 5) return false;
    const char *p = *cursor;
    while (*p && (strchr("\r\n\t ;", *p) || !strncmp(p, "; ", 3)))
        p += !strncmp(p, "; ", 3) ? 3 : 1;
    size_t used = 0;
    line[0] = 0;
    while (*p && !strchr("\r\n;", *p) && strncmp(p, "; ", 3)) {
        unsigned char lead = (unsigned char)*p;
        size_t bytes = lead < 0x80 ? 1 : lead < 0xe0 ? 2 : lead < 0xf0 ? 3 : 4;
        if (bytes > strlen(p) || used + bytes >= capacity) break;
        memcpy(line + used, p, bytes);
        line[used + bytes] = 0;
        if (used && ui_text_fixed_width_px(26, line) > 540) { line[used] = 0; break; }
        used += bytes;
        p += bytes;
    }
    *cursor = p;
    return used != 0;
}
static void draw_upgrade_notes(uint8_t *fb) {
    const char *cursor = s_update.release.notes;
    char line[sizeof(s_update.release.notes)];
    if (!*cursor) {
        ui_text_fixed(fb, 60, 530, 26, "Không có ghi chú phát hành cho bản cập nhật này", EPD_DRAW_ALIGN_LEFT, false);
        return;
    }
    for (int row = 0; row < 6 && upgrade_note_line(&cursor, line, sizeof(line)); ++row) {
        if (row == 5 && *cursor) {
            size_t used = strlen(line);
            while (used && (used + 4 > sizeof(line) || ui_text_fixed_width_px(26, line) > 514)) {
                do { --used; } while (used && ((unsigned char)line[used] & 0xc0) == 0x80);
                line[used] = 0;
            }
            memcpy(line + used, "…", 4);
        }
        ui_text_fixed(fb, 60, 530 + row * 38, 26, line, EPD_DRAW_ALIGN_LEFT, false);
    }
}
static EpdRect upgrade_confirm_button(bool online, bool accept) {
    return (EpdRect){accept ? 364 : 60, online ? 842 : 613, 260, 64};
}
#if KIIKO_BOARD_METALIO47
// 中文：本地TF升级与显示同属UI任务；先归还缓存并关面板轨，再擦写应用槽。
// English: Local TF installation runs on the display's UI task; release caches/rails before flash writes.
static esp_err_t install_local_update(void) {
    if (!(book_cover_jobs_quiesce(2000) && ui_avatar_quiesce(2000))) {
        snprintf(s_upgrade_notice,sizeof(s_upgrade_notice),"Đang xử lý ảnh bìa, vui lòng thử lại sau giây lát");
        return ESP_ERR_NOT_FINISHED;
    }
    app_home_cover_mode_changed();
    app_book_cover_mode_changed();
    epd_poweroff();
    return pico_ota_install(PICO_OTA_UPDATE_PATH, s_upgrade_notice, sizeof(s_upgrade_notice));
}

#else
#endif
static void upgrade_open(void) {
    (void)pico_ota_inspect(PICO_OTA_UPDATE_PATH, &s_local_update);
    pico_online_get_status(&s_update);
    s_upgrade_confirm = s_local_pending = s_upgrade_restart = false;
    s_upgrade_offer_pending = false;
    s_upgrade_job = UPGRADE_JOB_NONE;
    s_upgrade_bar_width = 0;
    upgrade_remember(esp_timer_get_time() / 1000);
    s_upgrade_notice[0] = 0;
    s_page = SETTINGS_UPGRADE;
}
static const char *const TAG = "device_settings";
// 大字号主列表上下排列标签/值；绘制与点击使用同一行高。
// Large-text main rows stack labels/values; drawing and hit testing share the same row height.
static bool settings_stacked(void) { return app_settings_system_font_size() >= 150; }
static int settings_detail_row_height(void) { return settings_stacked()?104:70; }
static int settings_profile_height(void) { return 106; }
static int settings_main_row_height(void) {
    return settings_stacked() ? 128 : 82;
}
#define SETTINGS_ROW_H settings_main_row_height()
#define SETTINGS_WIRELESS_ROWS 7
#if KIIKO_BOARD_METALIO47
#define SETTINGS_DISPLAY_ROWS 8
#else
#define SETTINGS_DISPLAY_ROWS 7
#endif
#define SETTINGS_PERSONAL_ROWS 4
#define SETTINGS_CONTROLS_ROWS 1
#if KIIKO_BOARD_METALIO47
#define SETTINGS_MAINTENANCE_ROWS 7
#else
#define SETTINGS_MAINTENANCE_ROWS 6
#endif
#define SETTINGS_WIRELESS_Y 240
#define SETTINGS_DISPLAY_Y 240
#define SETTINGS_PERSONAL_Y 240
#define SETTINGS_CONTROLS_Y 240
#define SETTINGS_MAINTENANCE_Y 240
#define SETTINGS_PARENT_Y (180 + settings_profile_height() + 28)
#define SETTINGS_PARENT_ROW_H 136
static int settings_menu_scroll_max(void) {
    const int rows[] = {SETTINGS_WIRELESS_ROWS, SETTINGS_DISPLAY_ROWS,
        SETTINGS_PERSONAL_ROWS, SETTINGS_CONTROLS_ROWS, SETTINGS_MAINTENANCE_ROWS};
    int bottom = s_settings_group < 0 ? SETTINGS_PARENT_Y + 5 * SETTINGS_PARENT_ROW_H
        : 240 + rows[s_settings_group] * SETTINGS_ROW_H;
    int limit = bottom - settings_viewport_end() + 12;
    return limit > 0 ? limit : 0;
}
static bool settings_parent_open(int y) {
    if(y<SETTINGS_PARENT_Y)return false;
    int group=(y-SETTINGS_PARENT_Y)/SETTINGS_PARENT_ROW_H;
    if(group>=5 || (y-SETTINGS_PARENT_Y)%SETTINGS_PARENT_ROW_H>=SETTINGS_PARENT_ROW_H-12)return false;
    s_settings_root_scroll=s_main_scroll;s_main_scroll=0;s_settings_group=group;
    return true;
}
static bool settings_parent_back(void) {
    if(s_settings_group<0)return false;
    s_settings_group=-1;s_main_scroll=s_settings_root_scroll;return true;
}
// 设置行图标：32 像素盒，在行高 68 里垂直居中；墨色统一，避免一行一个灰度。
// Setting row icons: a 32 px box centred in the 68 px row with one shared ink level.
#define SETTINGS_UNLOCK_ICON 18
#define SETTINGS_ICON_PX 32
#define SETTINGS_ICON_INK 0x58
// 主页面滚动上限：把最后一组的最后一行刚好推到导航栏之上，多留 8px 余量。
// Main-page scroll limit: just enough to bring the last row of the last group above the nav bar,
// with 8 px to spare.
#define SETTINGS_SCROLL_MAX settings_menu_scroll_max()
static char s_editor[144], s_editor_notice[80];
static ui_text_edit_t s_editor_input;
typedef enum {
    EDITOR_DEVICE_NAME,
    EDITOR_SIGNATURE,
    EDITOR_KOSYNC_URL,
    EDITOR_KOSYNC_USER,
    EDITOR_KOSYNC_PASS,
    EDITOR_OPDS_URL,
    EDITOR_OPDS_USER,
    EDITOR_OPDS_PASS,
} settings_editor_mode_t;
static settings_editor_mode_t s_editor_mode;
static const char *system_font_label(const char *path) {
    if (!path || !path[0]) return "Phông mặc định";
    const char *name = strrchr(path, '/');
    name = name ? name + 1 : path;
    static char stem[TTF_FONT_NAME_MAX];
#if KIIKO_BOARD_METALIO47
    for(int i=0;i<ttf_font_count();++i) {
        const ttf_font_item_t *item=ttf_font_item(i);
        if(item&&!strcmp(item->path,path))return ttf_font_localized_name(item->name);
    }
    if(!font_filename_make(name,stem,sizeof(stem),false))snprintf(stem,sizeof(stem),"Font-%08x",(unsigned)font_filename_hash(name));
#else
    snprintf(stem, sizeof(stem), "%s", name);
    size_t len = strlen(stem);
    if (len >= 4 && !strcasecmp(stem + len - 4, ".ttf")) stem[len - 4] = 0;
#endif
    if (!strcasecmp(stem, "Hei")) return "Phông mặc định (Thẻ nhớ)";
    return ttf_font_localized_name(stem);
}
typedef struct {
    EpdRect size, preview;
    int preview_title, font_title, first, row_height, slots;
} system_font_layout_t;
static system_font_layout_t system_font_layout(void) {
#if KIIKO_BOARD_METALIO47
    // 顶部预览保持紧凑，为100%–200%字号统一留四行字体；文字仍按系统字号绘制。
    // Keep previews compact and reserve four font rows at100%–200%, retaining scaled text.
    system_font_layout_t l = {{36,197,612,118},{36,398,612,154},344,580,640,94,4};
#else
    system_font_layout_t l = settings_stacked()
        ? (system_font_layout_t){{36,197,612,183},{36,471,612,216},413,722,785,142,2}
        : (system_font_layout_t){{36,197,612,100},{36,379,612,154},322,560,615,99,4};
#endif
    l.slots=(settings_viewport_end()-42-l.first)/l.row_height;
    return l;
}
static EpdRect system_font_item_rect(int slot) {
    system_font_layout_t l = system_font_layout();
    return (EpdRect){36,l.first + slot * l.row_height,612,l.row_height - 16};
}
#if KIIKO_BOARD_METALIO47
static EpdRect system_font_page_button(bool next) {
    return (EpdRect){next?504:36,settings_viewport_end()-66,144,48};
}
#endif
static EpdRect system_font_sync_button(bool sync) { return (EpdRect){sync ? 351 : 72,683,261,74}; }
static void draw_system_font_sync(uint8_t *fb) {
    // 仅一个有界弹窗，不额外打开阅读字体。/ One bounded dialog; never load the reading face here.
    settings_card(fb,(EpdRect){48,389,588,401},28,UI_GRAY_WHITE,0x58);
    ui_text_fixed_vc(fb,342,448,32,"Đồng bộ làm phông đọc sách?",EPD_DRAW_ALIGN_CENTER,false);
    char name[80]; snprintf(name,sizeof(name),"Đã chọn '%s'",system_font_label(s_font_sync_path));
    while (ui_text_fixed_width_px(27,name) > 520 && strlen(name) > 6) {
        size_t n = strlen(name) - 1;
        while (n && ((unsigned char)name[n] & 0xc0) == 0x80) --n;
        name[n] = 0;
    }
    ui_text_fixed_vc(fb,342,516,27,name,EPD_DRAW_ALIGN_CENTER,false);
    ui_text_fixed_vc(fb,342,570,24,"Bạn có muốn dùng phông chữ này cho cả nội dung đọc sách không?",EPD_DRAW_ALIGN_CENTER,false);
    ui_text_fixed_vc(fb,342,614,23,s_notice[0]?s_notice:"Nếu không đồng bộ, phông chữ đọc sách hiện tại sẽ giữ nguyên.",EPD_DRAW_ALIGN_CENTER,false);
    for (int i=0;i<2;++i) {
        EpdRect r=system_font_sync_button(i);
        settings_card(fb,r,20,i ? UI_GRAY_BLACK : UI_GRAY_WHITE,0x58);
        ui_text_fixed_vc(fb,r.x+r.width/2,r.y+r.height/2,25,i ? "Đồng bộ phông đọc sách" : "Chỉ phông hệ thống",EPD_DRAW_ALIGN_CENTER,i);
    }
}
static int s_year, s_month, s_day, s_hour, s_minute;
#define WALLPAPER_MAX 64
typedef struct { char path[288]; char name[96]; } wallpaper_item_t;
// 壁纸列表放 PSRAM，理由同上。
// The wallpaper list lives in PSRAM for the same reason.
static wallpaper_item_t *s_wallpapers;

static bool wallpapers_alloc(void) {
    if (!s_wallpapers)
        s_wallpapers = heap_caps_calloc(WALLPAPER_MAX, sizeof(wallpaper_item_t),
                                        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    return s_wallpapers != NULL;
}
static int s_wallpaper_count;
static int s_wallpaper_selected;
static bool s_wallpaper_confirm, s_wallpaper_preview_ok;
static bool s_bookmark_picker;
static uint8_t *s_bookmark_preview;
static uint8_t *s_bookmark_preview_base;

static void wallpaper_scan_dir(const char *root) {
    if (!wallpapers_alloc()) return;
    DIR *dir = opendir(root);
    if (!dir) return;
    struct dirent *entry;
    while (s_wallpaper_count < WALLPAPER_MAX && (entry = readdir(dir))) {
        if (entry->d_name[0] == '.' || !strncmp(entry->d_name, "._", 2)) continue;
        const char *ext = strrchr(entry->d_name, '.');
        if (!ext || (strcasecmp(ext, ".jpg") && strcasecmp(ext, ".jpeg") && strcasecmp(ext, ".png"))) continue;
        if(s_bookmark_picker && strcasecmp(ext,".png"))continue;
        wallpaper_item_t *item = &s_wallpapers[s_wallpaper_count];
        if (snprintf(item->path, sizeof(item->path), "%s/%s", root, entry->d_name) >= sizeof(item->path)) continue;
        struct stat st;
        if (stat(item->path, &st) || !S_ISREG(st.st_mode) || st.st_size <= 0 || (uint64_t)st.st_size > BOOK_IMAGE_FILE_MAX) continue;
        size_t name_len = strnlen(entry->d_name, sizeof(item->name) - 1);
        memcpy(item->name, entry->d_name, name_len);
        item->name[name_len] = 0;
        ++s_wallpaper_count;
    }
    closedir(dir);
}
static void bookmark_preview_release(void) {
    free(s_bookmark_preview);s_bookmark_preview=NULL;
    free(s_bookmark_preview_base);s_bookmark_preview_base=NULL;
}
static void bookmark_preview_prepare(const uint8_t *background) {
    free(s_bookmark_preview);s_bookmark_preview=NULL;
    if(!s_bookmark_picker || s_wallpaper_selected<0 || s_wallpaper_selected>=s_wallpaper_count)return;
    const size_t bytes=684u*1216u/2u;
    if(!background || heap_caps_get_free_size(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT)<(s_bookmark_preview_base?bytes:2*bytes)+2304u*1024u)return;
    // 一次保存目录画面，切换PNG预览时不反复叠加上一张；离开预览即释放。
    // Save the directory frame once so swiping never nests old PNG previews; release on leaving preview.
    if(!s_bookmark_preview_base){
        s_bookmark_preview_base=heap_caps_malloc(bytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
        if(s_bookmark_preview_base)memcpy(s_bookmark_preview_base,background,bytes);
    }
    if(!s_bookmark_preview_base)return;
    uint8_t *canvas=heap_caps_malloc(bytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(canvas)memcpy(canvas,s_bookmark_preview_base,bytes);
    if(canvas && book_lock_bookmark_draw(canvas,s_wallpapers[s_wallpaper_selected].path))s_bookmark_preview=canvas;
    else free(canvas);
}
static void wallpaper_scan(void) {
    s_wallpaper_count = s_wallpaper_page = 0;
    wallpaper_scan_dir("/sdcard/pictures");
    wallpaper_scan_dir("/sdcard/images");
    wallpaper_scan_dir("/sdcard");
}


static void settings_editor_open(settings_editor_mode_t mode) {
    s_editor_mode = mode;
    s_editor_notice[0] = 0;
    const char *val = "";
    bool ascii_only = false;
    switch (mode) {
        case EDITOR_DEVICE_NAME:
            val = app_settings_device_name();
            break;
        case EDITOR_SIGNATURE:
            val = app_settings_status_signature();
            break;
        case EDITOR_KOSYNC_URL:
            val = app_settings_kosync_url();
            ascii_only = true;
            break;
        case EDITOR_KOSYNC_USER:
            val = app_settings_kosync_user();
            ascii_only = true;
            break;
        case EDITOR_KOSYNC_PASS:
            val = "";
            ascii_only = true;
            break;
        case EDITOR_OPDS_URL:
            val = app_settings_opds_url();
            ascii_only = true;
            break;
        case EDITOR_OPDS_USER:
            val = app_settings_opds_user();
            ascii_only = true;
            break;
        case EDITOR_OPDS_PASS:
            val = app_settings_opds_pass();
            ascii_only = true;
            break;
    }
    snprintf(s_editor, sizeof(s_editor), "%s", val ? val : "");
    ui_text_edit_init(&s_editor_input, s_editor, sizeof(s_editor));
    ui_keyboard_begin(&s_editor_input, ascii_only);
    s_page = SETTINGS_TEXT_EDIT;
}

static void profile_editor_open(bool signature) {
    settings_editor_open(signature ? EDITOR_SIGNATURE : EDITOR_DEVICE_NAME);
}




static int days_in_month(int year, int month) {
    static const int days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (month == 2 && (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0))) return 29;
    return days[month - 1];
}

// 公历日期转 UTC 天数；界面统一按北京时间 UTC+8 编辑。
// Convert a Gregorian date to UTC days; the editor uses Beijing time (UTC+8).
static int64_t civil_days(int year, int month, int day) {
    year -= month <= 2;
    int era = year / 400;
    int yoe = year - era * 400;
    int doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
    int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return (int64_t)era * 146097 + doe - 719468;
}

static void time_open(void) {
    time_t now = time(NULL);
    if (now < 1704067200) {
        const pmu_snapshot_t *pmu = read_pico_pmu_get();
        if (pmu && pmu->time_synced && pmu->unix_sec >= 1704067200) now = pmu->unix_sec;
    }
    if (now < 1704067200) {
        char month[4] = "Jan"; int day = 1, year = 2024;
        if (sscanf(__DATE__, "%3s %d %d", month, &day, &year) != 3) {
            strcpy(month, "Jan"); day = 1; year = 2024;
        }
        static const char *const months[] = {"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
        int m = 1; while (m < 12 && strcmp(month, months[m - 1])) ++m;
        s_year = year; s_month = m; s_day = day; s_hour = s_minute = 0;
    } else {
        struct tm date;
        time_t local = now + 7 * 3600;
        gmtime_r(&local, &date);
        s_year = date.tm_year + 1900; s_month = date.tm_mon + 1; s_day = date.tm_mday;
        s_hour = date.tm_hour; s_minute = date.tm_min;
    }
    s_notice[0] = 0;
    s_page = SETTINGS_TIME_EDIT;
}

static void time_adjust(int field, int step) {
    if (field == 0) s_year = s_year + step > 2099 ? 2024 : s_year + step < 2024 ? 2099 : s_year + step;
    if (field == 1) s_month = s_month + step > 12 ? 1 : s_month + step < 1 ? 12 : s_month + step;
    if (field == 2) s_day = s_day + step > days_in_month(s_year, s_month) ? 1 :
                            s_day + step < 1 ? days_in_month(s_year, s_month) : s_day + step;
    if (field == 3) s_hour = (s_hour + step + 24) % 24;
    if (field == 4) s_minute = (s_minute + step + 60) % 60;
    if (s_day > days_in_month(s_year, s_month)) s_day = days_in_month(s_year, s_month);
}

static bool time_save(void) {
    int64_t epoch = civil_days(s_year, s_month, s_day) * 86400 +
                    s_hour * 3600 + s_minute * 60 - 7 * 3600;
    if (epoch < 1704067200 || epoch > 4102444799LL) return false;
    struct timeval tv = {.tv_sec = (time_t)epoch, .tv_usec = 0};
    if (settimeofday(&tv, NULL)) return false;
    uint8_t payload[4] = {(uint8_t)epoch, (uint8_t)(epoch >> 8),
                          (uint8_t)(epoch >> 16), (uint8_t)(epoch >> 24)};
    esp_err_t err = read_pico_pmu_cmd(PMU_CMD_TIME_SYNC, payload, sizeof(payload));
    if (err == ESP_OK) err = read_pico_pmu_cmd(PMU_CMD_TIME_GET, NULL, 0);
    const pmu_snapshot_t *pmu = read_pico_pmu_get();
    if (err != ESP_OK || !pmu || !pmu->time_synced) {
        snprintf(s_notice, sizeof(s_notice), "Đã đặt giờ hệ thống nhưng đồng bộ PMU thất bại");
        return false;
    }
    snprintf(s_notice, sizeof(s_notice), "Đã lưu thời gian và đồng bộ vào chip nguồn PMU");
    s_page = SETTINGS_TIME;
    return true;
}

static void settings_card(uint8_t *fb, EpdRect rect, int radius, uint8_t fill, uint8_t edge) {
    ui_fill_round_rect(fb, rect, radius, fill);
    ui_draw_control_frame(fb, rect, radius, edge);
}
static void settings_divider(uint8_t *fb, int y, int x, int width) {
    ui_draw_separator(fb, y, x, width, 0x60);
}
// 固定提示行限制宽高；主要标签/值另用可伸展布局。/ Bound fixed caption lines; primary labels/values use their own adaptive layout.
static void settings_line(uint8_t *fb, int x, int y, int px, const char *text, enum EpdFontFlags align, bool inverted) {
    int left=36, right=648;
    if (align==EPD_DRAW_ALIGN_LEFT) left=x;
    else if (align==EPD_DRAW_ALIGN_RIGHT) right=x;
    else { int half=x-36; if(648-x<half)half=648-x; left=x-half;right=x+half; }
    ui_text_box(fb,(EpdRect){left,y,right-left,px>=25?52:32},px,text,align,inverted);
}

static void settings_center_line(uint8_t *fb, int x, int cy, int px, const char *text, enum EpdFontFlags align, bool inverted) {
    int height=px>=25?60:40;
    int left=36,right=648;
    if(align==EPD_DRAW_ALIGN_LEFT)left=x;
    else if(align==EPD_DRAW_ALIGN_RIGHT)right=x;
    else {int half=x-36;if(648-x<half)half=648-x;left=x-half;right=x+half;}
    ui_text_box(fb,(EpdRect){left,cy-height/2,right-left,height},px,text,align,inverted);
}
#if KIIKO_BOARD_METALIO47
static EpdRect pin_grace_row(void) { return (EpdRect){36,settings_stacked()?618:606,612,settings_stacked()?120:84}; }
#endif
static void row(uint8_t *fb, int y, const char *label, const char *value) {
    bool stacked=settings_stacked()&&y+104<=settings_viewport_end();
    if (stacked) {
        ui_text_box(fb,(EpdRect){54,y+6,540,48},27,label,EPD_DRAW_ALIGN_LEFT,false);
        ui_text_box(fb,(EpdRect){54,y+58,540,38},21,value,EPD_DRAW_ALIGN_LEFT,false);
    } else {
        ui_text_box(fb,(EpdRect){54,y+10,350,48},27,label,EPD_DRAW_ALIGN_LEFT,false);
        ui_text_box(fb,(EpdRect){422,y+10,180,48},21,value,EPD_DRAW_ALIGN_RIGHT,false);
    }
    settings_divider(fb, y + (stacked?102:68), 54, 575);
}
static void section(uint8_t *fb, int y, const char *title) {
    ui_text_box(fb,(EpdRect){40,y,604,34},22,title,EPD_DRAW_ALIGN_LEFT,false);
}

static const char *ai_action_label(app_ai_action_t action) {
    switch (action) {
        case APP_AI_REFRESH: return "Khử bóng mờ";
        case APP_AI_BOOKMARK: return "Đánh dấu trang";
        case APP_AI_FRONTLIGHT: return "Tắt / Mở đèn nền";
        case APP_AI_HOME: return "Về Trang chủ";
        case APP_AI_KOSYNC: return "Đồng bộ KOSync";
        case APP_AI_PREV_PAGE: return "Trang trước";
        case APP_AI_NEXT_PAGE: return "Trang sau";
        case APP_AI_PREV_CHAPTER: return "Chương trước";
        case APP_AI_NEXT_CHAPTER: return "Chương sau";
        case APP_AI_NONE:
        default: return "Không dùng";
    }
}

static const char *reader_key_action_label(app_reader_key_action_t action) {
    switch (action) {
        case APP_READER_KEY_PREV: return "Trang trước";
        case APP_READER_KEY_NEXT: return "Trang sau";
        case APP_READER_KEY_TOOLS: return "Menu đọc sách";
        case APP_READER_KEY_FULLSCREEN: return "Menu đọc sách";
        case APP_READER_KEY_HOME: return "Về Trang chủ";
        case APP_READER_KEY_REFRESH: return "Khử bóng mờ";
        case APP_READER_KEY_PREV_CHAPTER: return "Chương trước";
        case APP_READER_KEY_NEXT_CHAPTER: return "Chương sau";
        case APP_READER_KEY_LIGHT: return "Tắt / Mở đèn nền";
        case APP_READER_KEY_KOSYNC: return "Đồng bộ KOSync";
        case APP_READER_KEY_NONE:
        default: return "Không dùng";
    }
}

#if KIIKO_BOARD_METALIO47
static void back_header(uint8_t *fb,const char *title);
static int s_control_pick=-1, s_control_key=-1;
static const char *const controls_key_names[]={"Nút AI","Âm lượng +","Âm lượng −","Nút nguồn","Phím giữa","Phím trái","Phím phải"};
static int controls_choice_count(int target) {
    if(target>=9)return APP_READER_KEY_COUNT;
    return target==0?APP_AI_ACTION_COUNT:(target==3||target>=7)?2:APP_READER_KEY_COUNT-1;
}
static int controls_choice_action(int index) {
    return index>=APP_READER_KEY_FULLSCREEN?index+1:index;
}
static int controls_choice_current(int target) {
    if(target>=9)return app_settings_control_hold(target-9);
    switch(target) {
        case 0:return app_settings_ai_short();
        case 1:return app_settings_volume_key_action(1);
        case 2:return app_settings_volume_key_action(0);
        case 3:return app_settings_reader_power_turn();
        case 4:return app_settings_reader_key_action(1);
        case 5:return app_settings_reader_key_action(0);
        case 6:return app_settings_reader_key_action(2);
        case 7:return app_settings_reader_swipe_up_tools();
        case 8:return app_settings_reader_swipe_bottom_home();
        default:return -1;
    }
}
static const char *controls_choice_label(int target,int index) {
    if(target>=9)return index?reader_key_action_label(controls_choice_action(index-1)):"Mặc định hệ thống";
    if(target==0)return ai_action_label(index);
    if(target==3)return index?"Lật trang khi đọc":"Khóa màn hình";
    if(target==7)return index?"Mở menu đọc sách":"Tắt";
    if(target==8)return index?"Về Trang chủ":"Tắt";
    return reader_key_action_label(controls_choice_action(index));
}
static bool controls_choice_save(int target,int index) {
    if(target<0||target>15||index<0||index>=controls_choice_count(target))return false;
    if(target>=9)return app_settings_set_control_hold(target-9,index?controls_choice_action(index-1):APP_CONTROL_HOLD_DEFAULT);
    app_reader_key_action_t action=controls_choice_action(index);
    switch(target) {
        case 0:return app_settings_set_ai_short(index)==ESP_OK;
        case 1:return app_settings_set_volume_key_action(1,action);
        case 2:return app_settings_set_volume_key_action(0,action);
        case 3:app_settings_set_reader_power_turn(index!=0);return true;
        case 4:return app_settings_set_reader_key_action(1,action);
        case 5:return app_settings_set_reader_key_action(0,action);
        case 6:return app_settings_set_reader_key_action(2,action);
        case 7:app_settings_set_reader_swipe_up_tools(index!=0);return true;
        case 8:app_settings_set_reader_swipe_bottom_home(index!=0);return true;
        default:return false;
    }
}
static EpdRect controls_choice_rect(int index) { return (EpdRect){48,248+index*58,588,50}; }
static void controls_row(uint8_t *fb,int y,const char *label,const char *value) {
    settings_card(fb,(EpdRect){36,y,612,80},16,UI_GRAY_WHITE,0x70);
    ui_text_box(fb,(EpdRect){54,y+6,532,32},22,label,EPD_DRAW_ALIGN_LEFT,false);
    ui_text_box(fb,(EpdRect){54,y+42,532,30},20,value,EPD_DRAW_ALIGN_LEFT,false);
    ui_draw_icon(fb,625,y+40,24,UI_ICON_CHEVRON_RIGHT,UI_GRAY_BLACK);
}
static void draw_controls_choices(uint8_t *fb) {
    epd_fill_rect((EpdRect){0,0,684,1088},UI_GRAY_WHITE,fb);
    back_header(fb,"Chọn tác vụ");
    EpdRect panel={24,176,636,888};
    ui_fill_round_rect(fb,panel,20,UI_GRAY_WHITE);
    ui_draw_control_frame(fb,panel,20,0x50);
    ui_text_box(fb,(EpdRect){48,190,588,44},27,"Chọn chức năng",EPD_DRAW_ALIGN_CENTER,false);
    for(int i=0;i<controls_choice_count(s_control_pick);++i){
        int value=s_control_pick>=9?(i?controls_choice_action(i-1):APP_CONTROL_HOLD_DEFAULT):
            s_control_pick==0||s_control_pick==3||s_control_pick>=7?i:controls_choice_action(i);
        bool selected=controls_choice_current(s_control_pick)==value;
        EpdRect r=controls_choice_rect(i);
        ui_fill_round_rect(fb,r,12,selected?0xd8:UI_GRAY_WHITE);
        ui_text_box(fb,(EpdRect){r.x+12,r.y+8,r.width-68,34},24,
                    controls_choice_label(s_control_pick,i),EPD_DRAW_ALIGN_LEFT,false);
        epd_draw_circle(610,r.y+r.height/2,10,0x38,fb);
        if(selected)epd_fill_circle(610,r.y+r.height/2,5,UI_GRAY_BLACK,fb);
    }
    ui_draw_button(fb,(EpdRect){48,944,588,60},"Hủy",false);
    if(s_notice[0])ui_text_box(fb,(EpdRect){48,1016,588,40},18,s_notice,EPD_DRAW_ALIGN_CENTER,false);
}
#endif

/* ---- 蓝牙翻页器子页的版式 / Layout of the Bluetooth sub-page ---- */

// 行高比 setting_group 的 68 略大，因为这一页每行右侧常常要放一个按钮。
// Rows are a little taller than setting_group's 68 because these rows often carry a button.
#define BLE_ROW_H 84
// 按钮右缘与 row() 的值右缘对齐；值文字右端再让出按钮宽度，两者不重叠。
// The button's right edge aligns with row()'s value edge; the value text then stops short of the
// button, so the two never overlap.
#define BLE_BTN_W 118
#define BLE_BTN_RIGHT 624
#define BLE_VALUE_RIGHT (BLE_BTN_RIGHT - BLE_BTN_W - 17)

// 渲染和命中测试都调这个函数，两边看到的是同一份坐标——否则会出现「按钮画在这儿、
// 点在别处」，这一页之前就是这么坏的。
// Rendering and hit testing both call this, so they see identical coordinates. Without it a
// control ends up drawn in one place and tapped in another, which is exactly how this page was
// broken before.
typedef struct {
    int toggle_top;                  // 开关卡片顶；-1 表示被学习提示取代 / switch card top, -1 when the learn prompt replaces it
    int status_top;
    int notice_top;                  // -1 表示没有提示 / -1 when absent
    int bonds_title_top;
    int bond_top[BLE_PT_MAX_BONDS];  // 已配对行顶；-1 表示该位不存在 / bonded row tops, -1 when absent
    int bond_rows;
    int scan_entry_top;              // 「扫描设备 ›」入口行顶 / the "scan devices" entry row
    int learn_title_top;
    int learn_top[2];
    int footer_top;
    int content_bottom;              // 内容底，用于滚动上限 / content bottom, for the scroll limit
} ble_layout_t;

static ble_layout_t ble_layout(void) {
    ble_layout_t l;
    memset(&l, 0, sizeof(l));
    int y = 202 - s_ble_scroll;
    l.toggle_top = s_ble_learning || s_ble_feedback[0] ? -1 : y;
    y += 126 + 38;
    l.status_top = y;
    y += BLE_ROW_H + 20;
    l.notice_top = s_ble_notice[0] ? y : -1;
    if (s_ble_notice[0]) y += 38;
    l.bonds_title_top = y;
    y += 38;
    l.bond_rows = (int)ble_pt_bond_count();
    for (int i = 0; i < BLE_PT_MAX_BONDS; ++i)
        l.bond_top[i] = i < l.bond_rows ? y + i * BLE_ROW_H : -1;
    y += (l.bond_rows ? l.bond_rows : 1) * BLE_ROW_H;
    l.scan_entry_top = y + 16;
    y = l.scan_entry_top + BLE_ROW_H + 30;
    l.learn_title_top = y;
    l.learn_top[0] = y + 38;
    l.learn_top[1] = l.learn_top[0] + BLE_ROW_H;
    l.footer_top = l.learn_top[1] + BLE_ROW_H + 25;
    l.content_bottom = l.footer_top + 66;
    return l;
}

// 一行的左标签 + 让出按钮后的右值，外加分隔线。与 row() 同高，但右端不会压到按钮。
// A row's left label plus a right value that stops short of the button, with the usual divider.
// Same height as row(), but the value never runs under the button.
// 学到的键按整帧显示：帧里非零的字节照原样列出来。同一个键每次按下发的字节一样，所以这串
// 标识每次都一样；不同的键只要有一个字节不同，读出来就不同。
// A learned key shows its whole frame: the non-zero bytes, in order. The same key sends the same
// bytes every press, so this identifier never changes, and two keys that differ in any byte read
// differently.
static void format_bound_key(const ble_pt_frame_t *frame, char *out, size_t size) {
    if (!frame || !frame->len) {
        if (size) out[0] = 0;
        return;
    }
    size_t used = 0;
    unsigned shown = 0;
    for (size_t i = 0; i < frame->len && used + 4 < size; ++i) {
        if (!frame->bytes[i]) continue;
        if (shown == 3) {  // 值列只有 140px：最多列 3 个字节，多的用 + 表示 / the value column holds three
            snprintf(out + used, size - used, " +");
            return;
        }
        const int written = snprintf(out + used, size - used, shown ? " %02X" : "%02X", frame->bytes[i]);
        if (written <= 0) break;
        used += (size_t)written;
        ++shown;
    }
    if (!shown) snprintf(out, size, "00");  // 理论上到不了：全零帧不给绑 / unreachable: a zero frame is never bound
}

static void ble_row(uint8_t *fb, int y, const char *label, const char *value) {
    if (y + BLE_ROW_H < 190 || y >= settings_viewport_end()) return;
    settings_card(fb, (EpdRect){36, y, 612, BLE_ROW_H - 8}, 18, UI_GRAY_WHITE, 0x70);
    ui_text_box(fb,(EpdRect){60,y+8,284,60},26,label,EPD_DRAW_ALIGN_LEFT,false);
    if(value&&value[0])ui_text_box(fb,(EpdRect){362,y+8,BLE_VALUE_RIGHT-362,60},21,value,EPD_DRAW_ALIGN_RIGHT,false);
}

// 行右侧的按钮。竖直居中于该行，右缘与值文字对齐。
// The button on a row's right: vertically centred in the row, right edge aligned with the value.
static EpdRect ble_button_rect(int row_top) {
    return (EpdRect){BLE_BTN_RIGHT - BLE_BTN_W, row_top + 10, BLE_BTN_W, 56};
}

// 三级页：进页即持续扫描，选中一台连上就退回二级页。列表长了也不影响二级页。
// Third-level page: scanning starts on entry and runs continuously; picking a device connects
// and returns to level two. A long list never affects the page above it.
typedef struct {
    int devices_top;
    int device_rows;
    int status_top;
    int content_bottom;
} ble_scan_layout_t;

// 扫描列表只列真的广播了名字的设备。没有广播名时组件把地址回落成名字，那一行对用户
// 认不出是什么设备，而且地址太长会把信号强度挤掉。
// The scan list shows only devices that actually advertised a name. Without one the component
// falls back to the address: nothing the user can recognise, and long enough to crowd the signal
// strength off the row.
static bool ble_scan_visible(const ble_pt_device_t *dev) {
    return dev != NULL && dev->has_name;
}

static int ble_scan_visible_count(void) {
    int visible = 0;
    const uint8_t count = ble_pt_device_count();
    for (uint8_t i = 0; i < count; ++i) {
        if (ble_scan_visible(ble_pt_device(i))) ++visible;
    }
    return visible;
}

// 取第 row 台有名字的设备；遍历顺序仍是组件的原始顺序。
// The row-th device that has a name; the walk keeps the component's order.
static const ble_pt_device_t *ble_scan_visible_at(int row) {
    if (row < 0) return NULL;
    const uint8_t count = ble_pt_device_count();
    int seen = 0;
    for (uint8_t i = 0; i < count; ++i) {
        const ble_pt_device_t *dev = ble_pt_device(i);
        if (!ble_scan_visible(dev)) continue;
        if (seen++ == row) return dev;
    }
    return NULL;
}

static ble_scan_layout_t ble_scan_layout(void) {
    ble_scan_layout_t l;
    memset(&l, 0, sizeof(l));
    l.status_top = 200;
    l.devices_top = 300;
    l.device_rows = ble_scan_visible_count();
    l.content_bottom = l.devices_top + (l.device_rows ? l.device_rows * BLE_ROW_H : 68) + 40;
    return l;
}
static void back_header(uint8_t *fb, const char *title) {
    ui_nav_back(fb, 36, 79);
    int width=s_page==SETTINGS_TEXT_EDIT?414:468;
#if KIIKO_BOARD_METALIO47
    if(s_page==SETTINGS_SYSTEM_FONT)width=368;
#endif
    // 左侧保留Back命中区，右侧保留完成/在线字体入口。/ Reserve Back on the left and Done/online-font actions on the right.
    ui_text_box(fb,(EpdRect){108,70,width,88},34,title,EPD_DRAW_ALIGN_CENTER,false);
    settings_divider(fb, 174, 36, 612);
}

static void setting_icon(uint8_t *fb, int index, int cx, int cy) {
    // 每个设置项对应一个 Lucide 图标；索引顺序由各分组传入的 icons[] 数组决定。
    // One Lucide icon per setting row; the index order comes from each group's icons[] array.
    static const ui_icon_t icons[] = {
        UI_ICON_WIFI,              // 0 无线连接
        UI_ICON_BLUETOOTH,         // 1 蓝牙
        UI_ICON_TYPE,              // 2 系统字体、阅读操作
        UI_ICON_A_LARGE_SMALL,     // 3 系统字号
        UI_ICON_LIBRARY_BIG,       // 4 书架样式
        UI_ICON_TICKET,            // 5 锁屏样式
        UI_ICON_CLOCK,             // 6 日期与时间
        UI_ICON_CONTRAST,          // 7 系统对比度
        UI_ICON_COPY,              // 8 保存与恢复
        UI_ICON_POWER,             // 9 关机睡眠
        UI_ICON_TEXT_ALIGN_START,  // 10 状态栏签名
        UI_ICON_REFRESH_CW,        // 11 首页强刷
        UI_ICON_DOWNLOAD,          // 12 系统升级
        UI_ICON_CPU,               // 13 BOOT 刷机
        UI_ICON_EYE_CLOSED,        // 14 自动休眠
        UI_ICON_SLIDERS_HORIZONTAL, // 15 主页刷新模式 / Main refresh mode
        UI_ICON_LOCK,              // 16 锁屏密码 / lock screen password
        UI_ICON_ROTATE_CW,         // 17 屏幕修复 / panel repair
        UI_ICON_LOCK,              // 18 双击解锁 / double-press unlock
    };
    if (index < 0 || index >= (int)(sizeof(icons) / sizeof(icons[0]))) return;
    ui_draw_icon(fb, cx, cy, SETTINGS_ICON_PX, icons[index], SETTINGS_ICON_INK);
}

static void settings_fit_text(char *value, int px, int width) {
    while (value[0] && ui_text_fixed_width_px(ui_text_effective_px(px),value)>width) {
        size_t n=strlen(value)-1;while(n&&((unsigned char)value[n]&0xc0)==0x80)--n;value[n]=0;
    }
}
static void fit_value(char *value, int width) { settings_fit_text(value,20,width); }

static void settings_switch(uint8_t *fb, int center_y, bool active) {
    // 灰色轨道在滚动差分扫描下会经过深色相位产生黑闪；纯黑白无灰阶过渡，任何刷新模式都稳定。
    // A gray track transits dark phases under the scroll's differential scan and flashes
    // black; pure black/white holds clean in every refresh mode.
    EpdRect track = {555, center_y - 19, 69, 39};
    if (active) ui_fill_round_rect(fb, track, 19, UI_GRAY_BLACK);
    else {
        ui_fill_round_rect(fb, track, 19, UI_GRAY_WHITE);
        ui_draw_round_rect(fb, track, 19, 0x30);
    }
    int cx = active ? track.x + 49 : track.x + 20;
    epd_fill_circle(cx, track.y + 19, 16, UI_GRAY_WHITE, fb);
    epd_draw_circle(cx, track.y + 19, 16, 0x30, fb);
}

static void setting_group(uint8_t *fb, int group, int title_y, const char *title,
                          int card_y, const int icons[], const char *const labels[],
                          const char *const values[], int count) {
    if (s_settings_group != group) return;
    const int row_height = SETTINGS_ROW_H;
    title_y -= s_main_scroll;
    card_y -= s_main_scroll;
    // 屏外的行不测字、不生成字形；保留卡片边界与原滚动坐标。
    // Skip off-screen glyph work while preserving card boundaries and scroll coordinates.
    if (card_y + count * row_height <= 160 || title_y >= settings_viewport_end()) return;
    if (title_y + ui_text_effective_px(20) > 160 && title_y < settings_viewport_end())
        settings_line(fb, 42, title_y, 20, s_notice[0]?s_notice:title, EPD_DRAW_ALIGN_LEFT, false);
    settings_card(fb, (EpdRect){36, card_y, 612, count * row_height}, 22,
                  UI_GRAY_WHITE, 0x70);
    for (int i = 0; i < count; ++i) {
        int y = card_y + i * row_height;
        if (y + row_height <= 160 || y >= settings_viewport_end()) continue;
        if (i) settings_divider(fb, y, 88, 544);
        setting_icon(fb, icons[i], 67, y + row_height / 2);
        if (icons[i] == SETTINGS_UNLOCK_ICON) {
            ui_text_box(fb,(EpdRect){100,y+8,438,row_height-16},25,labels[i],EPD_DRAW_ALIGN_LEFT,false);
            settings_switch(fb, y + row_height / 2, app_settings_wake_presses() == 2);
            continue;
        }
        if(settings_stacked()) {
            ui_text_box(fb,(EpdRect){100,y+8,498,56},25,labels[i],EPD_DRAW_ALIGN_LEFT,false);
            ui_text_box(fb,(EpdRect){100,y+72,494,42},20,values[i],EPD_DRAW_ALIGN_LEFT,false);
        } else {
            ui_text_box(fb,(EpdRect){100,y+10,328,row_height-20},25,labels[i],EPD_DRAW_ALIGN_LEFT,false);
            ui_text_box(fb,(EpdRect){446,y+10,148,row_height-20},20,values[i],EPD_DRAW_ALIGN_RIGHT,false);
        }
        // 导航是图标，不依赖用户字体是否包含 › 字形，也不会被截短掉。
        // Navigation is an icon, independent of the font cmap and value truncation.
        ui_draw_icon(fb,625,y+row_height/2,26,UI_ICON_CHEVRON_RIGHT,UI_GRAY_BLACK);
    }
}

static void controls_edge_toggle(uint8_t *fb,int y,const char *title,bool on) {
    settings_card(fb,(EpdRect){36,y,612,80},16,UI_GRAY_WHITE,0x70);
    ui_text_box(fb,(EpdRect){54,y+8,490,64},22,title,EPD_DRAW_ALIGN_LEFT,false);
    settings_switch(fb,y+40,on);
}

static void setting_toggle(uint8_t *fb, int y, const char *title, const char *detail, bool active) {
    settings_card(fb, (EpdRect){36, y, 612, 126}, 22, UI_GRAY_WHITE, 0x70);
    ui_text_box(fb,(EpdRect){59,y+10,480,48},27,title,EPD_DRAW_ALIGN_LEFT,false);
    ui_text_box(fb,(EpdRect){59,y+70,480,42},19,detail,EPD_DRAW_ALIGN_LEFT,false);
    settings_switch(fb, y + 62, active);
}

static void draw_style_thumbnail(uint8_t *fb, int style, int top) {
    if (top < 242 || top + 170 >= settings_viewport_end()) return;
    if (style == 5) {
        ui_fill_round_rect(fb, (EpdRect){68,top,548,170}, 12, UI_GRAY_WHITE);
        for (int i=0;i<3;++i) {
            int y=top+9+i*52;
            ui_fill_rect_fast((EpdRect){86,y,31,43},i==1?0x80:0x40,fb);
            ui_fill_rect_fast((EpdRect){137,y+6,218,3},0,fb);
            ui_fill_rect_fast((EpdRect){137,y+19,84,2},0x70,fb);
            ui_fill_rect_fast((EpdRect){137,y+34,435,4},0xd0,fb);
            ui_fill_rect_fast((EpdRect){137,y+34,120+i*75,4},0x40,fb);
        }
        return;
    }
    const int left = 99;
    for (int i = 0; i < 3; ++i) {
        int x = left + 20 + i * 155;
        EpdRect cover = {x, top, 116, 145};
        ui_fill_rect_fast(cover, i == 1 ? 0x60 : i == 2 ? 0xa0 : 0x40, fb);
        ui_draw_round_rect(fb, cover, 1, 0x30);
        ui_draw_round_rect(fb, (EpdRect){x + 7, top + 7, 102, 130}, 1, i == 1 ? 0xc0 : 0xe0);
        ui_hairline(fb, top + 47, x + 28, 60, i == 1 ? 0xc0 : 0xe0);
        ui_hairline(fb, top + 54, x + 39, 38, i == 1 ? 0xc0 : 0xe0);
        ui_hairline(fb, top + 111, x + 44, 28, i == 1 ? 0xc0 : 0xe0);
    }
    if (style == 1) {
        ui_fill_rect_fast((EpdRect){left, top + 143, 486, 16}, 0x30, fb);
        ui_hairline(fb, top + 143, left, 486, 0x80);
    } else if (style == 2) {
        ui_draw_acrylic_guard(fb, (EpdRect){left, top + 94, 486, 70});

    }
}

// 仅 82x82 灰阶头像，不缓存整屏或第二份字库；文件变更/拔卡会使缓存失效。
// One 82x82 gray avatar, never a full frame or second font; file changes/removal invalidate it.
static EXT_RAM_BSS_ATTR uint8_t s_avatar_pixels[82*82];
static char s_avatar_path[288];
static bool s_avatar_ready, s_avatar_pending;
static char s_main_wifi[33];
static bool s_main_wifi_saved;
static void render_content(app_ctx_t *ctx, uint8_t *fb) {
#if KIIKO_BOARD_METALIO47
    if(s_page==SETTINGS_ONLINE_FONTS){ui_online_fonts_draw(&s_online_fonts,fb);return;}
    if(s_page==SETTINGS_FRONTLIGHT) {
        epd_fill_rect((EpdRect){0,0,684,1216},UI_GRAY_WHITE,fb); back_header(fb,"Đèn nền & Rung");
        char value[48]; snprintf(value,sizeof(value),"%u%%",metalio_brightness());
        row(fb,300,"Độ sáng",value); row(fb,390,"Giảm 10%","Tăng 10%");
        snprintf(value,sizeof(value),"%u%%",metalio_warmth()); row(fb,530,"Tỷ lệ ánh sáng ấm",value);
        row(fb,620,"Ánh sáng lạnh","Ánh sáng ấm"); row(fb,750,"Đèn nền",metalio_frontlight_enabled()?"Bật":"Tắt");
        row(fb,850,"Rung phản hồi chạm",metalio_haptic_enabled()?"Bật":"Tắt");
        settings_navigation(fb); return;
    }
    if(s_page==SETTINGS_CONTROLS) {
        epd_fill_rect((EpdRect){0,0,684,1216},UI_GRAY_WHITE,fb);
        back_header(fb,s_control_key<0?"Phím cứng & Cử chỉ":controls_key_names[s_control_key]);
        if(s_control_key>=0) {
            int key=s_control_key;
            // Current choices are rendered directly; action indices omit the retired slot.
            const char *short_label=key==0?ai_action_label(app_settings_ai_short()):
                key==3?(app_settings_reader_power_turn()?"Lật trang khi đọc":"Khóa màn hình"):
                reader_key_action_label(controls_choice_current(key));
            controls_row(fb,250,"Nhấn ngắn",short_label);
            uint8_t held=app_settings_control_hold(key);
            controls_row(fb,390,"Nhấn giữ",held==APP_CONTROL_HOLD_DEFAULT?"Mặc định hệ thống":reader_key_action_label(held));
            ui_text_box(fb,(EpdRect){54,550,576,120},20,
                "Giữ 500 ms; nút nguồn 700 ms. Phím mặt trước áp dụng khi đọc sách. Giữ không chạy thêm nhấn ngắn.",EPD_DRAW_ALIGN_LEFT,false);
        } else {
            for(int i=0;i<7;++i) {
                int y=218+i*70;
                settings_card(fb,(EpdRect){36,y,612,64},16,UI_GRAY_WHITE,0x70);
                ui_text_box(fb,(EpdRect){60,y+6,528,52},25,controls_key_names[i],EPD_DRAW_ALIGN_LEFT,false);
                ui_draw_icon(fb,625,y+32,26,UI_ICON_CHEVRON_RIGHT,UI_GRAY_BLACK);
            }
            section(fb,726,"Cử chỉ khi đọc sách");
            controls_row(fb,770,"Vuốt lên",app_settings_reader_swipe_up_tools()?"Bật menu đọc sách":"Tắt");
            controls_row(fb,856,"Vuốt từ cạnh đáy lên",app_settings_reader_swipe_bottom_home()?"Về Trang chủ":"Tắt");
            controls_edge_toggle(fb,942,"Cạnh trái: tăng/giảm độ sáng",app_settings_reader_edge_brightness());
            controls_edge_toggle(fb,1028,"Cạnh phải: tăng/giảm đèn vàng",app_settings_reader_edge_warmth());
        }
        settings_navigation(fb);
        if(s_control_pick>=0)draw_controls_choices(fb);
        return;
    }

#else
#endif
    (void)ctx;
    if (s_page == SETTINGS_PIN_ENTRY) { ui_pinpad_paint(fb, &s_pinpad, ui_pinpad_full()); return; }
    ui_clear_page(fb);
    if (s_page == SETTINGS_FACTORY_RESET) {
        ui_nav_status(fb);
        back_header(fb,"Khôi phục cài đặt gốc");
        ui_text_box(fb,(EpdRect){54,260,576,170},26,
            "Đặt lại tùy chọn hệ thống, hồ sơ, WiFi, Bluetooth và mật khẩu khóa.",EPD_DRAW_ALIGN_LEFT,false);
        ui_text_box(fb,(EpdRect){54,450,576,170},23,
            "Giữ nguyên sách, phông chữ, ảnh, lịch sử đọc và thông số VCOM.",EPD_DRAW_ALIGN_LEFT,false);
        if(s_reset_confirm)ui_text_box(fb,(EpdRect){54,645,576,80},23,"Sau khi xác nhận sẽ khôi phục cài đặt mặc định và khởi động lại.",EPD_DRAW_ALIGN_LEFT,false);
        if(s_reset_notice[0])ui_text_box(fb,(EpdRect){54,735,576,90},21,s_reset_notice,EPD_DRAW_ALIGN_LEFT,false);
        ui_draw_button(fb,(EpdRect){54,850,576,78},s_reset_pending?"Đang khôi phục…":s_reset_confirm?"Khôi phục & Khởi động lại":"Khôi phục cài đặt gốc",true);
        ui_draw_button(fb,(EpdRect){54,950,576,78},"Hủy",false);
        settings_navigation(fb);return;
    }

    // 保留 ui_clear_page 的白底，灰阶只由头像、卡片与字形生成。
    // Keep ui_clear_page's white background; avatars, cards and glyphs alone generate grays.
    ui_nav_status(fb);
#if KIIKO_BOARD_METALIO47
    if (s_page == SETTINGS_SD_RECOVERY) {
        back_header(fb,"Khôi phục thẻ nhớ");
        ui_text_fixed(fb,54,270,26,"Quét lại thẻ nhớ và danh sách phông chữ",EPD_DRAW_ALIGN_LEFT,false);
        ui_text_fixed(fb,54,325,23,"Thử hạ xung nhịp và chế độ 1-bit để tăng tương thích thẻ nhớ",EPD_DRAW_ALIGN_LEFT,false);
        ui_text_fixed(fb,54,380,23,"Không định dạng thẻ, không xóa sách và cài đặt",EPD_DRAW_ALIGN_LEFT,false);
        app_sd_recovery_status_t status=app_sd_recovery_status();
        const char *message="Dùng khi thẻ nhớ chưa nhận hoặc chưa hiện phông chữ mới";
        switch(status.phase) {
            case APP_SD_RECOVERY_PREPARE: message="Đang giải phóng tài nguyên thẻ nhớ…";break;
            case APP_SD_RECOVERY_WAIT: message="Đang chờ tác vụ bộ nhớ hoàn tất…";break;
            case APP_SD_RECOVERY_MOUNT: message="Đang gắn lại thẻ nhớ, vui lòng không rút thẻ…";break;
            case APP_SD_RECOVERY_FONTS: message="Đã gắn thẻ, đang nhận diện lại phông chữ…";break;
            case APP_SD_RECOVERY_DONE: message="Đã gắn lại thẻ nhớ thành công";break;
            case APP_SD_RECOVERY_FAILED: message="Khôi phục chưa hoàn tất, hãy thử lại sau";break;
            default: break;
        }
        ui_text_fixed(fb,54,510,25,message,EPD_DRAW_ALIGN_LEFT,false);
        if(status.phase==APP_SD_RECOVERY_DONE) {
            char fonts[64];snprintf(fonts,sizeof(fonts),"Đã nhận diện %d phông chữ ngoài",status.fonts);
            ui_text_fixed(fb,54,575,24,fonts,EPD_DRAW_ALIGN_LEFT,false);
            if(!status.selected_font_ready)
                ui_text_fixed(fb,54,635,22,"Không tải được phông đã chọn, vui lòng chọn lại",EPD_DRAW_ALIGN_LEFT,false);
        } else if(status.phase==APP_SD_RECOVERY_FAILED) {
            ui_text_fixed(fb,54,575,21,esp_err_to_name(status.error),EPD_DRAW_ALIGN_LEFT,false);
            ui_text_fixed(fb,54,635,22,"Hãy kiểm tra xem thẻ nhớ đã lắp chặt chưa hoặc thử thẻ khác",EPD_DRAW_ALIGN_LEFT,false);
        }
        if(s_sd_notice[0])ui_text_box(fb,(EpdRect){54,710,576,110},22,s_sd_notice,EPD_DRAW_ALIGN_LEFT,false);
        ui_draw_button(fb,sd_recovery_button(),app_sd_recovery_busy()?"Đang khôi phục…":"Bắt đầu khôi phục",false);
        settings_navigation(fb);return;
    }
#endif
    if (s_page == SETTINGS_LOCK_PIN) {
        back_header(fb, "Mật khẩu màn hình khóa");
        row(fb, 266, "Mật khẩu 4 chữ số", !lock_pin_available() ? "Tạm không khả dụng" : lock_pin_enabled() ? "Đã bật" : "Đã tắt");
        if (!lock_pin_available()) settings_center_line(fb, 342, 408, 24, "Cài đặt màn hình khóa tạm thời không khả dụng", EPD_DRAW_ALIGN_CENTER, false);
        else if (lock_pin_enabled()) {
            row(fb, 406, "Đổi mật khẩu", "Xác minh mật khẩu cũ  ›");
            row(fb, 512, "Tắt mật khẩu", "Xác minh mật khẩu cũ  ›");
#if KIIKO_BOARD_METALIO47
            const unsigned grace=app_settings_pin_grace_code();
            const char *text=grace==1?"5 phút":grace==2?"1 giờ":grace==3?"8 giờ":grace==4?"24 giờ":"Tắt";
            row(fb,pin_grace_row().y+12,"Thời gian chờ khóa lại",text);
#endif
        } else row(fb, 406, "Bật mật khẩu", "Đặt mật khẩu 4 số  ›");
#if KIIKO_BOARD_METALIO47
        if(s_notice[0])settings_center_line(fb,342,780,23,s_notice,EPD_DRAW_ALIGN_CENTER,false);
        settings_center_line(fb,342,850,21,"Cần xác minh mật khẩu hiện tại khi đổi hoặc tắt mật khẩu",EPD_DRAW_ALIGN_CENTER,false);
#else
        if (s_notice[0]) settings_center_line(fb, 342, 680, 23, s_notice, EPD_DRAW_ALIGN_CENTER, false);
        settings_center_line(fb, 342, 752, 21, "Cần xác minh mật khẩu hiện tại khi đổi hoặc tắt mật khẩu", EPD_DRAW_ALIGN_CENTER, false);
#endif
        settings_navigation(fb); return;
    }
    if (s_page == SETTINGS_PROFILE) {
        back_header(fb, "Hồ sơ cá nhân");
        settings_line(fb, 42, 214, 21, "Tùy chỉnh tên thiết bị & ảnh đại diện", EPD_DRAW_ALIGN_LEFT, false);
        row(fb, 266, "Tên thiết bị", app_settings_device_name());
        row(fb, 366, "Đổi ảnh đại diện", app_settings_avatar_path()[0] ? "Đã chọn ảnh  ›" : "Biểu tượng mặc định  ›");
        settings_navigation(fb);
        return;
    }
    if (s_page == SETTINGS_AVATAR) {
        back_header(fb, "Chọn ảnh đại diện");
        section(fb, 232, "Ảnh JPG / PNG trong thư mục pictures trên thẻ nhớ");
        row(fb, 270, "Dùng biểu tượng mặc định", app_settings_avatar_path()[0] ? "Chọn  ›" : "Đang dùng  ✓");
        if (!s_wallpaper_count) settings_line(fb, 52, 384, 23, "Không tìm thấy ảnh, hãy chép vào thư mục pictures", EPD_DRAW_ALIGN_LEFT, false);
        for (int i = 0; i < 7; ++i) {
            int index = s_wallpaper_page * 7 + i;
            if (index >= s_wallpaper_count) break;
            EpdRect box = {36, 374 + i * 96, 612, 80};
            bool active = !strcmp(s_wallpapers[index].path, app_settings_avatar_path());
            settings_card(fb, box, 17, active ? 0xd0 : UI_GRAY_WHITE, 0x70);
            char name[96]; snprintf(name, sizeof(name), "%s", s_wallpapers[index].name);
            fit_value(name, 490);
            ui_text_box(fb,(EpdRect){60,box.y+8,box.width-100,box.height-16},24,name,EPD_DRAW_ALIGN_LEFT,false);
            if (active) epd_fill_circle(611, box.y + 40, 7, UI_GRAY_BLACK, fb);
        }
        settings_navigation(fb);
        return;
    }
    if (s_page == SETTINGS_TEXT_EDIT) {
        const char *title = "Chỉnh sửa";
        const char *desc = "";
        switch (s_editor_mode) {
            case EDITOR_DEVICE_NAME:
                title = "Tên thiết bị";
                desc = "Hiển thị trên thẻ hồ sơ trong phần Cài đặt";
                break;
            case EDITOR_SIGNATURE:
                title = "Chữ ký thanh trạng thái";
                desc = "Hiển thị ở giữa thanh trạng thái, để trống sẽ ẩn";
                break;
            case EDITOR_KOSYNC_URL:
                title = "Máy chủ KOSync";
                desc = "Địa chỉ máy chủ (ví dụ: https://sync.vreader.cloud)";
                break;
            case EDITOR_KOSYNC_USER:
                title = "Tài khoản KOSync";
                desc = "Tên đăng nhập tài khoản đồng bộ KOReader";
                break;
            case EDITOR_KOSYNC_PASS:
                title = "Mật khẩu KOSync";
                desc = "Mật khẩu sẽ được tự động băm MD5 chuẩn KOReader";
                break;
            case EDITOR_OPDS_URL:
                title = "Địa chỉ kho sách OPDS";
                desc = "URL danh mục sách Calibre-Web hoặc chuẩn OPDS";
                break;
            case EDITOR_OPDS_USER:
                title = "Tài khoản OPDS";
                desc = "Tên đăng nhập thư viện OPDS (để trống nếu không cần)";
                break;
            case EDITOR_OPDS_PASS:
                title = "Mật khẩu OPDS";
                desc = "Mật khẩu truy cập thư viện OPDS";
                break;
        }
        back_header(fb, title);
        ui_text_box(fb,(EpdRect){542,79,106,70},24,"Xong",EPD_DRAW_ALIGN_RIGHT,false);
        settings_line(fb, 36, 201, 21, desc, EPD_DRAW_ALIGN_LEFT, false);
        ui_text_input_draw(fb, &s_editor_input, (EpdRect){36, 243, 612, 82}, 28, false, NULL);
        if (s_editor_notice[0]) ui_text_fixed_vc(fb, 36, 510, 22, s_editor_notice, EPD_DRAW_ALIGN_LEFT, false);
        ui_keyboard_draw(fb, UI_SYSTEM_KEYBOARD_TOP);
        settings_navigation(fb);
        return;
    }
    if (s_page == SETTINGS_SHELF_STYLE) {
        back_header(fb,"Kiểu hiển thị Tủ sách");
        settings_line(fb, 36, 207, 23, "Chọn giao diện và cách sắp xếp Tủ sách", EPD_DRAW_ALIGN_LEFT, false);
        static const char *const styles[] = {"Thanh kệ tối màu", "Kệ sách Acrylic", "Danh sách"};
        static const char *const descriptions[] = {"9 cuốn mỗi trang", "9 cuốn mỗi trang", "Tác giả & tiến độ · 4 cuốn mỗi trang"};
        for (int i = 0; i < 3; ++i) {
            int y = 263 + i * 253 - s_style_scroll;
            if (y + 230 < 242 || y >= settings_viewport_end()) continue;
            EpdRect card = {36, y, 612, 230};
            int style = i == 2 ? 5 : i + 1;
            bool active = app_settings_shelf_style() == style;
            settings_card(fb, card, 24, active ? 0xd0 : UI_GRAY_WHITE,
                          active ? 0x58 : 0x70);
            draw_style_thumbnail(fb, style, y + 11);
            if (y + 190 < settings_viewport_end()) ui_text_box(fb,(EpdRect){68,y+186,218,38},23,styles[i],EPD_DRAW_ALIGN_LEFT,false);
            if (y + 195 < settings_viewport_end()) ui_text_box(fb,(EpdRect){310,y+186,266,38},17,descriptions[i],EPD_DRAW_ALIGN_LEFT,false);
            if (y + 220 < settings_viewport_end()) {
                epd_draw_circle(609, y + 207, 13, UI_GRAY_BLACK, fb);
                if (active) epd_fill_circle(609, y + 207, 6, UI_GRAY_BLACK, fb);
            }
        }
        settings_navigation(fb);
        return;
    }
    if (s_page == SETTINGS_TIME_EDIT) {
        back_header(fb,"Ngày & Giờ");
        settings_line(fb, 36, 216, 23, "Giờ Việt Nam · GMT+7", EPD_DRAW_ALIGN_LEFT, false);
        ui_hairline(fb, 259, 36, 612, UI_GRAY_LIGHT);
        static const char *const labels[] = {"Năm", "Thg", "Ngày", "Giờ", "Phút"};
        int values[] = {s_year, s_month, s_day, s_hour, s_minute};
        for (int i = 0; i < 5; ++i) {
            int x = 36 + i * 122;
            ui_text_box(fb,(EpdRect){x,292,106,48},24,labels[i],EPD_DRAW_ALIGN_CENTER,false);
            ui_draw_control_frame(fb, (EpdRect){x, 354, 106, 62}, 8, UI_GRAY_BLACK);
            settings_center_line(fb, x + 53, 385, 30, "+", EPD_DRAW_ALIGN_CENTER, false);
            char value[8]; snprintf(value, sizeof(value), i == 0 ? "%04d" : "%02d", values[i]);
            ui_text_box(fb,(EpdRect){x,440,106,72},i==0?34:39,value,EPD_DRAW_ALIGN_CENTER,false);
            ui_draw_control_frame(fb, (EpdRect){x, 535, 106, 62}, 8, UI_GRAY_BLACK);
            settings_center_line(fb, x + 53, 566, 30, "−", EPD_DRAW_ALIGN_CENTER, false);
        }
        settings_line(fb, 36, 649, 23, "Chạm dấu + phía trên hoặc − phía dưới để chỉnh số", EPD_DRAW_ALIGN_LEFT, false);
        ui_draw_button(fb, (EpdRect){36, 767, 612, 78}, "Lưu thời gian", false);
        ui_draw_button(fb, (EpdRect){36, 874, 612, 78}, "Hủy", false);
        if (s_notice[0]) settings_line(fb, 36, 996, 22, s_notice, EPD_DRAW_ALIGN_LEFT, false);
        return;
    }
    if (s_page == SETTINGS_WIFI) {
        back_header(fb, "WiFi");
        section(fb, 253, "Đã lưu mạng");
        row(fb, 296, s_main_wifi_saved ? s_main_wifi : "Chưa cấu hình", s_main_wifi_saved ? "Đã lưu" : "");
        row(fb, 408, "Kết nối WiFi, truyền sách & cấu hình KOSync/OPDS", "Mở Truyền dữ liệu  ›");
        settings_line(fb, 54, 529, 21, "Sau khi kết nối WiFi có thể đồng bộ giờ tự động GMT+7.", EPD_DRAW_ALIGN_LEFT, false);
        settings_navigation(fb);
        return;
    }
    if (s_page == SETTINGS_WIFI_TIMEOUT) {
        back_header(fb, "Tự tắt Wi-Fi");
        section(fb, 228, "Tự động tắt Wi-Fi khi không truyền tệp");
        const char *labels[] = {"1 phút", "3 phút", "5 phút", "10 phút", "Tắt (Không tự tắt)"};
        const uint8_t values[] = {1, 3, 5, 10, 0};
        for (int i = 0; i < 5; ++i) {
            EpdRect box = {36, 272 + i * 94, 612, 78};
            bool active = app_settings_wifi_timeout_minutes() == values[i];
            settings_card(fb, box, 18, active ? 0xd0 : UI_GRAY_WHITE, 0x70);
            ui_text_box(fb,(EpdRect){64,box.y+8,516,box.height-16},26,labels[i],EPD_DRAW_ALIGN_LEFT,false);
            if (active) epd_fill_circle(609, box.y + box.height / 2, 8, UI_GRAY_BLACK, fb);
        }
        settings_line(fb, 48, 770, 21, "Tự động ngắt kết nối và tắt Wi-Fi khi rảnh rỗi", EPD_DRAW_ALIGN_LEFT, false);
        settings_line(fb, 48, 804, 21, "để tiết kiệm pin và giải phóng RAM hệ thống.", EPD_DRAW_ALIGN_LEFT, false);
        settings_navigation(fb);
        return;
    }
    if (s_page == SETTINGS_BLE_TIMEOUT) {
        back_header(fb, "Tự tắt Bluetooth");
        section(fb, 228, "Tự động tắt Bluetooth khi không dùng");
        const char *labels[] = {"5 phút", "10 phút", "15 phút", "30 phút", "Tắt (Không tự tắt)"};
        const uint8_t values[] = {5, 10, 15, 30, 0};
        for (int i = 0; i < 5; ++i) {
            EpdRect box = {36, 272 + i * 94, 612, 78};
            bool active = app_settings_ble_timeout_minutes() == values[i];
            settings_card(fb, box, 18, active ? 0xd0 : UI_GRAY_WHITE, 0x70);
            ui_text_box(fb,(EpdRect){64,box.y+8,516,box.height-16},26,labels[i],EPD_DRAW_ALIGN_LEFT,false);
            if (active) epd_fill_circle(609, box.y + box.height / 2, 8, UI_GRAY_BLACK, fb);
        }
        settings_line(fb, 48, 770, 21, "Tự động tắt Bluetooth khi mất kết nối hoặc", EPD_DRAW_ALIGN_LEFT, false);
        settings_line(fb, 48, 804, 21, "không bấm phím lật trang để tiết kiệm pin và RAM.", EPD_DRAW_ALIGN_LEFT, false);
        settings_navigation(fb);
        return;
    }
    if (s_page == SETTINGS_SYSTEM_FONT) {
        back_header(fb, "Phông chữ hệ thống");
#if KIIKO_BOARD_METALIO47
        ui_online_fonts_draw_entry(fb,79);
#endif
        system_font_layout_t l = system_font_layout();
        settings_card(fb,l.size,22,UI_GRAY_WHITE,0x70);
        char size[16]; snprintf(size,sizeof(size),"%u%%",app_settings_system_font_size());
#if KIIKO_BOARD_METALIO47
        ui_text_box(fb,(EpdRect){60,207,394,48},27,"Cỡ chữ hệ thống",EPD_DRAW_ALIGN_LEFT,false);
        ui_text_box(fb,(EpdRect){474,207,115,48},27,size,EPD_DRAW_ALIGN_RIGHT,false);
        settings_center_line(fb,60,285,18,"Chạm để chuyển mức, tối đa 200%",EPD_DRAW_ALIGN_LEFT,false);
        ui_draw_icon(fb,619,231,28,UI_ICON_ROTATE_CW,0x38);
#else
        if (settings_stacked()) {
            settings_center_line(fb,60,247,27,"Cỡ chữ hệ thống",EPD_DRAW_ALIGN_LEFT,false);
            settings_center_line(fb,589,247,27,size,EPD_DRAW_ALIGN_RIGHT,false);
            settings_center_line(fb,60,328,18,"Chạm để chuyển mức, tối đa 200%",EPD_DRAW_ALIGN_LEFT,false);
        } else {
            settings_center_line(fb,58,227,25,"Cỡ chữ hệ thống",EPD_DRAW_ALIGN_LEFT,false);
            settings_center_line(fb,58,269,18,"Chạm để chuyển mức, tối đa 200%",EPD_DRAW_ALIGN_LEFT,false);
            settings_center_line(fb,589,247,26,size,EPD_DRAW_ALIGN_RIGHT,false);
        }
        ui_draw_icon(fb,619,247,28,UI_ICON_ROTATE_CW,0x38);
#endif
        section(fb,l.preview_title,"Xem trước hiệu ứng");
        settings_card(fb,l.preview,22,UI_GRAY_WHITE,0x70);
#if KIIKO_BOARD_METALIO47
        ui_text_box(fb,(EpdRect){60,l.preview.y+12,564,66},29,"Đọc sách mỗi ngày cùng dpReader",EPD_DRAW_ALIGN_LEFT,false);
        ui_text_box(fb,(EpdRect){60,l.preview.y+90,564,52},25,"Aa 123",EPD_DRAW_ALIGN_LEFT,false);
#else
        settings_center_line(fb,60,l.preview.y+(settings_stacked()?68:51),29,"Đọc sách mỗi ngày cùng dpReader",EPD_DRAW_ALIGN_LEFT,false);
        settings_center_line(fb,60,l.preview.y+(settings_stacked()?151:107),25,"Aa 123",EPD_DRAW_ALIGN_LEFT,false);
#endif
        ui_text_box(fb,(EpdRect){40,l.font_title,380,38},22,"Chọn phông chữ",EPD_DRAW_ALIGN_LEFT,false);
        const char *selected = app_settings_system_font_path();
        int count = ttf_font_count(), first = s_font_page*l.slots;
#if KIIKO_BOARD_METALIO47
        char total[32];snprintf(total,sizeof(total),"Tổng %d phông",count+1);
        ui_text_box(fb,(EpdRect){438,l.font_title,210,38},18,total,EPD_DRAW_ALIGN_RIGHT,false);
#endif
        for (int slot=0;slot<l.slots && first+slot<=count;++slot) {
            int index=first+slot;
            const ttf_font_item_t *item=index?ttf_font_item(index-1):NULL;
            bool active=item?!strcmp(selected,item->path):!selected[0];
            EpdRect box=system_font_item_rect(slot);
            settings_card(fb,box,20,active?0xe0:UI_GRAY_WHITE,0x70);
            char label[TTF_FONT_NAME_MAX];
            snprintf(label,sizeof(label),"%s",item?system_font_label(item->path):"Phông mặc định (Tích hợp)");
            settings_fit_text(label,26,box.width-100);
#if KIIKO_BOARD_METALIO47
            ui_text_box(fb,(EpdRect){60,box.y+8,box.width-100,box.height-16},26,label,EPD_DRAW_ALIGN_LEFT,false);
#else
            settings_center_line(fb,60,box.y+box.height/2,26,label,EPD_DRAW_ALIGN_LEFT,false);
#endif
            if (active) epd_fill_circle(613,box.y+box.height/2,6,UI_GRAY_BLACK,fb);
        }
#if KIIKO_BOARD_METALIO47
        char pages[32];
#else
        char pages[24];
#endif
        snprintf(pages,sizeof(pages),"%d / %d",s_font_page+1,(count+l.slots)/l.slots);
#if KIIKO_BOARD_METALIO47
        int page_count=(count+l.slots)/l.slots;
        for(int next=0;next<2;++next) {
            EpdRect r=system_font_page_button(next);
            bool enabled=next?s_font_page+1<page_count:s_font_page>0;
            settings_card(fb,r,18,UI_GRAY_WHITE,enabled?0x50:0xc0);
            ui_draw_icon(fb,r.x+r.width/2,r.y+r.height/2,28,
                next?UI_ICON_CHEVRON_RIGHT:UI_ICON_CHEVRON_LEFT,enabled?UI_GRAY_BLACK:0xc0);
        }
        if(s_notice[0])ui_text_box(fb,(EpdRect){190,settings_viewport_end()-66,304,48},18,s_notice,EPD_DRAW_ALIGN_CENTER,false);
        else settings_center_line(fb,342,settings_viewport_end()-42,20,pages,EPD_DRAW_ALIGN_CENTER,false);
#else
        ui_text_fixed_vc(fb,648,settings_viewport_end()-16,18,pages,EPD_DRAW_ALIGN_RIGHT,false);
        if (s_notice[0]) ui_text_fixed_vc(fb,42,settings_viewport_end()-16,18,s_notice,EPD_DRAW_ALIGN_LEFT,false);
#endif
        settings_navigation(fb);
        if (s_font_sync_confirm) draw_system_font_sync(fb);
        return;
    }
    if (s_page == SETTINGS_SYSTEM_SIZE) {
        back_header(fb, "Cỡ chữ hệ thống");
        section(fb, 248, "Cỡ chữ giao diện hệ thống");
        for (int i = 0; i < 11; ++i) {
            int value = 100 + i * 10;
            EpdRect box = {36 + (i % 2) * 316, 298 + (i / 2) * 122, 296, 102};
            bool active = app_settings_system_font_size() == value;
            settings_card(fb, box, 20, active ? 0xd0 : UI_GRAY_WHITE, 0x70);
            char label[48]; snprintf(label, sizeof(label), "%d%%", value);
            ui_text_box(fb,(EpdRect){box.x+24,box.y+8,box.width-80,box.height-16},29,label,EPD_DRAW_ALIGN_LEFT,false);
            if (active) epd_fill_circle(box.x + box.width - 25, box.y + 51, 8, UI_GRAY_BLACK, fb);
        }
        settings_navigation(fb);
        return;
    }
    if (s_page == SETTINGS_MAIN_REFRESH) {
        back_header(fb, "Chế độ làm mới Trang chủ");
        section(fb, 248, "Trang chủ · Tủ sách · Quản lý tệp · Cài đặt");
        const char* names[] = {"Thường", "Làm mới nhanh", "Sóng nước"};
        const char* details[] = {"Cân bằng giữa độ mịn thang xám và độ sắc nét", "Làm mới đen trắng nhanh, kệ sách dùng họa tiết caro mịn", "Chuyển trang hiệu ứng sóng nước, giữ độ mịn thang xám"};
        for (int i = 0; i < 3; ++i) {
            const EpdRect box = {36, 298 + i * 158, 612, 132};
            const bool active = app_settings_main_refresh_mode() == (app_main_refresh_mode_t)i;
            settings_card(fb, box, 22, active ? 0xd0 : UI_GRAY_WHITE, 0x58);
            ui_text_box(fb,(EpdRect){64,box.y+10,510,54},30,names[i],EPD_DRAW_ALIGN_LEFT,false);
            char detail[128]; snprintf(detail, sizeof(detail), "%s", details[i]); fit_value(detail, 498);
            ui_text_box(fb,(EpdRect){64,box.y+72,552,46},22,detail,EPD_DRAW_ALIGN_LEFT,false);
            if (active) epd_fill_circle(607, box.y + 38, 8, UI_GRAY_BLACK, fb);
        }
        settings_navigation(fb);
        return;
    }
    if (s_page == SETTINGS_SYSTEM_CONTRAST) {
        back_header(fb, "Độ tương phản hệ thống");
        section(fb, 248, "Điều chỉnh độ đậm và độ tương phản thang xám của giao diện");
        static const char *const names[] = {"Dịu nhẹ", "Chuẩn", "Rõ nét", "Sắc nét", "Tương phản cao"};
        for (int i = 0; i < 5; ++i) {
            int value = 100 + i * 10;
            EpdRect box = {36, 298 + i * 116, 612, 90};
            bool active = app_settings_system_contrast() == value;
            settings_card(fb, box, 20, active ? 0xd0 : UI_GRAY_WHITE, 0x70);
            ui_text_box(fb,(EpdRect){64,box.y+10,370,70},28,names[i],EPD_DRAW_ALIGN_LEFT,false);
            char shown[16]; snprintf(shown, sizeof(shown), "%d%%", value);
            ui_text_box(fb,(EpdRect){460,box.y+10,110,70},22,shown,EPD_DRAW_ALIGN_RIGHT,false);
            if (active) epd_fill_circle(605, box.y + 45, 8, UI_GRAY_BLACK, fb);
        }
        settings_navigation(fb);
        return;
    }
    if (s_page == SETTINGS_LOCK_STYLE) {
        back_header(fb, "Kiểu màn hình khóa");
        section(fb, 248, "Chọn hình hiển thị trên màn hình khi khóa máy");
        const char *labels[] = {"Hình nền khóa", "Thẻ đọc sách", "Ghép bìa Tủ sách", "Khung PNG trong suốt"};
        const char *details[] = {"Dùng ảnh JPG / PNG trên thẻ nhớ", "Bìa sách, tiến độ và thống kê đọc", "Bìa sách thật · Tên & số lượng · Phông hệ thống", "Lấy trang đang đọc làm nền · Tự động tách nền trắng PNG"};
        const uint8_t values[] = {1, 0, 2, 3};
        for (int i = 0; i < 4; ++i) {
            EpdRect box = {36, 300 + i * 154, 612, 132};
            bool active = app_settings_lock_style() == values[i];
            settings_card(fb, box, 22, active ? 0xd0 : UI_GRAY_WHITE, 0x70);
            ui_text_box(fb,(EpdRect){64,box.y+10,510,54},31,labels[i],EPD_DRAW_ALIGN_LEFT,false);
            ui_text_box(fb,(EpdRect){64,box.y+72,552,46},20,details[i],EPD_DRAW_ALIGN_LEFT,false);
            if (active) epd_fill_circle(609, box.y + 66, 8, UI_GRAY_BLACK, fb);
        }
        settings_line(fb,54,966,21,"Chép PNG trong suốt vào pictures · Khuyên dùng 684 × 1216",EPD_DRAW_ALIGN_LEFT,false);
        if (app_settings_wallpaper_path()[0])
            row(fb, 1010, "Hình nền hiện tại", strrchr(app_settings_wallpaper_path(), '/') + 1);
        settings_navigation(fb);
        return;
    }
    if (s_page == SETTINGS_WALLPAPER) {
        back_header(fb,s_bookmark_picker?"Chọn khung PNG trong suốt":"Chọn hình nền");
        section(fb,244,s_bookmark_picker?"pictures · PNG trong suốt · Giữ nguyên khung hình":"Ảnh trên thẻ nhớ · JPG / PNG · Tối đa 50 MB");
        if (!s_wallpaper_count)
            settings_line(fb, 54, 325, 25, "Không tìm thấy ảnh, hãy chép vào thư mục pictures trên thẻ nhớ", EPD_DRAW_ALIGN_LEFT, false);
        for (int i = 0; i < 8; ++i) {
            int index = s_wallpaper_page * 8 + i;
            if (index >= s_wallpaper_count) break;
            EpdRect box = {36, 292 + i * 92, 612, 78};
            bool active = !strcmp(s_wallpapers[index].path,s_bookmark_picker?app_settings_bookmark_path():app_settings_wallpaper_path());
            settings_card(fb, box, 18, active ? 0xd0 : UI_GRAY_WHITE, 0x70);
            char name[96]; snprintf(name, sizeof(name), "%s", s_wallpapers[index].name);
            fit_value(name, 490);
            ui_text_box(fb,(EpdRect){60,box.y+8,box.width-100,box.height-16},24,name,EPD_DRAW_ALIGN_LEFT,false);
            if (active) epd_fill_circle(609, box.y + 39, 6, UI_GRAY_BLACK, fb);
        }
        settings_navigation(fb);
        return;
    }
    if (s_page == SETTINGS_WALLPAPER_PREVIEW) {
        back_header(fb,s_bookmark_picker?"Xem trước khung trong suốt":"Xem trước hình nền");
        EpdRect image = {166, 194, 352, 626};
        ui_fill_round_rect(fb, image, 8, UI_GRAY_WHITE);
        s_wallpaper_preview_ok = s_wallpaper_selected >= 0 && s_wallpaper_selected < s_wallpaper_count &&
            (s_bookmark_picker ? s_bookmark_preview != NULL : ui_wallpaper_draw(fb, s_wallpapers[s_wallpaper_selected].path, image));
        if(s_bookmark_picker && s_bookmark_preview)book_lock_bookmark_preview_paint(fb,s_bookmark_preview,image);
        ui_draw_round_rect(fb, image, 8, 0x70);
        if (!s_wallpaper_preview_ok)
            settings_center_line(fb, 342, 507, 23, s_bookmark_picker?"Không tải được PNG, hãy chọn ảnh khác":"Không thể xem trước ảnh", EPD_DRAW_ALIGN_CENTER, false);
        if (s_wallpaper_selected >= 0 && s_wallpaper_selected < s_wallpaper_count) {
            char name[96]; snprintf(name, sizeof(name), "%s", s_wallpapers[s_wallpaper_selected].name);
            fit_value(name, 570);
            settings_center_line(fb, 342, 859, 24, name, EPD_DRAW_ALIGN_CENTER, false);
            char position[80];
            snprintf(position, sizeof(position), "%d / %d · Vuốt lên/xuống để chuyển", s_wallpaper_selected + 1, s_wallpaper_count);
            settings_center_line(fb, 342, 908, 20, position, EPD_DRAW_ALIGN_CENTER, false);
        }
        if (s_wallpaper_preview_ok) {
            EpdRect button = {94, 962, 496, 70};
            ui_fill_round_rect(fb, button, 22, 0x30);
            settings_center_line(fb, 342, 997, 25, s_bookmark_picker?"Dùng khung trong suốt":"Đặt làm hình nền khóa", EPD_DRAW_ALIGN_CENTER, true);
        }
        if (s_wallpaper_confirm) {
            EpdRect dialog = {68, 435, 548, 306};
            ui_fill_round_rect(fb, dialog, 26, UI_GRAY_WHITE);
            ui_draw_control_frame(fb, dialog, 26, 0x50);
            settings_center_line(fb, 342, 503, 31, s_bookmark_picker?"Bật màn hình khóa PNG trong suốt?":"Đặt làm hình nền khóa?", EPD_DRAW_ALIGN_CENTER, false);
            settings_center_line(fb, 342, 565, 20, s_bookmark_picker?"Dùng trang đang mở trước khi khóa làm nền":"Sau khi xác nhận, nhấn ngắn nút nguồn sẽ hiện hình nền này", EPD_DRAW_ALIGN_CENTER, false);
            EpdRect cancel = {94, 643, 232, 68}, confirm = {358, 643, 232, 68};
            ui_fill_round_rect(fb, cancel, 18, 0xe0);
            ui_fill_round_rect(fb, confirm, 18, 0x30);
            ui_text_box(fb,ui_inset_rect(cancel,10),24,"Hủy",EPD_DRAW_ALIGN_CENTER,false);
            ui_text_box(fb,ui_inset_rect(confirm,10),24,"Xác nhận",EPD_DRAW_ALIGN_CENTER,true);
        }
        settings_navigation(fb);
        return;
    }
    if (s_page == SETTINGS_TIME) {
        back_header(fb, "Ngày & Giờ");
        time_t now = time(NULL);
        char shown[64] = "Chưa đặt";
        if (now >= 1704067200) {
            struct tm tm; time_t local = now + 7 * 3600;
            if (gmtime_r(&local, &tm)) snprintf(shown, sizeof(shown), "%04d-%02d-%02d  %02d:%02d",
                tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min);
        }
        section(fb, 253, "Giờ hiện tại · GMT+7 (Việt Nam)");
        row(fb, 296, shown, "");
        row(fb, 407, "Đồng bộ giờ qua WiFi (NTP)", "Đồng bộ ngay  ›");
        row(fb, 518, "Chỉnh giờ thủ công", "Chỉnh sửa  ›");
        settings_line(fb, 54, 656, 21, "Sau khi đồng bộ, đồng hồ RTC nội bộ sẽ tự chạy không cần giữ WiFi.", EPD_DRAW_ALIGN_LEFT, false);
        settings_line(fb, 54, 694, 21, "Dùng lâu không mạng có thể lệch vài giây, nên thỉnh thoảng đồng bộ lại.", EPD_DRAW_ALIGN_LEFT, false);
        if (s_notice[0]) settings_line(fb, 54, 772, 22, s_notice, EPD_DRAW_ALIGN_LEFT, false);
        settings_navigation(fb);
        return;
    }
    if (s_page == SETTINGS_KOSYNC) {
        back_header(fb, "Đồng bộ KOSync");
        section(fb, 245, "Chạm vào từng mục để chỉnh sửa thông số:");

        settings_card(fb, (EpdRect){36, 275, 612, 174}, 20, UI_GRAY_WHITE, 0x70);
        // Hàng 1: Máy chủ
        ui_text_box(fb,(EpdRect){58,280,180,48},22,"Máy chủ",EPD_DRAW_ALIGN_LEFT,false);
        char srv_short[48];
        snprintf(srv_short, sizeof(srv_short), "%s", app_settings_kosync_url());
        ui_text_box(fb, (EpdRect){260, 280, 344, 48}, 19, srv_short, EPD_DRAW_ALIGN_RIGHT, false);
        ui_draw_icon(fb, 622, 304, 22, UI_ICON_CHEVRON_RIGHT, UI_GRAY_BLACK);
        ui_hairline(fb, 333, 48, 588, UI_GRAY_LIGHT);

        // Hàng 2: Tài khoản
        ui_text_box(fb,(EpdRect){58,338,180,48},22,"Tài khoản",EPD_DRAW_ALIGN_LEFT,false);
        ui_text_box(fb,(EpdRect){260,338,344,48},21,app_settings_kosync_user()[0] ? app_settings_kosync_user() : "Chạm để nhập",EPD_DRAW_ALIGN_RIGHT,false);
        ui_draw_icon(fb, 622, 362, 22, UI_ICON_CHEVRON_RIGHT, UI_GRAY_BLACK);
        ui_hairline(fb, 391, 48, 588, UI_GRAY_LIGHT);

        // Hàng 3: Mật khẩu
        ui_text_box(fb,(EpdRect){58,396,180,48},22,"Mật khẩu",EPD_DRAW_ALIGN_LEFT,false);
        ui_text_box(fb,(EpdRect){260,396,344,48},21,app_settings_kosync_key()[0] ? "•••••••• (Đã lưu)" : "Chạm để nhập",EPD_DRAW_ALIGN_RIGHT,false);
        ui_draw_icon(fb, 622, 420, 22, UI_ICON_CHEVRON_RIGHT, UI_GRAY_BLACK);

        bool auto_on = app_settings_kosync_auto();
        settings_card(fb, (EpdRect){36, 465, 612, 92}, 20, auto_on ? 0xd0 : UI_GRAY_WHITE, 0x70);
        ui_text_box(fb,(EpdRect){58,477,430,38},23,"Tự động đồng bộ tiến độ đọc",EPD_DRAW_ALIGN_LEFT,false);
        ui_text_box(fb,(EpdRect){58,521,550,30},18,"Tự gửi/nhận khi mở hoặc đóng sách (Chạm để Bật/Tắt)",EPD_DRAW_ALIGN_LEFT,false);
        ui_text_box(fb,(EpdRect){510,477,98,38},22,auto_on?"BẬT":"TẮT",EPD_DRAW_ALIGN_RIGHT,false);
        if (auto_on) epd_fill_circle(620, 511, 7, UI_GRAY_BLACK, fb);

        ui_draw_button(fb, (EpdRect){36, 577, 612, 72}, "Kiểm tra kết nối máy chủ KOSync", false);
        ui_draw_button(fb, (EpdRect){36, 665, 612, 72}, "Cấu hình tài khoản qua Web (WiFi Transfer)  ›", false);
        ui_draw_button(fb, (EpdRect){36, 753, 612, 72}, "Đặt lại máy chủ mặc định (sync.vreader.cloud)", false);

        if (s_kosync_notice[0]) {
            settings_line(fb, 48, 852, 21, s_kosync_notice, EPD_DRAW_ALIGN_LEFT, false);
        } else {
            settings_line(fb, 48, 852, 18, "Tương thích 100% với KOReader qua thuật toán băm partialMD5.", EPD_DRAW_ALIGN_LEFT, false);
        }
        settings_line(fb, 48, 892, 18, "Khi đọc sách: bấm nút 'Đồng bộ' trên thanh công cụ để đồng bộ tức thì.", EPD_DRAW_ALIGN_LEFT, false);
        settings_navigation(fb);
        return;
    }
    if (s_page == SETTINGS_OPDS) {
        back_header(fb, "Kho sách OPDS");
        section(fb, 245, "Chạm vào từng mục để chỉnh sửa thông số:");

        settings_card(fb, (EpdRect){36, 275, 612, 174}, 20, UI_GRAY_WHITE, 0x70);
        // Hàng 1: Kho sách
        ui_text_box(fb,(EpdRect){58,280,180,48},22,"Kho sách",EPD_DRAW_ALIGN_LEFT,false);
        char opds_short[48];
        snprintf(opds_short, sizeof(opds_short), "%s", app_settings_opds_url());
        ui_text_box(fb, (EpdRect){260, 280, 344, 48}, 19, opds_short, EPD_DRAW_ALIGN_RIGHT, false);
        ui_draw_icon(fb, 622, 304, 22, UI_ICON_CHEVRON_RIGHT, UI_GRAY_BLACK);
        ui_hairline(fb, 333, 48, 588, UI_GRAY_LIGHT);

        // Hàng 2: Tài khoản
        ui_text_box(fb,(EpdRect){58,338,180,48},22,"Tài khoản",EPD_DRAW_ALIGN_LEFT,false);
        ui_text_box(fb,(EpdRect){260,338,344,48},21,app_settings_opds_user()[0] ? app_settings_opds_user() : "Không có (Khách)",EPD_DRAW_ALIGN_RIGHT,false);
        ui_draw_icon(fb, 622, 362, 22, UI_ICON_CHEVRON_RIGHT, UI_GRAY_BLACK);
        ui_hairline(fb, 391, 48, 588, UI_GRAY_LIGHT);

        // Hàng 3: Mật khẩu
        ui_text_box(fb,(EpdRect){58,396,180,48},22,"Mật khẩu",EPD_DRAW_ALIGN_LEFT,false);
        ui_text_box(fb,(EpdRect){260,396,344,48},21,app_settings_opds_pass()[0] ? "••••••••" : "Không có",EPD_DRAW_ALIGN_RIGHT,false);
        ui_draw_icon(fb, 622, 420, 22, UI_ICON_CHEVRON_RIGHT, UI_GRAY_BLACK);

        ui_draw_button(fb, (EpdRect){36, 465, 612, 72}, "Mở kho sách OPDS ngay  ›", false);
        ui_draw_button(fb, (EpdRect){36, 553, 612, 72}, "Cấu hình kho sách qua Web (WiFi Transfer)  ›", false);
        ui_draw_button(fb, (EpdRect){36, 641, 612, 72}, "Đặt lại kho sách mặc định (sach.vreader.cloud)", false);

        if (s_opds_notice[0]) {
            settings_line(fb, 48, 742, 21, s_opds_notice, EPD_DRAW_ALIGN_LEFT, false);
        } else {
            settings_line(fb, 48, 742, 18, "Kho sách mặc định: https://sach.vreader.cloud:8443/opds", EPD_DRAW_ALIGN_LEFT, false);
        }
        settings_line(fb, 48, 782, 18, "Bạn có thể kết nối kho sách Calibre-Web riêng bằng cách đổi URL qua Web.", EPD_DRAW_ALIGN_LEFT, false);
        settings_line(fb, 48, 822, 18, "Trong Tủ sách: chọn '+ Nhập sách' -> 'Thư viện OPDS' để duyệt sách.", EPD_DRAW_ALIGN_LEFT, false);
        settings_navigation(fb);
        return;
    }
    if (s_page == SETTINGS_AUTO_LOCK) {
        back_header(fb, "Tự động khóa màn hình");
        section(fb, 228, "Tự động khóa khi không thao tác");
        const char *labels[] = {"1 phút", "5 phút", "10 phút", "Tắt"};
        const uint8_t values[] = {1, 5, 10, 0};
        for (int i = 0; i < 4; ++i) {
            EpdRect box = {36, 272 + i * 112, 612, 92};
            bool active = app_settings_auto_lock_minutes() == values[i];
            settings_card(fb, box, 20, active ? 0xd0 : UI_GRAY_WHITE, 0x70);
            ui_text_box(fb,(EpdRect){64,box.y+8,516,box.height-16},30,labels[i],EPD_DRAW_ALIGN_LEFT,false);
            if (active) epd_fill_circle(609, box.y + box.height / 2, 8, UI_GRAY_BLACK, fb);
        }
        settings_line(fb, 48, 760, 23, "Tự tính lại thời gian khi có thao tác; tạm dừng khi truyền sách", EPD_DRAW_ALIGN_LEFT, false);
#if KIIKO_BOARD_METALIO47
        settings_line(fb, 48, 810, 23, "Xem thêm tùy chọn tiết kiệm pin ở mục 'Chế độ tắt nguồn / ngủ'", EPD_DRAW_ALIGN_LEFT, false);
#else
        settings_line(fb, 48, 810, 23, "Khóa máy sẽ ngủ nhẹ trước, sau 10 phút chuyển sang ngủ sâu", EPD_DRAW_ALIGN_LEFT, false);
#endif
        settings_navigation(fb);
        return;
    }
#if KIIKO_BOARD_METALIO47
    if (s_page == SETTINGS_POWER_SLEEP) {
        back_header(fb, "Chế độ tắt nguồn / ngủ");
        section(fb, 248, "Hành vi khi chọn Tắt nguồn");
        const char *labels[] = {"Ngủ nhẹ rồi tắt nguồn", "Ngắt nguồn hoàn toàn"};
        const char *details[] = {"Ngủ nhẹ bật lên ngay; tắt nguồn sẽ khởi động lại từ đầu",
                                 "Nhấn giữ nút nguồn để bật máy"};
        for (int i = 0; i < 2; ++i) {
            EpdRect box = {36, 300 + i * 154, 612, 132};
            bool active = app_settings_staged_shutdown() == (i == 0);
            settings_card(fb, box, 22, active ? 0xd0 : UI_GRAY_WHITE, 0x70);
            ui_text_box(fb,(EpdRect){64,box.y+10,510,54},24,labels[i],EPD_DRAW_ALIGN_LEFT,false);
            ui_text_box(fb,(EpdRect){64,box.y+72,552,46},20,details[i],EPD_DRAW_ALIGN_LEFT,false);
            if (active) epd_fill_circle(609, box.y + 66, 8, UI_GRAY_BLACK, fb);
        }
        ui_text_fixed_vc(fb, 36, 650, 26, "Tự tắt sau khi ngủ nhẹ (1–24 giờ)", EPD_DRAW_ALIGN_LEFT, false);
        const unsigned hours=app_settings_sleep_off_hours();
        char duration[48];snprintf(duration,sizeof(duration),"%u giờ",hours);
        settings_card(fb,(EpdRect){36,690,80,72},18,UI_GRAY_WHITE,0x70);
        settings_card(fb,(EpdRect){568,690,80,72},18,UI_GRAY_WHITE,0x70);
        ui_text_fixed_vc(fb,76,726,40,"−",EPD_DRAW_ALIGN_CENTER,false);
        ui_text_fixed_vc(fb,608,726,40,"+",EPD_DRAW_ALIGN_CENTER,false);
        ui_text_fixed_vc(fb,342,726,36,duration,EPD_DRAW_ALIGN_CENTER,false);
        settings_line(fb, 54, 822, 18, "Khóa thủ công hoặc tự khóa: vào ngủ nhẹ ngay", EPD_DRAW_ALIGN_LEFT, false);
        settings_line(fb, 54, 866, 18, "Đủ thời gian đã chọn mới tự tắt nguồn", EPD_DRAW_ALIGN_LEFT, false);
        settings_line(fb, 54, 910, 18, "Tạm hoãn ngủ khi cắm USB hoặc có tác vụ đang bận", EPD_DRAW_ALIGN_LEFT, false);
        settings_navigation(fb);
        return;
    }

#else
    if (s_page == SETTINGS_POWER_SLEEP) {
        back_header(fb, "Chế độ tắt nguồn / ngủ");
        section(fb, 236, "Chọn hành vi khi nhấn Tắt nguồn hoặc khi máy tự ngủ");
        const char *shutdown_labels[] = {"Tắt nguồn hẳn", "Ngủ nhẹ trước, sau 10 phút chuyển sang ngủ sâu"};
        const char *shutdown_details[] = {"Ngắt nguồn hoàn toàn, nhấn giữ nút nguồn để bật máy",
                                          "Giữ nguyên hình khóa trên màn E-Ink; khi ngủ sâu nhấn nút nguồn để bật"};
        for (int i = 0; i < 2; ++i) {
            int y = 278 + i * 136;
            bool active = app_settings_staged_shutdown() == (i == 1);
            settings_card(fb, (EpdRect){36, y, 612, 124}, 22, active ? 0xd0 : UI_GRAY_WHITE, 0x70);
            settings_line(fb, 64, y + 20, 29, shutdown_labels[i], EPD_DRAW_ALIGN_LEFT, false);
            settings_line(fb, 64, y + 72, 20, shutdown_details[i], EPD_DRAW_ALIGN_LEFT, false);
            if (active) epd_fill_circle(609, y + 62, 8, UI_GRAY_BLACK, fb);
        }
        settings_line(fb, 54, 558, 21, "Khi ngủ nhẹ chạm phím mở ngay; ngủ sâu sẽ khởi động lại khi bật.", EPD_DRAW_ALIGN_LEFT, false);
        settings_line(fb, 54, 594, 21, "Áp dụng cho cả Hình nền khóa và Thẻ đọc sách.", EPD_DRAW_ALIGN_LEFT, false);
        settings_navigation(fb);
        return;
    }
#endif
    if (s_page == SETTINGS_BOOT) {
        back_header(fb, "Nạp ROM qua máy tính");
        settings_card(fb, (EpdRect){36, 254, 612, 298}, 22, UI_GRAY_WHITE, 0x70);
#if KIIKO_BOARD_METALIO47
        settings_line(fb, 60, 288, 29, "Chuẩn bị cổng nạp BOOT", EPD_DRAW_ALIGN_LEFT, false);
        settings_line(fb, 60, 350, 22, "Dùng để nạp firmware dpReader hoặc khôi phục qua USB", EPD_DRAW_ALIGN_LEFT, false);
        ui_draw_button(fb, (EpdRect){60, 438, 564, 78}, s_boot_pending ? "Đang chuẩn bị" :
            app_download_active() ? "Mở lại cổng nạp ROM" : "Mở cổng nạp ROM", false);
        settings_line(fb, 48, 620, 22, s_boot_notice[0] ? s_boot_notice :
            app_download_active() ? (s_boot_connected ? "Đã cắm USB, hãy nhấn 'Kiểm tra kết nối' trên máy tính" :
                "Đang chờ cáp USB, chạm nút phía trên để kiểm tra lại") :
            "Cắm cáp USB, mở cổng nạp rồi kiểm tra kết nối trên máy tính", EPD_DRAW_ALIGN_LEFT, false);
        settings_line(fb, 48, 670, 20, "Máy tính sẽ kích hoạt chế độ nạp; có thể quay lại để hủy.", EPD_DRAW_ALIGN_LEFT, false);
        settings_line(fb, 48, 714, 20, "Sau khi máy tính nhận cổng USB-JTAG/Serial sẽ vào chế độ ROM.", EPD_DRAW_ALIGN_LEFT, false);
#else
        settings_line(fb, 60, 288, 29, "Vào chế độ BOOT", EPD_DRAW_ALIGN_LEFT, false);
        settings_line(fb, 60, 350, 22, "Dùng để nạp toàn bộ firmware hoặc cứu máy qua cáp USB", EPD_DRAW_ALIGN_LEFT, false);
        ui_draw_button(fb, (EpdRect){60, 438, 564, 78}, s_boot_pending ? "Đang vào" : "Vào chế độ BOOT", false);
        settings_line(fb, 48, 620, 22, "Hãy cắm cáp USB vào máy tính rồi chạy công cụ nạp firmware", EPD_DRAW_ALIGN_LEFT, false);
#endif
        settings_navigation(fb); return;
    }
    if (s_page == SETTINGS_UPGRADE) {
        back_header(fb, "Cập nhật hệ thống");
        settings_card(fb, (EpdRect){36, 202, 612, 120}, 22, UI_GRAY_WHITE, 0x70);
        settings_line(fb, 60, 224, 22, "Phiên bản hiện tại", EPD_DRAW_ALIGN_LEFT, false);
        settings_line(fb, 60, 264, 29, esp_app_get_description()->version, EPD_DRAW_ALIGN_LEFT, false);
        if (s_upgrade_confirm) {
            settings_card(fb, (EpdRect){36, 356, 612, s_upgrade_online ? 574 : 344}, 22, UI_GRAY_WHITE, 0x70);
            ui_text_fixed(fb, 60, 383, 31, s_upgrade_online ? "Có phiên bản mới" : "Xác nhận cập nhật thẻ nhớ", EPD_DRAW_ALIGN_LEFT, false);
            ui_text_fixed(fb, 60, 447, 28, s_upgrade_online ? s_update.release.version : s_local_update.candidate_version,
                    EPD_DRAW_ALIGN_LEFT, false);
            if (s_upgrade_online) {
                ui_text_fixed(fb, 60, 494, 26, "Bản cập nhật này", EPD_DRAW_ALIGN_LEFT, false);
                draw_upgrade_notes(fb);
            }
            ui_text_fixed(fb, 60, s_upgrade_online ? 770 : 513, 24, "Vui lòng giữ nguồn điện ổn định trong suốt quá trình cập nhật", EPD_DRAW_ALIGN_LEFT, false);
            ui_text_fixed(fb, 60, s_upgrade_online ? 806 : 560, 23, "Giữ nguyên cài đặt, lịch sử đọc và tệp trên thẻ nhớ", EPD_DRAW_ALIGN_LEFT, false);
            ui_draw_button(fb, upgrade_confirm_button(s_upgrade_online, false), s_upgrade_online ? "Để sau" : "Hủy", false);
            ui_draw_button(fb, upgrade_confirm_button(s_upgrade_online, true), s_upgrade_online ? "Bắt đầu cập nhật" : "Xác nhận cập nhật", true);
        } else {
            settings_card(fb, (EpdRect){36, 356, 612, 278}, 22, UI_GRAY_WHITE, 0x70);
            settings_line(fb, 60, 383, 29, "Cập nhật OTA", EPD_DRAW_ALIGN_LEFT, false);
            char detail[96];
#if KIIKO_BOARD_METALIO47
            snprintf(detail, sizeof(detail), "%s", s_update.message[0] ? s_update.message : "Kết nối WiFi để kiểm tra bản cập nhật mới trên GitHub");
#else
            snprintf(detail, sizeof(detail), "%s", s_update.message[0] ? s_update.message : "Kết nối WiFi để kiểm tra bản cập nhật firmware mới");
#endif
            fit_value(detail, 540);
            settings_line(fb, 60, 436, 21, detail, EPD_DRAW_ALIGN_LEFT, false);
            if (s_update.state == PICO_UPDATE_AVAILABLE)
                settings_line(fb, 60, 477, 24, s_update.release.version, EPD_DRAW_ALIGN_LEFT, false);
            if (s_update.state == PICO_UPDATE_DOWNLOADING) draw_upgrade_progress(fb);
            ui_draw_button(fb, (EpdRect){60, 550, 564, 62}, s_update.busy ? "Hủy cập nhật OTA" :
                s_update.state == PICO_UPDATE_AVAILABLE ? "Bắt đầu cập nhật" : "Kiểm tra cập nhật", s_update.state == PICO_UPDATE_AVAILABLE);
            settings_card(fb, (EpdRect){36, 662, 612, 262}, 22, UI_GRAY_WHITE, 0x70);
            settings_line(fb, 60, 689, 29, "Cập nhật từ thẻ nhớ", EPD_DRAW_ALIGN_LEFT, false);
            settings_line(fb, 60, 739, 23, "Đặt tệp cập nhật ở thư mục gốc thẻ nhớ", EPD_DRAW_ALIGN_LEFT, false);
#if KIIKO_BOARD_METALIO47
            settings_line(fb, 60, 786, 21, s_local_update.ready ? s_local_update.candidate_version : "Chép dpReader-update.bin vào thẻ nhớ", EPD_DRAW_ALIGN_LEFT, false);
#else
            settings_line(fb, 60, 786, 21, s_local_update.ready ? s_local_update.candidate_version : "Chép gói cập nhật vào thư mục gốc của thẻ nhớ", EPD_DRAW_ALIGN_LEFT, false);
#endif
            ui_draw_button(fb, (EpdRect){60, 840, 564, 62}, s_local_pending ? "Đang cài đặt, vui lòng không tắt nguồn" :
                s_local_update.ready ? "Cài đặt gói cập nhật trên thẻ nhớ" : "Kiểm tra lại thẻ nhớ", s_local_update.ready);
        }
        if (s_upgrade_notice[0]) settings_line(fb, 48, 957, 21, s_upgrade_notice, EPD_DRAW_ALIGN_LEFT, false);
        settings_line(fb, 48, 1010, 20, "Nạp toàn bộ ROM hoặc khôi phục firmware qua cáp USB", EPD_DRAW_ALIGN_LEFT, false);
        settings_navigation(fb);
        return;
    }
    if (s_page == SETTINGS_CONFIG) {
        back_header(fb, "Sao lưu & Khôi phục cấu hình");
        section(fb, 248, "Nhanh chóng khôi phục cài đặt sau khi nạp lại firmware");
        settings_card(fb, (EpdRect){36, 299, 612, 197}, 22, UI_GRAY_WHITE, 0x70);
        settings_line(fb, 60, 326, 26, "Thư mục gốc thẻ nhớ", EPD_DRAW_ALIGN_LEFT, false);
        settings_line(fb, 60, 377, 23, "Tệp sao lưu cấu hình", EPD_DRAW_ALIGN_LEFT, false);
        settings_line(fb, 60, 435, 19, "Bố cục trang, hồ sơ máy, lịch sử đọc và dấu trang", EPD_DRAW_ALIGN_LEFT, false);
        ui_draw_button(fb, (EpdRect){36, 561, 612, 83}, "Sao lưu cấu hình hiện tại ra thẻ nhớ", false);
        ui_draw_button(fb, (EpdRect){36, 681, 612, 83},
                       s_config_confirm ? "Chạm lần nữa để xác nhận khôi phục cấu hình" : "Khôi phục cấu hình từ thẻ nhớ", false);
        if (s_notice[0]) settings_line(fb, 48, 819, 21, s_notice, EPD_DRAW_ALIGN_LEFT, false);
        settings_line(fb, 48, 899, 19, "Bao gồm lịch sử đọc, hồ sơ máy, WiFi và đồng bộ KOSync/OPDS.", EPD_DRAW_ALIGN_LEFT, false);
        settings_line(fb, 48, 939, 19, "Sách, phông chữ và hình ảnh vẫn lưu trên thẻ nhớ.", EPD_DRAW_ALIGN_LEFT, false);
        settings_line(fb, 48, 979, 19, "Bản sao lưu có chứa mật khẩu WiFi, hãy bảo quản thẻ nhớ cẩn thận.", EPD_DRAW_ALIGN_LEFT, false);
        settings_navigation(fb);
        return;
    }
    if (s_page == SETTINGS_LOG_EXPORT) {
        back_header(fb, "Nhật ký & Chẩn đoán lỗi");
        section(fb, 248, "Thông tin hệ thống và xuất log để kiểm tra lỗi");
        EpdRect info_card = {36, 298, 612, 216};
        settings_card(fb, info_card, 22, UI_GRAY_WHITE, 0x70);
        char v_line[80];
        const esp_app_desc_t *app_d = esp_app_get_description();
        snprintf(v_line, sizeof(v_line), "Firmware: %s (%s)", app_d ? app_d->version : "1.2.1RC1-vn", "ESP32-S31");
        settings_line(fb, 56, 324, 22, v_line, EPD_DRAW_ALIGN_LEFT, false);
        char heap_line[80];
        snprintf(heap_line, sizeof(heap_line), "RAM trống: %u KB · PSRAM trống: %u KB",
                 (unsigned)(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024),
                 (unsigned)(heap_caps_get_free_size(MALLOC_CAP_SPIRAM) / 1024));
        settings_line(fb, 56, 360, 20, heap_line, EPD_DRAW_ALIGN_LEFT, false);
        const pmu_snapshot_t *pmu = read_pico_pmu_get();
        char bat_line[80];
        if (pmu) {
            int pct = pmu_battery_percent(pmu);
            bool chg = pmu_battery_charging(pmu);
            snprintf(bat_line, sizeof(bat_line), "Pin: %d%% (%u mV) %s", pct >= 0 ? pct : 0, (unsigned)pmu->battery_mv, chg ? "· Đang sạc" : "");
        } else {
            snprintf(bat_line, sizeof(bat_line), "Pin: Đang đo...");
        }
        settings_line(fb, 56, 396, 20, bat_line, EPD_DRAW_ALIGN_LEFT, false);
        read_pico_sd_info_t sd = {0};
        read_pico_sd_get_info(&sd);
        char sd_line[80];
        snprintf(sd_line, sizeof(sd_line), "Thẻ nhớ SD: %s", sd.mounted ? "Đã nhận (Sẵn sàng xuất log)" : "Chưa cắm / Không nhận");
        settings_line(fb, 56, 432, 20, sd_line, EPD_DRAW_ALIGN_LEFT, false);
        char ble_line[80];
        snprintf(ble_line, sizeof(ble_line), "Bluetooth: %s", ble_pt_connected() ? ble_pt_connected_name() : (ble_pt_running() ? "Bật (Chưa kết nối)" : "Tắt"));
        settings_line(fb, 56, 468, 20, ble_line, EPD_DRAW_ALIGN_LEFT, false);
        EpdRect log_box = {36, 532, 612, 250};
        settings_card(fb, log_box, 18, 0xf0, 0x50);
        char preview[512] = {0};
        log_export_get_recent(preview, sizeof(preview));
        if (!preview[0]) {
            snprintf(preview, sizeof(preview), "Nhật ký hệ thống trống hoặc chưa có bản ghi mới.\nBấm nút bên dưới để xuất toàn bộ log ra thẻ nhớ.");
        }
        ui_text_box(fb, (EpdRect){48, 544, 588, 226}, 16, preview, EPD_DRAW_ALIGN_LEFT, false);
        ui_draw_button(fb, (EpdRect){36, 804, 612, 80}, "Xuất log ra thẻ nhớ (/sdcard/dpreader_log.txt)", false);
        ui_draw_button(fb, (EpdRect){36, 902, 612, 80}, "Xóa nhật ký lưu tạm", false);
        if (s_notice[0]) {
            settings_line(fb, 48, 1010, 20, s_notice, EPD_DRAW_ALIGN_LEFT, false);
        }
        settings_navigation(fb);
        return;
    }
    if (s_page == SETTINGS_BLE_SCAN) {
        back_header(fb,"Quét thiết bị");
        const ble_scan_layout_t l = ble_scan_layout();
        const int scroll = s_ble_scroll;
        // 持续扫描中：告诉用户正在找，以及找到几台。
        // Continuous scan: say that it is running and how many turned up.
        char head[64];
        if (ble_pt_scanning()) snprintf(head, sizeof(head), "Đang tìm… Đã thấy %u thiết bị", (unsigned)ble_scan_visible_count());
        else snprintf(head, sizeof(head), "Đã dừng quét");
        settings_line(fb, 44, l.status_top - scroll, 23, head, EPD_DRAW_ALIGN_LEFT, false);
        for (int i = 0; i < l.device_rows; ++i) {
            const ble_pt_device_t *dev = ble_scan_visible_at(i);
            if (!dev) continue;
            char label[48];
            snprintf(label, sizeof(label), "%s", dev->name);
            fit_value(label, 360);
            char value[44];
            snprintf(value, sizeof(value), "%d dBm%s%s", dev->rssi, dev->hid ? " · HID" : "",
                     dev->bonded ? " · Đã ghép đôi" : "");
            int top = l.devices_top - scroll + i * BLE_ROW_H;
            if (top + BLE_ROW_H >= 190 && top < settings_viewport_end()) {
                settings_card(fb, (EpdRect){36, top, 612, BLE_ROW_H - 8}, 18, UI_GRAY_WHITE, 0x70);
                ui_text_box(fb,(EpdRect){60,top+8,520,32},25,label,EPD_DRAW_ALIGN_LEFT,false);
                ui_text_box(fb,(EpdRect){60,top+44,520,28},19,value,EPD_DRAW_ALIGN_LEFT,false);
                ui_draw_icon(fb,(624)-8,top + 38,26,UI_ICON_CHEVRON_RIGHT,UI_GRAY_BLACK);
            }
        }
        if (!l.device_rows)
            settings_line(fb, 44, l.devices_top - scroll + 18, 23, "Chưa tìm thấy thiết bị BLE nào ở gần",
                    EPD_DRAW_ALIGN_LEFT, false);
        ui_fill_rect_fast((EpdRect){0, 0, UI_LOCK_WIDTH, 190}, UI_GRAY_WHITE, fb);
        ui_nav_status(fb);
        back_header(fb, "Quét thiết bị");
        settings_navigation(fb);
        return;
    }
    if (s_page == SETTINGS_BLUETOOTH) {
        back_header(fb,"Điều khiển lật trang BLE");
        const ble_layout_t l = ble_layout();

        // 学习按键时把提示顶到最上面，用户不用滚回去看。
        // While learning, the prompt is pinned to the top so the user need not scroll back.
        if (s_ble_learning || s_ble_feedback[0]) {
            settings_card(fb, (EpdRect){36, 202 - s_ble_scroll, 612, 126}, 22, 0xd0, 0x58);
            settings_center_line(fb, 342, 265 - s_ble_scroll, 24,
                       !s_ble_learning ? s_ble_feedback :
                       s_ble_learning == 1 ? "Hãy nhấn phím muốn dùng làm 'Trang trước' trên điều khiển"
                                           : "Hãy nhấn phím muốn dùng làm 'Trang sau' trên điều khiển",
                       EPD_DRAW_ALIGN_CENTER, false);
            if (!s_ble_learning)
                settings_center_line(fb, 342, 305 - s_ble_scroll, 19, "Chạm vào đây để quay lại công tắc Bluetooth", EPD_DRAW_ALIGN_CENTER, false);
        } else {
            setting_toggle(fb, l.toggle_top, "Bật Bluetooth", "Bật để kết nối điều khiển lật trang BLE; tắt sẽ giải phóng RAM",
                           app_settings_ble_turner());
        }

        section(fb, l.status_top - 38, "Trạng thái");
        char ble_status[80];
        if (!app_settings_ble_turner()) snprintf(ble_status, sizeof(ble_status), "Đã tắt");
        else if (!ble_pt_running()) {
#if KIIKO_BOARD_METALIO47
            snprintf(ble_status, sizeof(ble_status), "Đang khởi động…");
#else
            read_pico_transfer_status_t network;
            read_pico_transfer_get_status(&network);
            snprintf(ble_status, sizeof(ble_status), network.state != READ_PICO_TRANSFER_STOPPED ? "Đang dùng WiFi, tạm dừng Bluetooth" : "Đang khởi động…");
#endif
        }
        else if (ble_pt_connected()) snprintf(ble_status, sizeof(ble_status), "Đã kết nối %s", ble_pt_connected_name());
        else if (ble_pt_connecting()) snprintf(ble_status, sizeof(ble_status), "Đang kết nối…");
        else snprintf(ble_status, sizeof(ble_status), "Chưa kết nối");
        ble_row(fb, l.status_top, "Hiện tại", ble_status);
        if (l.notice_top >= 0) settings_line(fb, 44, l.notice_top, 21, s_ble_notice, EPD_DRAW_ALIGN_LEFT, false);

        // 已配对：点行连接，右侧按钮删除。/ Bonded peers: the row connects, the button forgets.
        section(fb, l.bonds_title_top, "Đã ghép đôi");
        if (!l.bond_rows)
            ble_row(fb, l.bonds_title_top + 38, "Chưa có thiết bị ghép đôi", "");
        for (int i = 0; i < l.bond_rows && i < BLE_PT_MAX_BONDS; ++i) {
            const ble_pt_bond_t *bond = ble_pt_bond((uint8_t)i);
            if (!bond) continue;
            char label[40];
            snprintf(label, sizeof(label), "%s", bond->name[0] ? bond->name : bond->addr);
            fit_value(label, 300);
            const bool live = ble_pt_connected() && !strcmp(ble_pt_connected_name(), bond->name);
            ble_row(fb, l.bond_top[i], label, live ? "Đã kết nối" : "Kết nối  ›");
            ui_draw_button(fb, ble_button_rect(l.bond_top[i]), "Xóa", false);
        }

        ble_row(fb, l.scan_entry_top, "Thêm điều khiển lật trang", "Quét thiết bị");
        ui_draw_icon(fb,(624)-8,l.scan_entry_top + 38,26,UI_ICON_CHEVRON_RIGHT,UI_GRAY_BLACK);

        section(fb, l.learn_title_top, "Gán phím điều khiển");
        static const char *const learn_names[] = {"Trang trước", "Trang sau"};
        static const ble_pt_action_t learn_actions[] = {BLE_PT_ACTION_PREV, BLE_PT_ACTION_NEXT};
        for (int i = 0; i < 2; ++i) {
            // 学到的键直接显示出来；没学过就走内置映射。
            // Show the learned key itself; with none, the built-in map applies.
            char bound_label[40];
            ble_pt_frame_t bound;
            if (ble_pt_binding(learn_actions[i], &bound)) format_bound_key(&bound, bound_label, sizeof(bound_label));
            else snprintf(bound_label, sizeof(bound_label), "Phím mặc định");
            ble_row(fb, l.learn_top[i], learn_names[i], bound_label);
            ui_draw_button(fb, ble_button_rect(l.learn_top[i]), "Học phím", false);
        }
        settings_line(fb, 48, l.footer_top, 20, "Phím Lên/Xuống, Trái/Phải và PageUp/PageDown đều lật trang", EPD_DRAW_ALIGN_LEFT, false);
        settings_line(fb, 48, l.footer_top + 34, 20, "Hỗ trợ điều khiển lật trang BLE khi đang đọc sách", EPD_DRAW_ALIGN_LEFT, false);
        ui_fill_rect_fast((EpdRect){0, 0, UI_LOCK_WIDTH, 190}, UI_GRAY_WHITE, fb);
        ui_nav_status(fb);
        back_header(fb, "Điều khiển lật trang BLE");
        settings_navigation(fb);
        return;
    }
    const int profile_y=180-s_main_scroll, profile_h=settings_profile_height();
    if (s_settings_group < 0 && profile_y+profile_h>160) {
        settings_card(fb,(EpdRect){36,profile_y,612,profile_h},24,UI_GRAY_WHITE,0x70);
        int avatar_y=profile_y+(profile_h-82)/2;
        bool avatar_ok=s_avatar_ready;
        if (avatar_ok) ui_wallpaper_draw_avatar(fb,s_avatar_pixels,(EpdRect){57,avatar_y,82,82},20);
#if KIIKO_BOARD_METALIO47
        if (app_settings_main_fast_refresh() && avatar_ok) ui_image_bw_rect(fb,(EpdRect){57,avatar_y,82,82});
#endif
        if (!avatar_ok) {
            ui_fill_round_rect(fb,(EpdRect){57,avatar_y,82,82},20,0x30);
            ui_text_fixed_vc(fb,98,avatar_y+41,49,"K",EPD_DRAW_ALIGN_CENTER,true);
        }
        char name[64];snprintf(name,sizeof(name),"%s",app_settings_device_name());
        int width=588-ui_text_fixed_width_px(ui_text_effective_px(20),"Sửa")-188;
        while (name[0] && ui_text_fixed_width_px(ui_text_effective_px(32),name)>width) {
            size_t n=strlen(name)-1;while(n&&((unsigned char)name[n]&0xc0)==0x80)--n;name[n]=0;
        }
        settings_center_line(fb,164,profile_y+53,32,name,EPD_DRAW_ALIGN_LEFT,false);
        settings_center_line(fb,588,profile_y+53,20,"Sửa",EPD_DRAW_ALIGN_RIGHT,false);
        ui_draw_icon(fb,625,profile_y+profile_h/2,26,UI_ICON_CHEVRON_RIGHT,UI_GRAY_BLACK);
    }
    if (s_settings_group < 0) {
        const int icons[] = {0, 2, 5, 1, 12};
        const char *const summaries[] = {
            "WiFi · Bluetooth · KOSync · OPDS",
            "Phông chữ · Làm mới · Nguồn & Ngủ",
            "Tủ sách · Màn hình khóa · Chữ ký",
            "Phím cứng & Cử chỉ",
            "Cập nhật · Sao lưu · Chẩn đoán"
        };
        for (int i=0; i<5; ++i) {
            int y=SETTINGS_PARENT_Y+i*SETTINGS_PARENT_ROW_H-s_main_scroll;
            if(y+SETTINGS_PARENT_ROW_H<=160 || y>=settings_viewport_end())continue;
            settings_card(fb,(EpdRect){36,y,612,SETTINGS_PARENT_ROW_H-12},22,UI_GRAY_WHITE,0x70);
            setting_icon(fb,icons[i],67,y+56);
            ui_text_box(fb,(EpdRect){100,y+8,488,54},27,settings_group_titles[i],EPD_DRAW_ALIGN_LEFT,false);
            ui_text_box(fb,(EpdRect){100,y+66,488,46},19,summaries[i],EPD_DRAW_ALIGN_LEFT,false);
            ui_draw_icon(fb,625,y+56,26,UI_ICON_CHEVRON_RIGHT,UI_GRAY_BLACK);
        }
    }
    char wifi[56];snprintf(wifi,sizeof(wifi),"%s",s_main_wifi_saved?s_main_wifi:"Chưa cấu hình");
    char wifi_tout[32];
    unsigned w_tout = app_settings_wifi_timeout_minutes();
    if (w_tout) snprintf(wifi_tout, sizeof(wifi_tout), "%u phút", w_tout); else strcpy(wifi_tout, "Tắt");
    char ble_tout[32];
    unsigned b_tout = app_settings_ble_timeout_minutes();
    if (b_tout) snprintf(ble_tout, sizeof(ble_tout), "%u phút", b_tout); else strcpy(ble_tout, "Tắt");
    char kosync_val[48];
    if (!app_settings_kosync_user()[0]) {
        snprintf(kosync_val, sizeof(kosync_val), "Chưa cấu hình");
    } else {
        snprintf(kosync_val, sizeof(kosync_val), "%s", app_settings_kosync_auto() ? "Tự động" : "Bật");
    }
    const char *connection_labels[]={
        "WiFi",
        "Tự tắt Wi-Fi khi rảnh",
        "Điều khiển lật trang BLE",
        "Tự tắt Bluetooth khi rảnh",
        "Đồng bộ KOSync",
        "Kho sách OPDS",
        "Ngày & Giờ"
    };
    const char *connection_values[]={
        wifi,
        wifi_tout,
        app_settings_ble_turner()?"Đã bật":"Đã tắt",
        ble_tout,
        kosync_val,
        "Kho trực tuyến",
        "Cài đặt"
    };
    const int connection_icons[]={0,14,1,14,11,4,6};
    setting_group(fb,0,SETTINGS_WIRELESS_Y-49,"Kết nối",SETTINGS_WIRELESS_Y,
                  connection_icons,connection_labels,connection_values,SETTINGS_WIRELESS_ROWS);
    char font_name[96],contrast[24],idle[24],signature[96];
    snprintf(font_name,sizeof(font_name),"%s · %u%%",system_font_label(app_settings_system_font_path()),app_settings_system_font_size());
    snprintf(contrast,sizeof(contrast),"%u%%",app_settings_system_contrast());
    unsigned minutes=app_settings_auto_lock_minutes();
    if(minutes)snprintf(idle,sizeof(idle),"%u phút",minutes);else strcpy(idle,"Tắt");
#if KIIKO_BOARD_METALIO47
    const char *display_labels[]={"Phông chữ hệ thống","Độ tương phản hệ thống","Chế độ làm mới Trang chủ","Làm mới kỹ Trang chủ","Tự động khóa màn hình","Nhấn đúp mở khóa","Chế độ tắt nguồn / ngủ","Đèn nền & Rung"};
#else
    const char *display_labels[]={"Phông chữ hệ thống","Độ tương phản hệ thống","Chế độ làm mới Trang chủ","Làm mới kỹ Trang chủ","Tự động khóa màn hình","Nhấn đúp mở khóa","Chế độ tắt nguồn / ngủ"};
#endif
    const char *display_values[]={font_name,contrast,
        app_settings_main_refresh_mode()==APP_MAIN_REFRESH_FAST?"Làm mới nhanh":app_settings_main_refresh_mode()==APP_MAIN_REFRESH_WATER?"Sóng nước":"Thường",
#if KIIKO_BOARD_METALIO47
        app_settings_home_full_refresh()?"Bật":"Tắt",idle,"",app_settings_staged_shutdown()?"Ngủ nhẹ rồi tắt":"Ngắt nguồn hoàn toàn","Độ sáng & Nhiệt màu"};
    const int display_icons[]={2,7,15,11,14,SETTINGS_UNLOCK_ICON,9,7};
#else
        app_settings_home_full_refresh()?"Bật":"Tắt",idle,"",app_settings_staged_shutdown()?"Ngủ nhẹ rồi sâu":"Ngắt nguồn hoàn toàn"};
    const int display_icons[]={2,7,15,11,14,SETTINGS_UNLOCK_ICON,9};
#endif
    setting_group(fb,1,SETTINGS_DISPLAY_Y-49,"Hiển thị",SETTINGS_DISPLAY_Y,
                  display_icons,display_labels,display_values,SETTINGS_DISPLAY_ROWS);
    snprintf(signature,sizeof(signature),"%s",app_settings_status_signature()[0]?app_settings_status_signature():"Chưa đặt");
    const char *personal_labels[]={"Kiểu hiển thị Tủ sách","Kiểu màn hình khóa","Mật khẩu màn hình khóa","Chữ ký thanh trạng thái"};
    const char *personal_values[]={app_settings_shelf_style()==5?"Danh sách":app_settings_shelf_style()==1?"Thanh kệ tối màu":"Kệ sách Acrylic",
        app_settings_lock_style()==3?"Khung PNG trong suốt":app_settings_lock_style()==2?"Ghép bìa Tủ sách":app_settings_lock_style()==1?"Hình nền":"Thẻ đọc sách",
        lock_pin_enabled()?"Đã bật":"Đã tắt",signature};
    const int personal_icons[]={4,5,16,10};
    setting_group(fb,2,SETTINGS_PERSONAL_Y-49,"Cá nhân hóa",SETTINGS_PERSONAL_Y,
                  personal_icons,personal_labels,personal_values,SETTINGS_PERSONAL_ROWS);
    const char *controls_labels[]={"Phím cứng & Cử chỉ"};
    const char *controls_values[]={"Cài đặt"};
    const int controls_icons[]={1};
    setting_group(fb,3,SETTINGS_CONTROLS_Y-49,"Điều khiển",SETTINGS_CONTROLS_Y,
                  controls_icons,controls_labels,controls_values,SETTINGS_CONTROLS_ROWS);
#if KIIKO_BOARD_METALIO47
    const char *maintenance_labels[]={"Cập nhật hệ thống","Khôi phục thẻ nhớ","Sao lưu & Khôi phục","Nhật ký & Chẩn đoán","Khử bóng mờ màn hình","Nạp ROM (BOOT)","Khôi phục cài đặt gốc"};
    const char *maintenance_values[]={"OTA / Thẻ nhớ","Thẻ nhớ & Phông chữ","Cấu hình","Xuất log ra thẻ nhớ","Làm sạch vệt mờ","Nạp ROM qua máy tính","Đặt lại hệ thống"};
    const int maintenance_icons[]={12,11,8,10,17,13,11};
#else
    const char *maintenance_labels[]={"Cập nhật hệ thống","Sao lưu & Khôi phục","Nhật ký & Chẩn đoán","Khử bóng mờ màn hình","Nạp ROM (BOOT)","Khôi phục cài đặt gốc"};
    const char *maintenance_values[]={"OTA / Thẻ nhớ","Cấu hình","Xuất log ra thẻ nhớ","Làm sạch vệt mờ","Nạp ROM qua máy tính","Đặt lại hệ thống"};
    const int maintenance_icons[]={12,8,10,17,13,11};
#endif
    setting_group(fb,4,SETTINGS_MAINTENANCE_Y-49,"Cập nhật & Khôi phục",SETTINGS_MAINTENANCE_Y,
                  maintenance_icons,maintenance_labels,maintenance_values,SETTINGS_MAINTENANCE_ROWS);
    // 主列表也要有一行提示位：确认、修复结果都写在这里，不然点了没有任何回音。
    // The main list needs a notice line too: confirmation and the repair result land here, or a tap
    // would have no answer at all.
    ui_fill_rect_fast((EpdRect){0,0,UI_LOCK_WIDTH,160},UI_GRAY_WHITE,fb);
    ui_nav_status(fb);
    if(s_settings_group < 0)
        ui_text_box(fb,(EpdRect){36,78,240,76},52,"Cài đặt",EPD_DRAW_ALIGN_LEFT,false);
    else back_header(fb,settings_group_titles[s_settings_group]);
    if(s_notice[0] && s_settings_group<0)ui_text_box(fb,(EpdRect){300,86,348,64},20,s_notice,EPD_DRAW_ALIGN_LEFT,false);
    settings_navigation(fb);
#if KIIKO_BOARD_METALIO47
    if(app_settings_main_refresh_mode()==APP_MAIN_REFRESH_NORMAL){
        // 与书架共用逐相输出：状态栏/题头柔和，内容白像素不反向压黑；不影响其他模式。
        // Reuse shelf scan phases: soft status/title and no reverse-blackening on content whites, without changing other modes.
        const EpdRect content={0,160,UI_LOCK_WIDTH,UI_NAV_REFRESH_END-160};
        display_main_transition_gray_bands(&content,1);
    }
#endif
}

#if KIIKO_BOARD_METALIO47
static bool s_lock_style_return;
void app_device_settings_resume_lock_style(void) { s_lock_style_return=true; }
static bool settings_take_lock_style_return(void) {
    bool pending=s_lock_style_return;s_lock_style_return=false;return pending;
}
static app_redraw_t settings_open_lock_images(app_ctx_t *ctx, const char *path, bool transparent) {
    extern const app_desc_t app_image;
    if(!app_image_request_lock_picker(path,transparent)) {
        snprintf(s_notice,sizeof(s_notice),"Không mở được đường dẫn ảnh, vui lòng chọn lại");
        return APP_REDRAW_PAGE;
    }
    ctx->request_app=&app_image;
    return APP_REDRAW_NONE;
}
static bool settings_gray_preview(void) {
    return s_page==SETTINGS_WALLPAPER_PREVIEW||s_page==SETTINGS_SHELF_STYLE;
}
#endif
static void render(app_ctx_t *ctx,uint8_t *fb) {
    render_content(ctx,fb);
#if KIIKO_BOARD_METALIO47
    if(!settings_gray_preview())display_monochrome_content(fb);
#endif
    settings_navigation(fb);
}

static void settings_prepare_profile(void) {
    const char *path=app_settings_avatar_path();
    if(pico_boot_recovery()||!path[0]){ui_avatar_request("",60);s_avatar_ready=s_avatar_pending=false;s_avatar_path[0]=0;return;}
    if(strcmp(path,s_avatar_path))s_avatar_ready=false;
    snprintf(s_avatar_path,sizeof(s_avatar_path),"%s",path);
    ui_avatar_request(path,82);
    ui_avatar_state_t state=ui_avatar_poll(path,82,s_avatar_pixels);
    s_avatar_ready=state==UI_AVATAR_READY;s_avatar_pending=state==UI_AVATAR_PENDING;
}
static void on_enter(app_ctx_t *ctx) {
#if KIIKO_BOARD_METALIO47
    s_control_pick=s_control_key=-1;
#endif
    (void)ctx;
    pin_setup_close();
    s_notice[0] = 0;
    s_ble_feedback[0] = 0;
    s_page = SETTINGS_MAIN;
    s_settings_group=-1; s_settings_root_scroll=0;
    s_style_scroll = s_main_scroll = s_font_page = s_wallpaper_page = 0;
    s_collage_prepare_pending = false;
    s_scroll_drag_consumed = s_scroll_present_pending = false;
    s_wallpaper_selected = -1;
    s_bookmark_picker=false;bookmark_preview_release();
    s_wallpaper_confirm = s_wallpaper_preview_ok = false;
    s_sync_pending = false;
    s_font_sync_confirm = false; s_font_sync_path[0] = 0;
    s_config_confirm = false;
    s_panel_repair_pending = false;
    s_boot_pending = s_local_pending = s_upgrade_restart = false;
    s_upgrade_offer_pending = s_upgrade_confirm = false;
    (void)read_pico_transfer_get_saved_wifi(s_main_wifi,&s_main_wifi_saved);
    settings_prepare_profile();
#if KIIKO_BOARD_METALIO47
    if(settings_take_lock_style_return()){s_page=SETTINGS_LOCK_STYLE;s_settings_group=2;}
#endif
}

// 主循环主动消费按键；识别不需要再触摸屏幕。/ Consume key edges on ticks without requiring another touch.
static bool ble_receive_feedback(void) {
    ble_pt_event_t unused;
    while (ble_pt_pop_key(&unused)) {}
    bool changed = false;
    ble_pt_raw_t raw;
    while (ble_pt_pop_raw(&raw)) {
        if (!raw.pressed || !ble_pt_connected()) continue;
        if (s_ble_learning) {
            const ble_pt_action_t action = s_ble_learning == 1 ? BLE_PT_ACTION_PREV : BLE_PT_ACTION_NEXT;
            const esp_err_t error = ble_pt_bind(action, &raw.frame);
            snprintf(s_ble_feedback, sizeof(s_ble_feedback), "%s",
                     error == ESP_OK ? (action == BLE_PT_ACTION_PREV ? "Nhận diện thành công · Đã gán phím Trang trước" : "Nhận diện thành công · Đã gán phím Trang sau")
                                     : "Nhận diện được nhưng lưu thất bại, vui lòng thử lại");
            s_ble_learning = 0;
            s_ble_scroll = 0;
            changed = true;
            // 同一帧的其余字节不得覆盖这次学习结果。/ Drain remaining bytes without overwriting this learned result.
            while (ble_pt_pop_raw(&raw)) {}
            break;
        }
        if (!s_ble_feedback[0]) {
            snprintf(s_ble_feedback, sizeof(s_ble_feedback), "Nhận diện thành công · Đã nhận tín hiệu phím");
            s_ble_scroll = 0;
            changed = true;
        }
    }
    return changed;
}

static app_redraw_t profile_editor_paint(app_ctx_t *ctx, bool field);
static app_redraw_t reset_tick(void) {
    s_reset_pending=false;
    read_pico_transfer_status_t state;read_pico_transfer_get_status(&state);
    esp_err_t error=ESP_ERR_INVALID_STATE;
    if(!pico_online_busy() && state.state!=READ_PICO_TRANSFER_UPLOADING &&
       book_cover_jobs_quiesce(2000) && ui_avatar_quiesce(2000)) {
        ble_pt_cancel_recent();
        error=ble_pt_stop(2000);
        if(error==ESP_OK) {
            read_pico_transfer_stop();
            error=app_factory_reset();
        }
    }
    if(error==ESP_OK) {esp_restart();return APP_REDRAW_NONE;}
    s_reset_confirm=false;
    snprintf(s_reset_notice,sizeof(s_reset_notice),"Khôi phục chưa hoàn tất, hãy thử lại: %s",esp_err_to_name(error));
    return APP_REDRAW_PAGE;
}

static app_redraw_t on_tick(app_ctx_t *ctx) {
#if KIIKO_BOARD_METALIO47
    if(s_page==SETTINGS_SD_RECOVERY) {
        if(!app_sd_recovery_tick(ctx->now_ms))return APP_REDRAW_NONE;
        if(!app_sd_recovery_busy())settings_prepare_profile();
        return APP_REDRAW_PAGE;
    }
    if(s_page==SETTINGS_ONLINE_FONTS)return ui_online_fonts_tick(&s_online_fonts,ctx->now_ms);
#endif
    // 屏幕修复：先画最终结果，再在 tick 中整屏恢复；成功后不重复推页。
    // Paint the final notice before the tick's full restoration; do not push the page twice on success.
    if (s_panel_repair_pending) {
        s_panel_repair_pending = false;
        if (s_page != SETTINGS_MAIN) return APP_REDRAW_NONE;
        s_scroll_present_pending = false;
        snprintf(s_notice, sizeof(s_notice), "Đã làm sạch màn hình; nếu còn vệt mờ có thể chạy thêm lần nữa");
        render(ctx, ctx->fb);
        if (display_panel_repair(ctx->hl) != EPD_DRAW_SUCCESS) {
            snprintf(s_notice, sizeof(s_notice), "Màn hình đang bận hiển thị, vui lòng thử lại sau");
            return APP_REDRAW_FULL;
        }
        return APP_REDRAW_DONE;
    }

    if (s_page==SETTINGS_MAIN && s_settings_group<0 && s_avatar_pending && !(ctx->touch && ctx->touch->touched)) {
        ui_avatar_state_t state=ui_avatar_poll(s_avatar_path,82,s_avatar_pixels);
        s_avatar_pending=state==UI_AVATAR_PENDING;
        s_avatar_ready=state==UI_AVATAR_READY;
        if (s_avatar_ready && s_main_scroll < 180+settings_profile_height()-160) {
            render(ctx,ctx->fb);
            EpdRect area={57,180-s_main_scroll+(settings_profile_height()-82)/2,82,82};
            if (area.y<160) { area.height-=160-area.y; area.y=160; }
            if(area.height>0)guard_draw_result(ctx->hl,update_display_area_diff_with(ctx->hl,
#if KIIKO_BOARD_METALIO47
                display_main_bw_waveform(),MODE_DU,area));
#else
                app_settings_main_refresh_mode()==APP_MAIN_REFRESH_NORMAL?display_main_normal_waveform():display_main_gray_waveform(),MODE_GL16,area));
#endif
            return APP_REDRAW_DONE;
        }
    }
    if(s_reset_pending && s_page==SETTINGS_FACTORY_RESET)return reset_tick();
    if (s_page == SETTINGS_PIN_ENTRY) return pin_setup_tick(ctx);
#if KIIKO_BOARD_METALIO47
    // 选择拼贴只保存样式；实际锁屏按原有缓存路径生成，不在设置tick里阻塞导航。
    // Selecting collage saves its style only; build through the existing lock/cache path without blocking settings navigation.
#else
    if (s_collage_prepare_pending && s_page == SETTINGS_LOCK_STYLE) {
        s_collage_prepare_pending = false;
        // 先让选中反馈可见，再生成卡上缓存；render 本身只绘图。
        // Show selection feedback first, then prepare the SD cache; render stays paint-only.
        const size_t bytes = UI_LOCK_WIDTH * UI_LOCK_HEIGHT / 2u;
        size_t available = heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        uint8_t *preview = available > bytes + 1024u * 1024u
            ? heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) : NULL;
        if (preview) {
            app_lock_font_t saved;
            app_font_begin_lock(&saved);
            (void)book_lock_collage_draw(preview);
            app_font_end_lock(&saved);
            free(preview);
        }
        return APP_REDRAW_NONE;
    }
#endif

    if (s_page == SETTINGS_TEXT_EDIT) {
        if (ctx->consumed) return APP_REDRAW_NONE;
        bool held = !ctx->released && ctx->touch && ctx->touch->touched && ctx->touch->count == 1;
        if (ui_keyboard_hold_tick(held, ctx->touch ? ctx->touch->x : 0,
            ctx->touch ? ctx->touch->y : 0, ctx->now_ms) == UI_KEYBOARD_CHANGED) return profile_editor_paint(ctx, false);
        return ui_keyboard_idle_tick(ctx->touch && ctx->touch->touched, ctx->now_ms) == UI_KEYBOARD_CHANGED
            ? profile_editor_paint(ctx, false) : APP_REDRAW_NONE;
    }
    if (s_page == SETTINGS_BLUETOOTH && ble_receive_feedback()) return APP_REDRAW_PAGE;
    // 蓝牙启动失败的原因转成提示。放在这里而不是 render()：render 必须是纯绘制，
    // 而 take_* 会清空状态。
    // Fold a Bluetooth start failure into the notice here rather than in render(), which must
    // stay pure, and because take_* clears the state.
    // 蓝牙页要跟着栈的状态重绘：开关拨开后栈是异步起来的，配对表、扫描结果和连接状态
    // 都在页面画完以后才变。不重绘的话用户看到的永远是进页那一刻的快照，只能退出去再进来。
    // The Bluetooth pages redraw with the stack: enabling it starts it asynchronously, and the
    // bond list, scan results and link state all change after the page was painted. Without a
    // poll the user only ever sees the snapshot from when they entered, and has to leave and
    // come back.
    if (s_page == SETTINGS_BLUETOOTH || s_page == SETTINGS_BLE_SCAN) {
        static uint32_t ble_seen;
        const uint32_t sig = (app_settings_ble_turner() ? 1u : 0u) | (ble_pt_running() ? 2u : 0u) |
                             (ble_pt_connected() ? 4u : 0u) | (ble_pt_connecting() ? 8u : 0u) |
                             (ble_pt_scanning() ? 16u : 0u) |
                             ((uint32_t)ble_pt_bond_count() << 8) |
                             ((uint32_t)ble_scan_visible_count() << 16);
        if (sig != ble_seen) {
            ble_seen = sig;
            return APP_REDRAW_PAGE;
        }
    }
    if (s_page == SETTINGS_BLUETOOTH && !s_ble_notice[0]) {
        char why[64];
        if (ble_pt_take_start_failure(why, sizeof(why))) {
            snprintf(s_ble_notice, sizeof(s_ble_notice), "%s", why);
            return APP_REDRAW_PAGE;
        }
    }

    (void)ctx;
    if (s_boot_pending) {
        s_boot_pending = false;
#if KIIKO_BOARD_METALIO47
        esp_err_t err=app_enter_download();
        s_boot_connected=metalio_download_connected();
        if (err==ESP_OK) s_boot_notice[0]=0;
        else snprintf(s_boot_notice,sizeof(s_boot_notice),err==ESP_ERR_NOT_FINISHED ?
            "Bộ nhớ hoặc truyền tệp đang bận, vui lòng thử lại sau" : "Chuyển chế độ BOOT lỗi: %s",esp_err_to_name(err));
        return APP_REDRAW_PAGE;
    }
    if (s_page==SETTINGS_BOOT && app_download_active()) {
        const bool connected=metalio_download_connected();
        if (connected!=s_boot_connected) { s_boot_connected=connected; return APP_REDRAW_PAGE; }
#else
        pmu_selftest_prepare_powerdown();
        uint8_t req[2] = {0, 0};
        esp_err_t error = read_pico_pmu_cmd(PMU_CMD_HOST_REQUEST_RESET, req, sizeof(req));
        if (error != ESP_OK) ESP_LOGW(TAG, "BOOT PMU notice: %s", esp_err_to_name(error));
        REG_WRITE(RTC_CNTL_OPTION1_REG, RTC_CNTL_FORCE_DOWNLOAD_BOOT);
        esp_restart(); return APP_REDRAW_NONE;
#endif
    }
    if (s_upgrade_restart) { s_upgrade_restart = false; esp_restart(); return APP_REDRAW_NONE; }
    if (s_page == SETTINGS_UPGRADE) {
        if (s_upgrade_job != UPGRADE_JOB_NONE) return upgrade_start_pending();
        if (s_local_pending) {
            s_local_pending = false;
#if KIIKO_BOARD_METALIO47
            esp_err_t error = install_local_update();
#else
            esp_err_t error = pico_ota_install(PICO_OTA_UPDATE_PATH, s_upgrade_notice, sizeof(s_upgrade_notice));
#endif
            s_upgrade_restart = error == ESP_OK;
            return APP_REDRAW_PAGE;
        }
        pico_online_get_status(&s_update);
        if (s_update.state == PICO_UPDATE_READY && !s_update.busy) {
            if (pico_online_commit() == ESP_OK) { esp_restart(); return APP_REDRAW_NONE; }
            pico_online_get_status(&s_update);
        }
        uint32_t now = esp_timer_get_time() / 1000;
        app_redraw_t redraw = upgrade_status_redraw(now);
        if (redraw == APP_REDRAW_AREA) draw_upgrade_progress_delta(ctx->fb);
        return redraw;
    }
    if (!s_sync_pending) return APP_REDRAW_NONE;
    s_sync_pending = false;
    uint32_t utc = 0;
    read_pico_transfer_status_t network = {0};
    read_pico_transfer_get_status(&network);
    esp_err_t err = read_pico_transfer_sync_time(&utc);
    bool network_time_received = err == ESP_OK;
    if (network_time_received) {
        // 临时 WiFi 收尾可能耗时；写入 PMU 时取当前 S3 时间，避免回写旧秒数。
        // Temporary WiFi teardown takes time; seed the PMU from the current S3 clock.
        utc = (uint32_t)time(NULL);
        uint8_t payload[4] = {(uint8_t)utc, (uint8_t)(utc >> 8), (uint8_t)(utc >> 16), (uint8_t)(utc >> 24)};
        err = read_pico_pmu_cmd(PMU_CMD_TIME_SYNC, payload, sizeof(payload));
        if (err == ESP_OK) err = read_pico_pmu_cmd(PMU_CMD_TIME_GET, NULL, 0);
        if (err == ESP_OK) {
            const pmu_snapshot_t *pmu = read_pico_pmu_get();
            if (!pmu || !pmu->time_synced || pmu->unix_sec < utc || pmu->unix_sec - utc > 10)
                err = ESP_ERR_INVALID_RESPONSE;
        }
    }
    ESP_LOGI(TAG, "time sync network=%d ready=%d utc=%lu result=%s",
             network.mode, network.network_ready, (unsigned long)utc, esp_err_to_name(err));
    const char *notice = err == ESP_OK ? "Đã đồng bộ giờ qua WiFi và lưu vào RTC" :
        network_time_received ? "Đã lấy giờ mạng nhưng ghi vào RTC lỗi" :
        err == ESP_ERR_NOT_FOUND ? "Vui lòng kết nối mạng trong phần Cài đặt WiFi trước" :
        err == ESP_ERR_INVALID_STATE ? "WiFi chưa kết nối, vui lòng thử lại sau" :
        err == ESP_ERR_TIMEOUT ? "Quá thời gian đồng bộ giờ, hãy kiểm tra WiFi rồi thử lại" :
        "Đồng bộ giờ thất bại, hãy kiểm tra WiFi rồi thử lại";
    snprintf(s_notice, sizeof(s_notice), "%s", notice);
    return APP_REDRAW_PAGE;
}

static EpdRect s_input_area;
static bool s_input_layout, s_input_settle;
static app_redraw_t profile_editor_paint(app_ctx_t *ctx, bool field) {
    ui_keyboard_update_t update = ui_keyboard_update(ctx->fb, UI_SYSTEM_KEYBOARD_TOP, (EpdRect){36, 243, 612, 82}, 28, false, NULL, field);
    s_input_area = update.area; s_input_layout = update.layout; s_input_settle = update.settle;
    return update.area.width ? APP_REDRAW_AREA : APP_REDRAW_NONE;
}
static app_redraw_t profile_editor_gesture(app_ctx_t *ctx, const ui_gesture_event_t *ev) {
    if (ev->type == UI_GESTURE_PRESS)
        return ui_keyboard_press(ev->x0, ev->y0, UI_SYSTEM_KEYBOARD_TOP, ctx->now_ms) ? profile_editor_paint(ctx, false) : APP_REDRAW_NONE;
    bool feedback = ev->type != UI_GESTURE_LONG_PRESS && ui_keyboard_release();
    if (ev->type == UI_GESTURE_SWIPE_L || ev->type == UI_GESTURE_SWIPE_R) {
        bool page = ui_keyboard_page(ev->type == UI_GESTURE_SWIPE_L ? 1 : -1);
        return (page || feedback) ? profile_editor_paint(ctx, false) : APP_REDRAW_NONE;
    }
    if (ev->type != UI_GESTURE_TAP) return feedback ? profile_editor_paint(ctx, false) : APP_REDRAW_NONE;
    int x = ev->x0, y = ev->y0;
    if (y < 160) {
        if (x < 160) {
            ui_keyboard_end();
            if (s_editor_mode == EDITOR_SIGNATURE) s_page = SETTINGS_MAIN;
            else if (s_editor_mode == EDITOR_DEVICE_NAME) s_page = SETTINGS_PROFILE;
            else if (s_editor_mode == EDITOR_KOSYNC_URL || s_editor_mode == EDITOR_KOSYNC_USER || s_editor_mode == EDITOR_KOSYNC_PASS)
                s_page = SETTINGS_KOSYNC;
            else
                s_page = SETTINGS_OPDS;
            return APP_REDRAW_PAGE;
        }
        if (x > 510) goto save_text;
    }
    if (ui_text_input_tap(&s_editor_input, (EpdRect){36, 243, 612, 82}, 28,
                          false, NULL, x, y)) return profile_editor_paint(ctx, true);
    ui_keyboard_result_t result = ui_keyboard_tap(x, y, UI_SYSTEM_KEYBOARD_TOP, esp_timer_get_time() / 1000);
    if (result == UI_KEYBOARD_DONE) goto save_text;
    return result == UI_KEYBOARD_CHANGED ? profile_editor_paint(ctx, false) : APP_REDRAW_NONE;
save_text:
    if (ui_keyboard_pending()) {
        snprintf(s_editor_notice, sizeof(s_editor_notice), "Vui lòng chọn từ gợi ý trước");
        return APP_REDRAW_PAGE;
    }
    ui_keyboard_end();
    switch (s_editor_mode) {
        case EDITOR_DEVICE_NAME:
            if (!s_editor[0]) {
                snprintf(s_editor_notice, sizeof(s_editor_notice), "Tên thiết bị không được để trống");
                return APP_REDRAW_PAGE;
            }
            app_settings_set_device_name(s_editor);
            s_page = SETTINGS_PROFILE;
            break;
        case EDITOR_SIGNATURE:
            app_settings_set_status_signature(s_editor);
            s_page = SETTINGS_MAIN;
            break;
        case EDITOR_KOSYNC_URL:
            if (!s_editor[0]) app_settings_set_kosync_url(DPREADER_DEFAULT_KOSYNC_URL);
            else app_settings_set_kosync_url(s_editor);
            snprintf(s_kosync_notice, sizeof(s_kosync_notice), "Đã cập nhật máy chủ KOSync");
            s_page = SETTINGS_KOSYNC;
            break;
        case EDITOR_KOSYNC_USER:
            app_settings_set_kosync_user(s_editor);
            snprintf(s_kosync_notice, sizeof(s_kosync_notice), "Đã lưu tài khoản KOSync");
            s_page = SETTINGS_KOSYNC;
            break;
        case EDITOR_KOSYNC_PASS:
            if (s_editor[0]) {
                char md5_hex[33] = {0};
                kosync_md5_string(s_editor, md5_hex);
                app_settings_set_kosync_key(md5_hex);
                snprintf(s_kosync_notice, sizeof(s_kosync_notice), "Đã cập nhật mật khẩu KOSync");
            }
            s_page = SETTINGS_KOSYNC;
            break;
        case EDITOR_OPDS_URL:
            if (!s_editor[0]) app_settings_set_opds_url(DPREADER_DEFAULT_OPDS_URL);
            else app_settings_set_opds_url(s_editor);
            snprintf(s_opds_notice, sizeof(s_opds_notice), "Đã cập nhật kho sách OPDS");
            s_page = SETTINGS_OPDS;
            break;
        case EDITOR_OPDS_USER:
            app_settings_set_opds_user(s_editor);
            snprintf(s_opds_notice, sizeof(s_opds_notice), "Đã lưu tài khoản OPDS");
            s_page = SETTINGS_OPDS;
            break;
        case EDITOR_OPDS_PASS:
            app_settings_set_opds_pass(s_editor);
            snprintf(s_opds_notice, sizeof(s_opds_notice), "Đã lưu mật khẩu OPDS");
            s_page = SETTINGS_OPDS;
            break;
    }
    return APP_REDRAW_PAGE;
}

// 越过阈值按段推进，一次手势只更新一次，松手不再播放惯性。
// Advance one discrete step after crossing the threshold, with one update per gesture and no release inertia.
static app_redraw_t scroll_gesture(const ui_gesture_event_t *ev, int *offset, int limit, bool *handled) {
    *handled = ev->type != UI_GESTURE_TAP || s_scroll_drag_consumed;
    if (ev->type == UI_GESTURE_PRESS) {
        s_scroll_drag_consumed = false;
        return APP_REDRAW_NONE;
    }
    if (ev->type == UI_GESTURE_TAP || ev->type == UI_GESTURE_CANCEL) {
        s_scroll_drag_consumed = false;
        return APP_REDRAW_NONE;
    }
    if (ev->type != UI_GESTURE_MOVE && ev->type != UI_GESTURE_SWIPE_U && ev->type != UI_GESTURE_SWIPE_D)
        return APP_REDRAW_NONE;
    if (ev->y0 < (s_page == SETTINGS_MAIN ? 160 : 190) || ev->y0 >= settings_viewport_end())
        return APP_REDRAW_NONE;
    if (s_scroll_drag_consumed) {
        if (ev->type != UI_GESTURE_MOVE) s_scroll_drag_consumed = false;
        return APP_REDRAW_NONE;
    }
    int dy = (int)ev->y - ev->y0, dx = (int)ev->x - ev->x0;
    int distance = abs(dy);
    if (ev->type == UI_GESTURE_MOVE && (distance < 108 || distance <= abs(dx))) return APP_REDRAW_NONE;
    int step = distance < 204 ? 204 : distance > 450 ? 450 : distance;
    bool up = ev->type == UI_GESTURE_SWIPE_U || (ev->type == UI_GESTURE_MOVE && dy < 0);
    int next = *offset + (up ? step : -step);
    if (limit < 0) limit = 0;
    if (next < 0) next = 0;
    if (next > limit) next = limit;
    if (ev->type == UI_GESTURE_MOVE) s_scroll_drag_consumed = true;
    if (next == *offset) return APP_REDRAW_NONE;
    *offset = next;
    s_scroll_present_pending = true;
    return APP_REDRAW_AREA;
}

static app_redraw_t reset_gesture(app_ctx_t *ctx,const ui_gesture_event_t *ev) {
    if(ev->type!=UI_GESTURE_TAP || s_reset_pending)return APP_REDRAW_NONE;
    int tab=settings_nav_hit(ev->x0,ev->y0);
    if(tab>=0){s_reset_confirm=false;ui_nav_request(ctx,tab);return APP_REDRAW_NONE;}
    if((ev->y0<190 && ev->x0<170)||ui_rect_hit((EpdRect){54,950,576,78},ev->x0,ev->y0)) {
        s_reset_confirm=false;s_page=SETTINGS_MAIN;return APP_REDRAW_PAGE;
    }
    if(ui_rect_hit((EpdRect){54,850,576,78},ev->x0,ev->y0)) {
        s_reset_notice[0]=0;
        if(s_reset_confirm)s_reset_pending=true;
        else s_reset_confirm=true;
        return APP_REDRAW_PAGE;
    }
    return APP_REDRAW_NONE;
}

#if KIIKO_BOARD_METALIO47
static app_redraw_t sd_recovery_gesture(app_ctx_t *ctx,const ui_gesture_event_t *ev) {
    if(app_sd_recovery_busy() || ev->type!=UI_GESTURE_TAP)return APP_REDRAW_NONE;
    if(ui_rect_hit(sd_recovery_button(),ev->x0,ev->y0)) {
        esp_err_t error=app_sd_recovery_begin(ctx->now_ms);
        snprintf(s_sd_notice,sizeof(s_sd_notice),"%s",error==ESP_OK?"":
            "Hãy thoát Truyền sách, USB và Cập nhật trước khi thử lại");
        return APP_REDRAW_PAGE;
    }
    if(ev->y0<190 && ev->x0<170) {s_page=SETTINGS_MAIN;settings_prepare_profile();return APP_REDRAW_PAGE;}
    int tab=settings_nav_hit(ev->x0,ev->y0);
    if(tab>=0)ui_nav_request(ctx,tab);
    return APP_REDRAW_NONE;
}
#endif

static app_redraw_t on_gesture(app_ctx_t *ctx, const ui_gesture_event_t *ev) {
#if KIIKO_BOARD_METALIO47
    if(app_sd_recovery_busy())return APP_REDRAW_NONE;
    if(s_page==SETTINGS_SD_RECOVERY)return sd_recovery_gesture(ctx,ev);
    if(s_page==SETTINGS_ONLINE_FONTS){
        app_redraw_t redraw=ui_online_fonts_gesture(&s_online_fonts,ev);
        if(s_online_fonts.closed){online_font_cancel_join();ttf_font_scan();s_font_page=0;s_page=SETTINGS_SYSTEM_FONT;}
        return redraw;
    }
    if(s_page==SETTINGS_SYSTEM_FONT&&!s_font_sync_confirm&&ev->type==UI_GESTURE_TAP&&
       ui_rect_hit(ui_online_fonts_entry_rect(79),ev->x0,ev->y0)){
        ui_online_fonts_open(&s_online_fonts,false);s_page=SETTINGS_ONLINE_FONTS;return APP_REDRAW_PAGE;
    }
#endif
    if (s_page==SETTINGS_SYSTEM_FONT && s_font_sync_confirm) {
        if (ev->type!=UI_GESTURE_TAP) return APP_REDRAW_NONE;
        for (int i=0;i<2;++i) {
            if (!ui_rect_hit(system_font_sync_button(i),ev->x0,ev->y0)) continue;
            if (i) {
                app_settings_set_font_path(s_font_sync_path);
                if (strcmp(app_settings_font_path(),s_font_sync_path)) {
                    snprintf(s_notice,sizeof(s_notice),"Lưu thất bại, vui lòng thử lại");return APP_REDRAW_PAGE;
                }
            }
            s_notice[0]=0;s_font_sync_confirm=false;return APP_REDRAW_PAGE;
        }
        return APP_REDRAW_NONE;
    }
    if (s_page == SETTINGS_PIN_ENTRY) {
        EpdRect dirty; ui_pin_result_t result = ui_pinpad_handle(&s_pinpad, ev, &dirty);
        if (result == UI_PIN_CANCEL) { pin_setup_close(); s_page = SETTINGS_LOCK_PIN; return APP_REDRAW_PAGE; }
        if (result == UI_PIN_COMPLETE) s_pin_pending = true;
        if (result == UI_PIN_NONE) return APP_REDRAW_NONE;
        ui_pinpad_paint_input(ctx->fb, &s_pinpad);
        guard_draw_result(ctx->hl, update_display_area_diff_with(ctx->hl, &E0470_FOLLOW_WAVEFORM, MODE_DU, dirty));
        return APP_REDRAW_DONE;
    }
    if (s_page == SETTINGS_LOCK_PIN && ev->type == UI_GESTURE_TAP && lock_pin_available() &&
        (ui_rect_hit((EpdRect){36,406,612,settings_detail_row_height()},ev->x0,ev->y0) ||
         (lock_pin_enabled() && ui_rect_hit((EpdRect){36,512,612,settings_detail_row_height()},ev->x0,ev->y0)))) {
        if (ctx->now_ms < s_pin_retry_at) { snprintf(s_notice, sizeof(s_notice), "Vui lòng đợi giây lát rồi thử lại"); return APP_REDRAW_PAGE; }
        pin_setup_open(ctx, ev->y0 >= 512); return APP_REDRAW_PAGE;
    }

#if KIIKO_BOARD_METALIO47
    if(s_page==SETTINGS_LOCK_PIN && ev->type==UI_GESTURE_TAP && lock_pin_available() && lock_pin_enabled() &&
       ui_rect_hit(pin_grace_row(),ev->x0,ev->y0)) {
        app_settings_set_pin_grace_code((app_settings_pin_grace_code()+1)%5);
        return APP_REDRAW_PAGE;
    }
#endif

    if (s_page == SETTINGS_BOOT) {
        if (ev->type != UI_GESTURE_TAP || s_boot_pending) return APP_REDRAW_NONE;
#if KIIKO_BOARD_METALIO47
        if (ev->y0 < 190 && ev->x0 < 170) { app_cancel_download(); s_page = SETTINGS_MAIN; (void)read_pico_transfer_get_saved_wifi(s_main_wifi,&s_main_wifi_saved); settings_prepare_profile(); return APP_REDRAW_PAGE; }
        if (ui_rect_hit((EpdRect){60, 438, 564, 78}, ev->x0, ev->y0)) { s_boot_notice[0]=0; s_boot_pending = true; return APP_REDRAW_PAGE; }
#else
        if (ev->y0 < 190 && ev->x0 < 170) { s_page = SETTINGS_MAIN; (void)read_pico_transfer_get_saved_wifi(s_main_wifi,&s_main_wifi_saved); settings_prepare_profile(); return APP_REDRAW_PAGE; }
        if (ui_rect_hit((EpdRect){60, 438, 564, 78}, ev->x0, ev->y0)) { s_boot_pending = true; return APP_REDRAW_PAGE; }
#endif
        int tab = settings_nav_hit(ev->x0, ev->y0);
        if (tab >= 0) ui_nav_request(ctx, tab);
        return APP_REDRAW_NONE;
    }
    if (s_page == SETTINGS_UPGRADE) {
        if (ev->type != UI_GESTURE_TAP || s_local_pending || s_upgrade_restart) return APP_REDRAW_NONE;
        int y = ev->y0;
        if (y < 190 && ev->x0 < 170) {
            s_upgrade_job = UPGRADE_JOB_NONE;
            s_upgrade_offer_pending = false;
            if (s_upgrade_confirm) s_upgrade_confirm = false;
            else { pico_online_cancel_join(); s_page = SETTINGS_MAIN; }
            return APP_REDRAW_PAGE;
        }
        int tab = settings_nav_hit(ev->x0, y);
        if (tab >= 0) { s_upgrade_job = UPGRADE_JOB_NONE; pico_online_cancel_join(); ui_nav_request(ctx, tab); return APP_REDRAW_NONE; }
        if (s_upgrade_confirm) {
            bool accept = ui_rect_hit(upgrade_confirm_button(s_upgrade_online, true), ev->x0, y);
            if (accept || ui_rect_hit(upgrade_confirm_button(s_upgrade_online, false), ev->x0, y)) {
                s_upgrade_confirm = false;
                s_upgrade_offer_pending = false;
                if (accept) {
                    if (s_upgrade_online) {
                        upgrade_schedule(true, esp_timer_get_time() / 1000);
                    } else s_local_pending = true;
                }
                return APP_REDRAW_PAGE;
            }
            return APP_REDRAW_NONE;
        }
        if (ev->x0 < 60 || ev->x0 >= 624) return APP_REDRAW_NONE;
        if (y >= 550 && y < 612) {
            s_upgrade_notice[0] = 0;
            if (s_update.busy) { s_upgrade_job = UPGRADE_JOB_NONE; s_upgrade_offer_pending = false; pico_online_cancel_join(); }
            else if (s_update.state == PICO_UPDATE_AVAILABLE) { s_upgrade_confirm = true; s_upgrade_online = true; }
            else {
                upgrade_schedule(false, esp_timer_get_time() / 1000);
                return APP_REDRAW_PAGE;
            }
            pico_online_get_status(&s_update);
            upgrade_remember(esp_timer_get_time() / 1000);
            return APP_REDRAW_PAGE;
        }
        if (y >= 840 && y < 902 && !s_update.busy) {
            (void)pico_ota_inspect(PICO_OTA_UPDATE_PATH, &s_local_update);
            if (s_local_update.ready) { s_upgrade_confirm = true; s_upgrade_online = false; }
            else snprintf(s_upgrade_notice, sizeof(s_upgrade_notice), "%s", s_local_update.message);
            return APP_REDRAW_PAGE;
        }
        return APP_REDRAW_NONE;
    }
    if(s_page==SETTINGS_FACTORY_RESET)return reset_gesture(ctx,ev);
    if (s_page == SETTINGS_TEXT_EDIT) return profile_editor_gesture(ctx, ev);
    if (s_page == SETTINGS_MAIN || s_page == SETTINGS_BLUETOOTH || s_page == SETTINGS_BLE_SCAN) {
        int limit = SETTINGS_SCROLL_MAX;
        int *offset = &s_main_scroll;
        if (s_page != SETTINGS_MAIN) {
            int bottom = s_page == SETTINGS_BLUETOOTH ? ble_layout().content_bottom + s_ble_scroll : ble_scan_layout().content_bottom;
            limit = bottom - settings_content_end() + 12;
            offset = &s_ble_scroll;
        }
        bool handled;
        app_redraw_t redraw = scroll_gesture(ev, offset, limit, &handled);
        if (handled) return redraw;
    }

    if (s_page == SETTINGS_AVATAR && (ev->type == UI_GESTURE_SWIPE_U || ev->type == UI_GESTURE_SWIPE_D)) {
        int pages = (s_wallpaper_count + 6) / 7;
        int old = s_wallpaper_page;
        if (ev->type == UI_GESTURE_SWIPE_U && s_wallpaper_page + 1 < pages) ++s_wallpaper_page;
        if (ev->type == UI_GESTURE_SWIPE_D && s_wallpaper_page > 0) --s_wallpaper_page;
        if (s_wallpaper_page == old) return APP_REDRAW_NONE;
        s_scroll_present_pending = true;
        return APP_REDRAW_AREA;
    }
    if (s_page == SETTINGS_WALLPAPER_PREVIEW && !s_wallpaper_confirm &&
        (ev->type == UI_GESTURE_SWIPE_U || ev->type == UI_GESTURE_SWIPE_D)) {
        int next = s_wallpaper_selected + (ev->type == UI_GESTURE_SWIPE_U ? 1 : -1);
        if (next < 0 || next >= s_wallpaper_count) return APP_REDRAW_NONE;
        s_wallpaper_selected = next;
        bookmark_preview_prepare(ctx->fb);
        s_wallpaper_page = next / 8;
        return APP_REDRAW_PAGE;
    }
    if ((s_page == SETTINGS_SYSTEM_FONT || s_page == SETTINGS_WALLPAPER) &&
        (ev->type == UI_GESTURE_SWIPE_U || ev->type == UI_GESTURE_SWIPE_D)) {
        int *page = s_page == SETTINGS_SYSTEM_FONT ? &s_font_page : &s_wallpaper_page;
        int count = s_page == SETTINGS_SYSTEM_FONT ? ttf_font_count() + 1 : s_wallpaper_count;
        int slots=s_page==SETTINGS_SYSTEM_FONT?system_font_layout().slots:8;
        int pages = count > 0 ? (count + slots - 1) / slots : 1;
        int old = *page;
        if (ev->type == UI_GESTURE_SWIPE_U && *page + 1 < pages) ++*page;
        if (ev->type == UI_GESTURE_SWIPE_D && *page > 0) --*page;
        if (*page == old) return APP_REDRAW_NONE;
        s_scroll_present_pending = true;
        return APP_REDRAW_AREA;
    }
    if (ev->type != UI_GESTURE_TAP) return APP_REDRAW_NONE;
    if (s_page == SETTINGS_WALLPAPER_PREVIEW && s_wallpaper_confirm) {
        if (ev->y0 >= 643 && ev->y0 < 711 && ev->x0 >= 358 && ev->x0 < 590) {
            const char *path = s_wallpapers[s_wallpaper_selected].path;
            if(s_bookmark_picker)app_settings_set_bookmark_path(path);
            else app_settings_set_wallpaper_path(path);
            if (!strcmp(s_bookmark_picker?app_settings_bookmark_path():app_settings_wallpaper_path(), path)) {
                app_settings_set_lock_style(s_bookmark_picker?3:1);
                s_page = SETTINGS_LOCK_STYLE;
                bookmark_preview_release();
            }
        }
        s_wallpaper_confirm = false;
        return APP_REDRAW_PAGE;
    }
    if (s_page == SETTINGS_TIME_EDIT) {
        if (ev->y0 < 190) { s_page = SETTINGS_TIME; return APP_REDRAW_PAGE; }
        if (ev->y0 >= 354 && ev->y0 < 597) {
            int field = (ev->x0 - 36) / 122;
            if (ev->x0 >= 36 && field >= 0 && field < 5 && (ev->x0 - 36) % 122 < 106) {
                if (ev->y0 < 416) time_adjust(field, 1);
                else if (ev->y0 >= 535) time_adjust(field, -1);
                else return APP_REDRAW_NONE;
                return APP_REDRAW_PAGE;
            }
        }
        if (ev->y0 >= 767 && ev->y0 < 845) { (void)time_save(); return APP_REDRAW_PAGE; }
        if (ev->y0 >= 874 && ev->y0 < 952) { s_page = SETTINGS_TIME; return APP_REDRAW_PAGE; }
        return APP_REDRAW_NONE;
    }
    int tab = settings_nav_hit(ev->x0, ev->y0);
    if (tab >= 0) { ui_nav_request(ctx, tab); return APP_REDRAW_NONE; }
    int y = ev->y0;
#if KIIKO_BOARD_METALIO47
    if(s_page==SETTINGS_FRONTLIGHT) {
        if(y<190) { s_page=SETTINGS_MAIN; return APP_REDRAW_PAGE; }
        unsigned brightness=metalio_brightness(), warmth=metalio_warmth();
        if(y>=390 && y<458) brightness=ev->x0<342?(brightness>=10?brightness-10:0):(brightness<=90?brightness+10:100);
        else if(y>=620 && y<688) warmth=ev->x0<342?(warmth>=10?warmth-10:0):(warmth<=90?warmth+10:100);
        else if(y>=750 && y<818) { metalio_frontlight_toggle(); return APP_REDRAW_PAGE; }
        else if(y>=850 && y<918) { metalio_haptic_set_enabled(!metalio_haptic_enabled()); return APP_REDRAW_PAGE; }
        else return APP_REDRAW_NONE;
        metalio_frontlight_adjust_position(y>=620,(y>=620?warmth:brightness)*METALIO_FRONTLIGHT_RANGE/100);
        metalio_frontlight_commit(); return APP_REDRAW_PAGE;
    }
    if(s_page==SETTINGS_CONTROLS) {
        if(s_control_pick>=0){
            if(ui_rect_hit((EpdRect){48,944,588,60},ev->x0,y)||y<176||y>=1064){
                s_control_pick=-1;s_notice[0]=0;return APP_REDRAW_PAGE;
            }
            for(int i=0;i<controls_choice_count(s_control_pick);++i)
                if(ui_rect_hit(controls_choice_rect(i),ev->x0,y)){
                    if(controls_choice_save(s_control_pick,i)){s_control_pick=-1;s_notice[0]=0;}
                    else snprintf(s_notice,sizeof(s_notice),"Không thể lưu, hãy thử lại");
                    return APP_REDRAW_PAGE;
                }
            return APP_REDRAW_NONE;
        }
        if(y<190 && ev->x0<170) {
            if(s_control_key>=0)s_control_key=-1;else s_page=SETTINGS_MAIN;
            return APP_REDRAW_PAGE;
        }
        int tab=settings_nav_hit(ev->x0,y);
        if(tab>=0) { ui_nav_request(ctx,tab); return APP_REDRAW_NONE; }
        if(s_control_key>=0) {
            if(y>=250&&y<330)s_control_pick=s_control_key;
            else if(y>=390&&y<470)s_control_pick=9+s_control_key;
        } else {
            for(int i=0;i<7;++i)if(y>=218+i*70&&y<282+i*70){s_control_key=i;s_notice[0]=0;return APP_REDRAW_PAGE;}
            if(y>=770&&y<850)s_control_pick=7;
            else if(y>=856&&y<936)s_control_pick=8;
            else if(y>=942&&y<1022) {
                if(!app_settings_set_reader_edge_brightness(!app_settings_reader_edge_brightness()))snprintf(s_notice,sizeof(s_notice),"Lưu thất bại");
                return APP_REDRAW_PAGE;
            } else if(y>=1028&&y<1108) {
                if(!app_settings_set_reader_edge_warmth(!app_settings_reader_edge_warmth()))snprintf(s_notice,sizeof(s_notice),"Lưu thất bại");
                return APP_REDRAW_PAGE;
            }
        }
        if(s_control_pick>=0){s_notice[0]=0;return APP_REDRAW_PAGE;}
        return APP_REDRAW_NONE;
    }

#else
#endif
    if (s_page == SETTINGS_PROFILE) {
        if (y < 190) { s_page = SETTINGS_MAIN; (void)read_pico_transfer_get_saved_wifi(s_main_wifi,&s_main_wifi_saved); settings_prepare_profile(); return APP_REDRAW_PAGE; }
        if (y >= 266 && y < 350) { profile_editor_open(false); return APP_REDRAW_PAGE; }
        if (y >= 366 && y < 450) { s_bookmark_picker=false;wallpaper_scan(); s_page = SETTINGS_AVATAR; return APP_REDRAW_PAGE; }
        return APP_REDRAW_NONE;
    }
    if (s_page == SETTINGS_AVATAR) {
        if (y < 190) { s_page = SETTINGS_PROFILE; return APP_REDRAW_PAGE; }
        if (y >= 270 && y < 354) { app_settings_set_avatar_path(""); s_page = SETTINGS_PROFILE; return APP_REDRAW_PAGE; }
        if (y >= 374 && y < 1046) {
            int i = (y - 374) / 96, index = s_wallpaper_page * 7 + i;
            if (index >= 0 && index < s_wallpaper_count && (y - 374) % 96 < 80) {
                app_settings_set_avatar_path(s_wallpapers[index].path);
                s_page = SETTINGS_PROFILE;
                return APP_REDRAW_PAGE;
            }
        }
        return APP_REDRAW_NONE;
    }
    if (s_page == SETTINGS_WALLPAPER_PREVIEW) {
        if (y < 190) { bookmark_preview_release(); s_page = SETTINGS_WALLPAPER; return APP_REDRAW_PAGE; }
        if (s_wallpaper_preview_ok && y >= 194 && y < 1032) {
            s_wallpaper_confirm = true;
            return APP_REDRAW_PAGE;
        }
        return APP_REDRAW_NONE;
    }
    if (s_page == SETTINGS_WALLPAPER && y < 190) {
        s_page = SETTINGS_LOCK_STYLE;
        return APP_REDRAW_PAGE;
    }
    if (s_page == SETTINGS_SHELF_STYLE) {
        if (y < 190) { s_page = SETTINGS_MAIN; (void)read_pico_transfer_get_saved_wifi(s_main_wifi,&s_main_wifi_saved); settings_prepare_profile(); return APP_REDRAW_PAGE; }
        for (int i = 0; i < 3; ++i) {
            if (y >= 263 + i * 253 - s_style_scroll && y < 493 + i * 253 - s_style_scroll && y < settings_viewport_end()) {
                app_settings_set_shelf_style((uint8_t)(i == 2 ? 5 : i + 1));
                s_page = SETTINGS_MAIN;
                return APP_REDRAW_PAGE;
            }
        }
        return APP_REDRAW_NONE;
    }
    if (s_page == SETTINGS_BLE_SCAN && y < 190) {
        ble_pt_scan_stop(); s_page = SETTINGS_BLUETOOTH; s_ble_scroll = 0; return APP_REDRAW_PAGE;
    }
    if(s_page==SETTINGS_MAIN && s_settings_group>=0 && y<190 && ev->x0<170) {
        (void)settings_parent_back();return APP_REDRAW_PAGE;
    }
    if (s_page != SETTINGS_MAIN && y < 190) { s_page = SETTINGS_MAIN; (void)read_pico_transfer_get_saved_wifi(s_main_wifi,&s_main_wifi_saved); settings_prepare_profile(); return APP_REDRAW_PAGE; }
    if (s_page == SETTINGS_LOCK_PIN) return APP_REDRAW_NONE;
    if (s_page == SETTINGS_AUTO_LOCK) {
        const uint8_t values[] = {1, 5, 10, 0};
        for (int i = 0; i < 4; ++i) if (ui_rect_hit((EpdRect){36, 272 + i * 112, 612, 92}, ev->x0, y)) {
            app_settings_set_auto_lock_minutes(values[i]);
            return APP_REDRAW_PAGE;
        }
        return APP_REDRAW_NONE;
    }
    if (s_page == SETTINGS_WIFI_TIMEOUT) {
        const uint8_t values[] = {1, 3, 5, 10, 0};
        for (int i = 0; i < 5; ++i) if (ui_rect_hit((EpdRect){36, 272 + i * 94, 612, 78}, ev->x0, y)) {
            app_settings_set_wifi_timeout_minutes(values[i]);
            return APP_REDRAW_PAGE;
        }
        return APP_REDRAW_NONE;
    }
    if (s_page == SETTINGS_BLE_TIMEOUT) {
        const uint8_t values[] = {5, 10, 15, 30, 0};
        for (int i = 0; i < 5; ++i) if (ui_rect_hit((EpdRect){36, 272 + i * 94, 612, 78}, ev->x0, y)) {
            app_settings_set_ble_timeout_minutes(values[i]);
            return APP_REDRAW_PAGE;
        }
        return APP_REDRAW_NONE;
    }

    if (s_page == SETTINGS_WIFI) {
        if (y >= 408 && y < 408+settings_detail_row_height()) {
            extern const app_desc_t app_transfer;
            app_transfer_request_wifi_setup();
            ctx->request_app = &app_transfer;
        }
        return APP_REDRAW_NONE;
    }
    if (s_page == SETTINGS_SYSTEM_FONT) {
        system_font_layout_t l=system_font_layout();
        if (ui_rect_hit(l.size,ev->x0,y)) {
            unsigned prior=app_settings_system_font_size();
            unsigned next=prior>=200?100:prior+10;
            app_settings_set_system_font_size(next);
            if (app_settings_system_font_size()!=next) {
                snprintf(s_notice,sizeof(s_notice),"Lưu thất bại, vui lòng thử lại");return APP_REDRAW_PAGE;
            }
            s_font_page=0;
            if (s_main_scroll>SETTINGS_SCROLL_MAX) s_main_scroll=SETTINGS_SCROLL_MAX;
            ui_text_set_system_scale(true);s_notice[0]=0;return APP_REDRAW_PAGE;
        }
#if KIIKO_BOARD_METALIO47
        for(int next=0;next<2;++next)if(ui_rect_hit(system_font_page_button(next),ev->x0,y)) {
            int pages=(ttf_font_count()+l.slots)/l.slots;
            int page=s_font_page+(next?1:-1);
            if(page<0||page>=pages)return APP_REDRAW_NONE;
            s_font_page=page;s_scroll_present_pending=true;return APP_REDRAW_AREA;
        }
#endif
        for (int slot=0;slot<l.slots;++slot) {
            if (!ui_rect_hit(system_font_item_rect(slot),ev->x0,y)) continue;
            int index=s_font_page*l.slots+slot;
            if (index>ttf_font_count()) return APP_REDRAW_NONE;
            const ttf_font_item_t *item=index?ttf_font_item(index-1):NULL;
            if (index && !item) return APP_REDRAW_NONE;
            snprintf(s_font_sync_path,sizeof(s_font_sync_path),"%s",item?item->path:"");
#if KIIKO_BOARD_METALIO47
            // 先确认手动选择能实际装载，成功后才保存并放宽恢复门禁。
            // Load an explicit choice successfully before saving it and relaxing recovery protection.
            if(s_font_sync_path[0]&&(ttf_font_open(s_font_sync_path)!=ESP_OK||strcmp(ttf_font_path(),s_font_sync_path))){
                app_font_activate_system();
                snprintf(s_notice,sizeof(s_notice),"Tải phông chữ thất bại, hãy kiểm tra tệp");return APP_REDRAW_PAGE;
            }
#endif
            app_settings_set_system_font_path(s_font_sync_path);
            if (strcmp(app_settings_system_font_path(),s_font_sync_path)) {
                app_font_activate_system();
                snprintf(s_notice,sizeof(s_notice),"Lưu thất bại, vui lòng thử lại");return APP_REDRAW_PAGE;
            }
            // 用户明确选择即解除本次启动的字体限制；空选择仍回到内建。
            // An explicit choice lifts this boot's font gate; clearing the choice still returns to built-in.
            if (s_font_sync_path[0]) pico_boot_allow_font();
            app_font_activate_system();s_notice[0]=0;
            s_font_sync_confirm=strcmp(app_settings_font_path(),s_font_sync_path)!=0;
            return APP_REDRAW_PAGE;
        }
        return APP_REDRAW_NONE;
    }
    if (s_page == SETTINGS_MAIN_REFRESH) {
        for (int i = 0; i < 3; ++i) {
            if (!ui_rect_hit((EpdRect){36, 298 + i * 158, 612, 132}, ev->x0, y)) continue;
            app_settings_set_main_refresh_mode((app_main_refresh_mode_t)i);
            return APP_REDRAW_PAGE;
        }
        return APP_REDRAW_NONE;
    }
    if (s_page == SETTINGS_SYSTEM_SIZE) {
        for (int index = 0; index < 11; ++index) {
            EpdRect box = {36 + (index % 2) * 316, 298 + (index / 2) * 122, 296, 102};
            if (!ui_rect_hit(box, ev->x0, y)) continue;
            app_settings_set_system_font_size((uint8_t)(100 + index * 10));
            ui_text_set_system_scale(true);
            return APP_REDRAW_PAGE;
        }
        return APP_REDRAW_NONE;
    }
    if (s_page == SETTINGS_SYSTEM_CONTRAST) {
        if (y < 298 || y >= 878) return APP_REDRAW_NONE;
        int index = (y - 298) / 116;
        app_settings_set_system_contrast((uint8_t)(100 + index * 10));
        return APP_REDRAW_PAGE;
    }
    if (s_page == SETTINGS_LOCK_STYLE) {
        if (y >= 300 && y < 432) {
#if KIIKO_BOARD_METALIO47
            return settings_open_lock_images(ctx,NULL,false);
#else
            s_bookmark_picker=false;wallpaper_scan();
            s_page = SETTINGS_WALLPAPER;
            return APP_REDRAW_PAGE;
#endif
        }
        if (y >= 454 && y < 586) {
            app_settings_set_lock_style(0);
            return APP_REDRAW_PAGE;
        }
        if (y >= 608 && y < 740) {
            app_settings_set_lock_style(2);
#if KIIKO_BOARD_METALIO47
#else
            s_collage_prepare_pending = true;
#endif
            return APP_REDRAW_PAGE;
        }
        if(y>=762 && y<894){
#if KIIKO_BOARD_METALIO47
            return settings_open_lock_images(ctx,NULL,true);
#else
            s_bookmark_picker=true;wallpaper_scan();s_page=SETTINGS_WALLPAPER;
            return APP_REDRAW_PAGE;
#endif
        }
        if (y >= 1010 && app_settings_wallpaper_path()[0]) {
#if KIIKO_BOARD_METALIO47
            return settings_open_lock_images(ctx,NULL,false);
#else
            s_bookmark_picker=false;wallpaper_scan();
            s_page = SETTINGS_WALLPAPER;
            return APP_REDRAW_PAGE;
#endif
        }
        return APP_REDRAW_NONE;
    }
    if (s_page == SETTINGS_WALLPAPER) {
        if (y < 292 || y >= 1028) return APP_REDRAW_NONE;
        int index = s_wallpaper_page * 8 + (y - 292) / 92;
        if (index >= s_wallpaper_count) return APP_REDRAW_NONE;
#if KIIKO_BOARD_METALIO47
        return settings_open_lock_images(ctx,s_wallpapers[index].path,s_bookmark_picker);
#else
        s_wallpaper_selected = index;
        bookmark_preview_prepare(ctx->fb);
        s_wallpaper_confirm = false;
        s_page = SETTINGS_WALLPAPER_PREVIEW;
        return APP_REDRAW_PAGE;
#endif
    }
    if (s_page == SETTINGS_TIME) {
        if (y >= 407 && y < 407+settings_detail_row_height()) {
            s_notice[0] = 0; s_sync_pending = true;
            snprintf(s_notice, sizeof(s_notice), "Đang lấy giờ mạng (NTP GMT+7)…");
            return APP_REDRAW_PAGE;
        }
        if (y >= 518 && y < 518+settings_detail_row_height()) { time_open(); return APP_REDRAW_PAGE; }
        return APP_REDRAW_NONE;
    }
    if (s_page == SETTINGS_KOSYNC) {
        if (ev->x0 >= 36 && ev->x0 <= 648) {
            if (y >= 275 && y < 333) { settings_editor_open(EDITOR_KOSYNC_URL); return APP_REDRAW_PAGE; }
            if (y >= 333 && y < 391) { settings_editor_open(EDITOR_KOSYNC_USER); return APP_REDRAW_PAGE; }
            if (y >= 391 && y < 455) { settings_editor_open(EDITOR_KOSYNC_PASS); return APP_REDRAW_PAGE; }
        }
        if (y >= 465 && y < 557) {
            app_settings_set_kosync_auto(!app_settings_kosync_auto());
            snprintf(s_kosync_notice, sizeof(s_kosync_notice), "Đã %s tự động đồng bộ KOSync",
                     app_settings_kosync_auto() ? "bật" : "tắt");
            return APP_REDRAW_PAGE;
        }
        if (y >= 577 && y < 649) {
            if (!app_settings_kosync_user()[0]) {
                snprintf(s_kosync_notice, sizeof(s_kosync_notice), "Chưa có tài khoản (Chạm mục Tài khoản ở trên để nhập)");
            } else {
                char msg[80] = {0};
                esp_err_t err = kosync_verify_account(false, msg, sizeof(msg));
                snprintf(s_kosync_notice, sizeof(s_kosync_notice), "%s",
                         err == ESP_OK ? (msg[0] ? msg : "Tài khoản hợp lệ! Kết nối KOSync thành công.") :
                                         (msg[0] ? msg : "Kết nối KOSync thất bại, hãy kiểm tra mạng/tài khoản"));
            }
            return APP_REDRAW_PAGE;
        }
        if (y >= 665 && y < 737) {
            extern const app_desc_t app_transfer;
            app_transfer_request_wifi_setup();
            ctx->request_app = &app_transfer;
            return APP_REDRAW_NONE;
        }
        if (y >= 753 && y < 825) {
            app_settings_set_kosync_url(DPREADER_DEFAULT_KOSYNC_URL);
            snprintf(s_kosync_notice, sizeof(s_kosync_notice), "Đã đặt lại máy chủ: %s", DPREADER_DEFAULT_KOSYNC_URL);
            return APP_REDRAW_PAGE;
        }
        return APP_REDRAW_NONE;
    }
    if (s_page == SETTINGS_OPDS) {
        if (ev->x0 >= 36 && ev->x0 <= 648) {
            if (y >= 275 && y < 333) { settings_editor_open(EDITOR_OPDS_URL); return APP_REDRAW_PAGE; }
            if (y >= 333 && y < 391) { settings_editor_open(EDITOR_OPDS_USER); return APP_REDRAW_PAGE; }
            if (y >= 391 && y < 455) { settings_editor_open(EDITOR_OPDS_PASS); return APP_REDRAW_PAGE; }
        }
        if (y >= 465 && y < 537) {
            extern const app_desc_t app_book;
            app_book_request_opds();
            ctx->request_app = &app_book;
            return APP_REDRAW_NONE;
        }
        if (y >= 553 && y < 625) {
            extern const app_desc_t app_transfer;
            app_transfer_request_wifi_setup();
            ctx->request_app = &app_transfer;
            return APP_REDRAW_NONE;
        }
        if (y >= 641 && y < 713) {
            app_settings_set_opds_url(DPREADER_DEFAULT_OPDS_URL);
            snprintf(s_opds_notice, sizeof(s_opds_notice), "Đã đặt lại kho sách: %s", DPREADER_DEFAULT_OPDS_URL);
            return APP_REDRAW_PAGE;
        }
        return APP_REDRAW_NONE;
    }
#if KIIKO_BOARD_METALIO47
    if (s_page == SETTINGS_POWER_SLEEP) {
        for (int i = 0; i < 2; ++i) {
            if (ui_rect_hit((EpdRect){36, 300 + i * 154, 612, 132}, ev->x0, ev->y0)) {
                app_settings_set_staged_shutdown(i == 0);
                return APP_REDRAW_PAGE;
            }
        }
        int delta=ui_rect_hit((EpdRect){36,690,80,72},ev->x0,ev->y0)?-1:
                  ui_rect_hit((EpdRect){568,690,80,72},ev->x0,ev->y0)?1:0;
        if(delta){
            int hours=app_settings_sleep_off_hours()+delta;
            if(hours<APP_SLEEP_OFF_HOURS_MIN||hours>APP_SLEEP_OFF_HOURS_MAX)return APP_REDRAW_NONE;
            (void)app_settings_set_sleep_off_hours((uint8_t)hours);
            return APP_REDRAW_PAGE;
        }
        return APP_REDRAW_NONE;
    }
#else
    if (s_page == SETTINGS_POWER_SLEEP) {
        if (y >= 278 && y < 402) {
            app_settings_set_staged_shutdown(false);
            return APP_REDRAW_PAGE;
        }
        if (y >= 414 && y < 538) {
            app_settings_set_staged_shutdown(true);
            return APP_REDRAW_PAGE;
        }
        return APP_REDRAW_NONE;
    }
#endif
    if (s_page == SETTINGS_CONFIG) {
        if (y >= 561 && y < 644) {
            s_config_confirm = false;
            esp_err_t err = app_settings_backup_save();
            snprintf(s_notice, sizeof(s_notice), "%s",
                     err == ESP_OK ? "Đã sao lưu vào thư mục gốc thẻ nhớ" :
                     err == ESP_ERR_INVALID_STATE ? "Không nhận diện được thẻ nhớ, hãy kiểm tra thẻ rồi thử lại" :
                     "Lưu thất bại, hãy kiểm tra dung lượng trống trên thẻ nhớ");
            return APP_REDRAW_PAGE;
        }
        if (y >= 681 && y < 764) {
            if (!s_config_confirm) {
                s_config_confirm = true;
                snprintf(s_notice, sizeof(s_notice), "Sẽ ghi đè cài đặt hiện tại, chạm lần nữa để xác nhận");
                return APP_REDRAW_PAGE;
            }
            s_config_confirm = false;
            esp_err_t err = app_settings_backup_restore();
            if (err == ESP_OK) {
                ttf_font_scan();
                app_font_activate_system();
                ui_text_set_system_scale(true);
                // 阅读记录恢复后令首页和书架重新取进度、时长与排序。
                // Rebuild home and shelf caches after restoring reading records.
                book_store_notify_changed();
            }
            char saved_ssid[33] = {0};
            bool wifi_saved = false;
            if (err == ESP_OK)
                (void)read_pico_transfer_get_saved_wifi(saved_ssid, &wifi_saved);
            snprintf(s_notice, sizeof(s_notice), "%s",
                     err == ESP_OK ? (wifi_saved ? "Đã khôi phục cấu hình và lịch sử đọc; hãy kết nối lại WiFi" :
                                                   "Đã khôi phục cấu hình và lịch sử đọc") :
                     err == ESP_ERR_INVALID_STATE ? "Không nhận diện được thẻ nhớ, hãy kiểm tra thẻ rồi thử lại" :
                     err == ESP_ERR_NOT_FOUND ? "Không tìm thấy tệp sao lưu cấu hình trên thẻ nhớ" :
                     err == ESP_ERR_INVALID_RESPONSE ? "Tệp cấu hình bị hỏng hoặc không tương thích phiên bản" :
                     "Khôi phục chưa hoàn tất, vui lòng thử lại");
            return APP_REDRAW_PAGE;
        }
        return APP_REDRAW_NONE;
    }
    if (s_page == SETTINGS_LOG_EXPORT) {
        if (ev->type != UI_GESTURE_TAP) return APP_REDRAW_NONE;
        if (y >= 804 && y < 884) {
            char err[96] = {0};
            char path[64] = {0};
            esp_err_t err_code = log_export_to_sd(path, sizeof(path), err, sizeof(err));
            if (err_code == ESP_OK) {
                snprintf(s_notice, sizeof(s_notice), "Đã xuất log thành công ra %s", path[0] ? path : "/sdcard/dpreader_log.txt");
            } else {
                snprintf(s_notice, sizeof(s_notice), "Lỗi: %s", err[0] ? err : "Thất bại");
            }
            return APP_REDRAW_PAGE;
        }
        if (y >= 902 && y < 982) {
            log_export_clear();
            rp_crash_log_clear();
            snprintf(s_notice, sizeof(s_notice), "Đã xóa sạch nhật ký tạm thời");
            return APP_REDRAW_PAGE;
        }
        return APP_REDRAW_NONE;
    }
    if (s_page == SETTINGS_BLUETOOTH) {
        const ble_layout_t l = ble_layout();
        if (ev->type != UI_GESTURE_TAP) return APP_REDRAW_NONE;
        // 版式给的是屏幕坐标（已减去滚动量），所以这里直接用 y 比，不再加 scroll。
        // The layout yields screen coordinates (scroll already subtracted), so compare y directly.
        const int ty = y;

        // 学习时轻点取消；识别结果轻点后返回开关。/ Tap to cancel learning or dismiss the received-key result.
        if (s_ble_learning) {
            s_ble_learning = 0;
            snprintf(s_ble_notice, sizeof(s_ble_notice), "Đã hủy học phím");
            return APP_REDRAW_PAGE;
        }
        if (s_ble_feedback[0] && ty >= 202 - s_ble_scroll && ty < 328 - s_ble_scroll) {
            s_ble_feedback[0] = 0;
            return APP_REDRAW_PAGE;
        }
        if (ty < 160 || ty >= settings_viewport_end()) return APP_REDRAW_NONE;

        if (l.toggle_top >= 0 && ty >= l.toggle_top && ty < l.toggle_top + 126) {
            app_settings_set_ble_turner(!app_settings_ble_turner());
            // 明确关掉时把失败计数和原因一起清掉：否则自锁之后用户没有重试的途径。
            // Switching it off explicitly clears the failure count and reason; without this a
            // self-locked stack leaves the user no way to try again.
            if (!app_settings_ble_turner()) ble_pt_reset_failure();
            snprintf(s_ble_notice, sizeof(s_ble_notice), "%s",
                     app_settings_ble_turner() ? "Đã bật, đang chờ thiết bị kết nối" : "Đã tắt và giải phóng bộ nhớ Bluetooth");
            return APP_REDRAW_PAGE;
        }
        if (ty >= l.scan_entry_top && ty < l.scan_entry_top + BLE_ROW_H) {
            if (!app_settings_ble_turner() || !ble_pt_running()) {
                snprintf(s_ble_notice, sizeof(s_ble_notice), "Vui lòng bật Bluetooth trước"); return APP_REDRAW_PAGE;
            }
            // 进扫描页就开持续扫描，离页时停掉。/ Scan continuously while the page is open.
            ble_pt_scan_start(BLE_PT_SCAN_FOREVER);
            s_ble_scroll = 0;
            s_ble_notice[0] = 0;
            s_page = SETTINGS_BLE_SCAN;
            return APP_REDRAW_PAGE;
        }
        for (int i = 0; i < 2; ++i) {
            const EpdRect btn = ble_button_rect(l.learn_top[i]);
            // 命中判定必须同时比 x：按钮的纵向范围几乎覆盖整行，只比 y 会让整行都算按钮。
            // The hit test needs the horizontal test too: the button's vertical span covers almost
            // the whole row, so comparing y alone makes the entire row count as the button.
            if (ev->x0 >= btn.x && ev->x0 < btn.x + btn.width && ty >= btn.y && ty < btn.y + btn.height) {
                if (!ble_pt_connected()) {
                    snprintf(s_ble_notice, sizeof(s_ble_notice), "Vui lòng kết nối thiết bị lật trang trước khi học phím");
                    return APP_REDRAW_PAGE;
                }
                ble_pt_raw_t stale;
                while (ble_pt_pop_raw(&stale)) {}
                // 把此刻那一帧当基准：点「学习」时用户还没按键，外设发的就是静止帧。有些遥控器
                // 的静止帧带着非零常量，不这样标一下，那串常量会被当成一次按下学进去。
                // Pin the frame present now as the baseline: the user has not pressed yet, so what the
                // remote sends is its resting frame. Some remotes keep non-zero constants there, and
                // without this the constants get learned as if they were a press.
                ble_pt_learn_begin();
                s_ble_feedback[0] = s_ble_notice[0] = 0;
                s_ble_learning = i + 1;
                s_ble_scroll = 0;
                return APP_REDRAW_PAGE;
            }
        }
        // 已配对行：右侧按钮忘记，行本身连接。/ Bonded rows: the button forgets, the row connects.
        for (int i = 0; i < l.bond_rows && i < BLE_PT_MAX_BONDS; ++i) {
            const ble_pt_bond_t *bond = ble_pt_bond((uint8_t)i);
            if (!bond) continue;
            const EpdRect btn = ble_button_rect(l.bond_top[i]);
            if (ev->x0 >= btn.x && ev->x0 < btn.x + btn.width && ty >= btn.y && ty < btn.y + btn.height) {
#if KIIKO_BOARD_METALIO47
                const esp_err_t forgotten=ble_pt_forget(bond->addr);
                snprintf(s_ble_notice,sizeof(s_ble_notice),"%s",forgotten==ESP_OK?"Đã xóa thiết bị ghép đôi":
                    forgotten==ESP_ERR_INVALID_STATE?"Vui lòng bật Bluetooth trước khi xóa ghép đôi":"Xóa thất bại, vui lòng thử lại");
#else
                if (ble_pt_connected()) ble_pt_disconnect();
                ble_pt_forget(bond->addr);
                snprintf(s_ble_notice, sizeof(s_ble_notice), "Đã xóa thiết bị ghép đôi");
#endif
                return APP_REDRAW_PAGE;
            }
            if (ty >= l.bond_top[i] && ty < l.bond_top[i] + BLE_ROW_H) {
                if (ble_pt_connected() || ble_pt_connecting()) ble_pt_disconnect();
                snprintf(s_ble_notice, sizeof(s_ble_notice),
                         ble_pt_connect(bond->addr) == ESP_OK ? "Đang kết nối…" : "Kết nối thất bại, vui lòng thử lại");
                return APP_REDRAW_PAGE;
            }
        }
        return APP_REDRAW_NONE;
    }
    if (s_page == SETTINGS_BLE_SCAN) {
        const ble_scan_layout_t l = ble_scan_layout();
        if (ev->type != UI_GESTURE_TAP) return APP_REDRAW_NONE;
        if (y < 160 || y >= settings_viewport_end()) return APP_REDRAW_NONE;
        const int ty = y + s_ble_scroll;
        // 选一台就连，连上后退回二级页。扫描在离页时停。
        // Picking one connects, then returns to level two; scanning stops on the way out.
        for (int i = 0; i < l.device_rows; ++i) {
            const ble_pt_device_t *dev = ble_scan_visible_at(i);
            if (!dev) continue;
            const int top = l.devices_top + i * BLE_ROW_H;
            if (ty < top || ty >= top + BLE_ROW_H) continue;
            ble_pt_scan_stop();
            if (ble_pt_connected() || ble_pt_connecting()) ble_pt_disconnect();
            snprintf(s_ble_notice, sizeof(s_ble_notice),
                     ble_pt_connect(dev->addr) == ESP_OK ? "Đang kết nối…" : "Kết nối thất bại, vui lòng thử lại");
            s_page = SETTINGS_BLUETOOTH;
            s_ble_scroll = 0;
            return APP_REDRAW_PAGE;
        }
        return APP_REDRAW_NONE;
    }
    if (y<160 || y>=UI_NAV_TOP || ev->x0<36 || ev->x0>=648) return APP_REDRAW_NONE;
    y+=s_main_scroll;
    if (s_settings_group < 0) {
        if (y>=180 && y<180+settings_profile_height()) { s_page=SETTINGS_PROFILE;return APP_REDRAW_PAGE; }
        if(settings_parent_open(y))return APP_REDRAW_PAGE;
        return APP_REDRAW_NONE;
    }
    int row;
    if (s_settings_group==0 && y>=SETTINGS_WIRELESS_Y && y<SETTINGS_WIRELESS_Y+SETTINGS_WIRELESS_ROWS*SETTINGS_ROW_H) {
        row=(y-SETTINGS_WIRELESS_Y)/SETTINGS_ROW_H;
        if(row==0) {
            extern const app_desc_t app_transfer;app_transfer_request_wifi_setup();ctx->request_app=&app_transfer;return APP_REDRAW_NONE;
        }
        if(row==1) {
            s_page=SETTINGS_WIFI_TIMEOUT;return APP_REDRAW_PAGE;
        }
        if(row==2) {
            s_page=SETTINGS_BLUETOOTH;s_ble_scroll=s_ble_learning=0;s_ble_notice[0]=s_ble_feedback[0]=0;
            ble_pt_raw_t unused;while(ble_pt_pop_raw(&unused)){}return APP_REDRAW_PAGE;
        }
        if(row==3) {
            s_page=SETTINGS_BLE_TIMEOUT;return APP_REDRAW_PAGE;
        }
        if(row==4) {
            s_page=SETTINGS_KOSYNC;s_kosync_notice[0]=0;return APP_REDRAW_PAGE;
        }
        if(row==5) {
            s_page=SETTINGS_OPDS;s_opds_notice[0]=0;return APP_REDRAW_PAGE;
        }
        s_page=SETTINGS_TIME;return APP_REDRAW_PAGE;
    }
    if(s_settings_group==1 && y>=SETTINGS_DISPLAY_Y && y<SETTINGS_DISPLAY_Y+SETTINGS_DISPLAY_ROWS*SETTINGS_ROW_H) {
        row=(y-SETTINGS_DISPLAY_Y)/SETTINGS_ROW_H;
        if(row==0) { ttf_font_scan();s_font_page=0;s_notice[0]=0;s_page=SETTINGS_SYSTEM_FONT; }
        else if(row==1)s_page=SETTINGS_SYSTEM_CONTRAST;
        else if(row==2)s_page=SETTINGS_MAIN_REFRESH;
        else if(row==3)app_settings_set_home_full_refresh(!app_settings_home_full_refresh());
        else if(row==4)s_page=SETTINGS_AUTO_LOCK;
        else if(row==5)app_settings_set_wake_presses(app_settings_wake_presses()==2?1:2);
#if KIIKO_BOARD_METALIO47
        else if(row==6)s_page=SETTINGS_POWER_SLEEP;
        else s_page=SETTINGS_FRONTLIGHT;
#else
        else s_page=SETTINGS_POWER_SLEEP;
#endif
        return APP_REDRAW_PAGE;
    }
    if(s_settings_group==2 && y>=SETTINGS_PERSONAL_Y && y<SETTINGS_PERSONAL_Y+SETTINGS_PERSONAL_ROWS*SETTINGS_ROW_H) {
        row=(y-SETTINGS_PERSONAL_Y)/SETTINGS_ROW_H;
        if(row==0) {s_page=SETTINGS_SHELF_STYLE;s_style_scroll=0;}
        else if(row==1)s_page=SETTINGS_LOCK_STYLE;
        else if(row==2){s_page=SETTINGS_LOCK_PIN;s_notice[0]=0;}
        else profile_editor_open(true);
        return APP_REDRAW_PAGE;
    }
    if(s_settings_group==3 && y>=SETTINGS_CONTROLS_Y && y<SETTINGS_CONTROLS_Y+SETTINGS_CONTROLS_ROWS*SETTINGS_ROW_H) {
        s_page=SETTINGS_CONTROLS;s_control_key=s_control_pick=-1;
        return APP_REDRAW_PAGE;
    }
    if(s_settings_group==4 && y>=SETTINGS_MAINTENANCE_Y && y<SETTINGS_MAINTENANCE_Y+SETTINGS_MAINTENANCE_ROWS*SETTINGS_ROW_H) {
        row=(y-SETTINGS_MAINTENANCE_Y)/SETTINGS_ROW_H;
#if KIIKO_BOARD_METALIO47
        if(row==1){s_page=SETTINGS_SD_RECOVERY;s_sd_notice[0]=0;return APP_REDRAW_PAGE;}
        if(row>1)--row;
#endif
        if(row==5) {s_page=SETTINGS_FACTORY_RESET;s_reset_confirm=s_reset_pending=false;s_reset_notice[0]=0;}
        else if(row==0)upgrade_open();
        else if(row==1){s_page=SETTINGS_CONFIG;s_config_confirm=false;s_notice[0]=0;}
        else if(row==2){s_page=SETTINGS_LOG_EXPORT;s_notice[0]=0;}
        else if(row==3){
            // 屏幕在刷新中途断电会停在半帧，常规差分刷新永远盖不掉那道条纹；点一下就整屏盲刷
            // 一遍，然后自己把当前页面（含底栏）推回面板。
            // A power cut mid-refresh leaves the panel half-driven, and an ordinary differential
            // refresh can never cover that stripe. One tap runs the blind full-screen pass, which
            // then pushes the current page -- navigation bar included -- back onto the panel.
            s_panel_repair_pending=true;
            snprintf(s_notice,sizeof(s_notice),"Đang làm sạch màn hình…");
            return APP_REDRAW_PAGE;
        }
#if KIIKO_BOARD_METALIO47
        else {s_boot_pending=false;s_boot_notice[0]=0;s_page=SETTINGS_BOOT;}
#else
        else {s_boot_pending=false;s_page=SETTINGS_BOOT;}
#endif
        return APP_REDRAW_PAGE;
    }
    return APP_REDRAW_NONE;
}

static app_redraw_t on_key(app_ctx_t *ctx, int key) {
#if KIIKO_BOARD_METALIO47
    if(app_sd_recovery_busy())return APP_REDRAW_NONE;
    if(s_page==SETTINGS_CONTROLS&&s_control_pick>=0){s_control_pick=-1;s_notice[0]=0;return APP_REDRAW_PAGE;}
    if(s_page==SETTINGS_CONTROLS&&s_control_key>=0){s_control_key=-1;return APP_REDRAW_PAGE;}
    if(s_page==SETTINGS_ONLINE_FONTS){
        ui_online_fonts_close(&s_online_fonts);ttf_font_scan();s_font_page=0;s_page=SETTINGS_SYSTEM_FONT;
        return APP_REDRAW_PAGE;
    }
#endif
    s_reset_confirm=s_reset_pending=false;
#if KIIKO_BOARD_METALIO47
    if (s_page==SETTINGS_BOOT) { s_boot_pending=false; app_cancel_download(); }
#else
#endif
    if(s_font_sync_confirm) {
        s_font_sync_confirm=false;s_notice[0]=0;
        if(key!=1)return APP_REDRAW_PAGE;
    }
    if (s_page == SETTINGS_PIN_ENTRY) { pin_setup_close(); s_page = SETTINGS_LOCK_PIN; return APP_REDRAW_PAGE; }
    if (s_page == SETTINGS_UPGRADE) {
        s_upgrade_job = UPGRADE_JOB_NONE;
        if (s_local_pending || s_upgrade_restart) return APP_REDRAW_NONE;
        pico_online_cancel_join(); s_upgrade_confirm = false;
    }
    if (s_page == SETTINGS_BLE_SCAN) {
        ble_pt_scan_stop();
        if (key != 1) { s_page = SETTINGS_BLUETOOTH; s_ble_scroll = 0; return APP_REDRAW_PAGE; }
    }
    if (key == 1) { ui_nav_request(ctx, 0); return APP_REDRAW_NONE; }
    if (s_page == SETTINGS_WALLPAPER_PREVIEW) {
        if (s_wallpaper_confirm) s_wallpaper_confirm = false;
        else { bookmark_preview_release(); s_page = SETTINGS_WALLPAPER; }
        return APP_REDRAW_PAGE;
    }
    if (s_page == SETTINGS_WALLPAPER) { s_page = SETTINGS_LOCK_STYLE; return APP_REDRAW_PAGE; }
    if (s_page == SETTINGS_AVATAR) { s_page = SETTINGS_PROFILE; return APP_REDRAW_PAGE; }
    if (s_page == SETTINGS_TEXT_EDIT) {
        ui_keyboard_end();
        if (s_editor_mode == EDITOR_SIGNATURE) s_page = SETTINGS_MAIN;
        else if (s_editor_mode == EDITOR_DEVICE_NAME) s_page = SETTINGS_PROFILE;
        else if (s_editor_mode == EDITOR_KOSYNC_URL || s_editor_mode == EDITOR_KOSYNC_USER || s_editor_mode == EDITOR_KOSYNC_PASS)
            s_page = SETTINGS_KOSYNC;
        else
            s_page = SETTINGS_OPDS;
        return APP_REDRAW_PAGE;
    }
    if (s_page == SETTINGS_PROFILE) { s_page = SETTINGS_MAIN; (void)read_pico_transfer_get_saved_wifi(s_main_wifi,&s_main_wifi_saved); settings_prepare_profile(); return APP_REDRAW_PAGE; }
    if (s_page == SETTINGS_TIME_EDIT) { s_page = SETTINGS_TIME; return APP_REDRAW_PAGE; }
    if (s_page != SETTINGS_MAIN) { s_page = SETTINGS_MAIN; (void)read_pico_transfer_get_saved_wifi(s_main_wifi,&s_main_wifi_saved); settings_prepare_profile(); return APP_REDRAW_PAGE; }
    if(s_settings_group>=0){(void)settings_parent_back();return APP_REDRAW_PAGE;}
    ui_nav_request(ctx, 0);
    return APP_REDRAW_NONE;
}
static void settings_exit(app_ctx_t *ctx) {
#if KIIKO_BOARD_METALIO47
    if(s_page==SETTINGS_ONLINE_FONTS)ui_online_fonts_close(&s_online_fonts);
#endif
    s_reset_confirm=s_reset_pending=false;
#if KIIKO_BOARD_METALIO47
    s_boot_pending=false; app_cancel_download();
#else
#endif
    bookmark_preview_release();s_bookmark_picker=false;
    s_font_sync_confirm=false;s_font_sync_path[0]=0;
    pin_setup_close();
    s_collage_prepare_pending = false;
    s_upgrade_job = UPGRADE_JOB_NONE;
    (void)ctx; ui_keyboard_end(); pico_online_cancel_join(); ble_pt_scan_stop(); s_ble_learning = 0;
}
static void on_before_lock(app_ctx_t *ctx) {
#if KIIKO_BOARD_METALIO47
    if(s_page==SETTINGS_ONLINE_FONTS){ui_online_fonts_close(&s_online_fonts);ttf_font_scan();s_font_page=0;s_page=SETTINGS_SYSTEM_FONT;}
#endif
    s_reset_confirm=s_reset_pending=false;
#if KIIKO_BOARD_METALIO47
    s_boot_pending=false; app_cancel_download();
#else
#endif
    s_font_sync_confirm=false;s_font_sync_path[0]=0;
    if (s_page == SETTINGS_PIN_ENTRY) { pin_setup_close(); s_page = SETTINGS_LOCK_PIN; }
    s_upgrade_job = UPGRADE_JOB_NONE;
    (void)ctx; pico_online_cancel_join(); ble_pt_scan_stop(); s_ble_learning = 0;
    (void)ble_pt_stop(2000);
}
static bool no_menu_handle(app_ctx_t *ctx) { (void)ctx; return false; }
static void settings_media_lost(app_ctx_t *ctx) {
#if KIIKO_BOARD_METALIO47
    if(s_page==SETTINGS_ONLINE_FONTS){ui_online_fonts_close(&s_online_fonts);s_page=SETTINGS_SYSTEM_FONT;s_font_page=0;}
#endif
    bookmark_preview_release();
    (void)ctx; s_avatar_ready=s_avatar_pending=false; s_avatar_path[0]=0;
}
static void settings_media_ready(app_ctx_t *ctx) { (void)ctx; settings_prepare_profile(); }
static bool settings_edge_back(app_ctx_t *ctx) {
    (void)ctx;
#if KIIKO_BOARD_METALIO47
    if(app_sd_recovery_busy())return false;
#endif
    return (s_page != SETTINGS_MAIN || s_settings_group>=0) && !s_reset_pending && !s_local_pending &&
        !s_upgrade_restart && !s_pin_pending && !s_font_sync_confirm;
}
static bool settings_back_swipe_area(app_ctx_t *ctx,const ui_gesture_event_t *event) {
    (void)ctx;
    return s_page!=SETTINGS_TEXT_EDIT || event->y0<UI_SYSTEM_KEYBOARD_TOP;
}
static bool main_page_visible(app_ctx_t *ctx) { (void)ctx; return s_page == SETTINGS_MAIN && s_settings_group<0; }
static bool settings_navigation_enabled(app_ctx_t *ctx) {
    (void)ctx;
#if KIIKO_BOARD_METALIO47
    if(app_sd_recovery_busy())return false;
    if(s_page==SETTINGS_ONLINE_FONTS)return false;
#endif
    return !s_reset_pending && !s_local_pending && !s_upgrade_restart && !s_pin_pending && !s_boot_pending;
}
static bool settings_present(app_ctx_t *ctx, app_redraw_t redraw) {
#if KIIKO_BOARD_METALIO47
    if(s_page==SETTINGS_ONLINE_FONTS)return ui_online_fonts_present(&s_online_fonts,ctx,redraw);
    if(settings_gray_preview()&&(redraw==APP_REDRAW_PAGE||redraw==APP_REDRAW_FULL||redraw==APP_REDRAW_AREA)){
        if(ctx->request_app)return true;
        // 壁纸/透明书封及书架样式预览不继承系统二值化；同轮输出完整灰阶，退出后恢复原策略。
        // Wallpaper/transparent-cover/shelf previews bypass system thresholding; present full grays once and restore policy afterward.
        const bool binary=display_system_binary_set(false);
        display_main_transition_cancel();s_scroll_present_pending=false;
        render(ctx,ctx->fb);
        guard_draw_result(ctx->hl,redraw==APP_REDRAW_FULL?update_display_full(ctx->hl):
            update_display_area_full_with(ctx->hl,&E0470_FULL_WAVEFORM,MODE_GL16,
                (EpdRect){0,0,UI_LOCK_WIDTH,UI_LOCK_HEIGHT}));
        display_system_binary_set(binary);
        return true;
    }
#endif
    if (s_page == SETTINGS_PIN_ENTRY && (redraw == APP_REDRAW_PAGE || redraw == APP_REDRAW_FULL)) {
        display_main_transition_cancel(); render(ctx, ctx->fb);
        guard_draw_result(ctx->hl, update_display_area_diff_with(ctx->hl, &E0470_WAVEFORM, MODE_GL16, (EpdRect){0,0,UI_LOCK_WIDTH,UI_NAV_REFRESH_END}));
        return true;
    }
    if (redraw == APP_REDRAW_AREA && s_page == SETTINGS_TEXT_EDIT) {
        if (s_input_settle) {
            guard_draw_result(ctx->hl, update_display_area_full_with(ctx->hl, &E0470_WAVEFORM, MODE_GL16, s_input_area));
            s_input_settle = false;
            return true;
        }
        guard_draw_result(ctx->hl, update_display_area_diff_with(ctx->hl,
            s_input_layout ? &E0470_WAVEFORM : &E0470_FOLLOW_WAVEFORM,
            s_input_layout ? MODE_GL16 : MODE_DU, s_input_area));
        return true;
    }
    if (redraw == APP_REDRAW_AREA && s_scroll_present_pending &&
        (s_page == SETTINGS_MAIN || s_page == SETTINGS_BLUETOOTH || s_page == SETTINGS_BLE_SCAN ||
         s_page == SETTINGS_SHELF_STYLE || s_page == SETTINGS_SYSTEM_FONT ||
         s_page == SETTINGS_WALLPAPER || s_page == SETTINGS_AVATAR)) {
        s_scroll_present_pending = false;
        render(ctx, ctx->fb);
        int top = s_page == SETTINGS_MAIN ? 160 : s_page == SETTINGS_SHELF_STYLE ? 242 : 190;
        EpdRect area = {0, top, UI_LOCK_WIDTH, settings_content_end() - top};
        // 只驱动变化像素，滚动次数不触发黑白清屏；题头与底栏保持稳定。
        // Drive changed pixels only; scroll counts never trigger a wipe, and header/nav stay stable.
#if KIIKO_BOARD_METALIO47
        guard_draw_result(ctx->hl,update_display_area_diff_with(ctx->hl,display_main_bw_waveform(),MODE_DU,area));
#else
        // 列表仅一次短灰阶差分；保留抗锯齿目标，取消主页替换、清白预刷与松手补刷。
        // Present the list once with short gray differences; retain AA targets without main substitution, white prepasses or release settling.
        display_main_transition_cancel();
        const EpdWaveform *wave=display_settings_scroll_waveform();
        guard_draw_result(ctx->hl, update_display_area_diff_with(ctx->hl,wave,MODE_DU,area));
#endif
        return true;
    }
    if (s_page != SETTINGS_UPGRADE) return false;
    if (redraw == APP_REDRAW_PAGE) {
        display_main_transition_cancel();
        render(ctx, ctx->fb);
        guard_draw_result(ctx->hl, update_display_area_diff_with(ctx->hl, &E0470_WAVEFORM,
                          MODE_GL16, (EpdRect){0, 0, UI_LOCK_WIDTH, settings_content_end()}));
        return true;
    }
    if (redraw == APP_REDRAW_AREA) {
        guard_draw_result(ctx->hl, update_display_area_diff_with(ctx->hl, &E0470_FOLLOW_WAVEFORM,
                          MODE_DU, upgrade_progress_area()));
        return true;
    }
    return false;
}

const app_desc_t app_device_settings = {
#if KIIKO_BOARD_METALIO47
    .binary_ui = true,
#else
    .binary_ui = false,
#endif
    .title = "Cài đặt Settings", .detail = "Hiển thị, kết nối & thiết bị", .enter_full = false,
    .owns_keys = true, .menu_handle_enabled = no_menu_handle,
    .main_page_visible = main_page_visible, .edge_back_enabled = settings_edge_back,
    .back_swipe_area=settings_back_swipe_area,
    .persistent_navigation=true,.navigation_enabled=settings_navigation_enabled,
    .on_enter = on_enter, .on_exit = settings_exit, .on_before_lock = on_before_lock, .render = render, .on_tick = on_tick, .on_gesture = on_gesture, .on_key = on_key,
    .present = settings_present, .on_media_lost=settings_media_lost, .on_media_ready=settings_media_ready,
};
