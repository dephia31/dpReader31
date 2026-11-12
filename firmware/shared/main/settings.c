/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * NVS 读写。打开失败就用深睡默认值，不擦除整个分区。
 *
 * 用户修订：底部上滑是固定工具栏入口，短按和长按可自由设置；启动与恢复不再强制改成工具栏，提交失败仍保留原映射。
 * User revision: bottom-edge swipes provide a permanent toolbar entry, freeing all short/hold mappings. Boot/restore no longer force a toolbar key; failed commits retain previous mappings.
 * 用户修订：三键短按映射单独持久化；备份v11追加长按映射，旧备份缺少长按映射时恢复返回首页。
 * User revision: persist short key mappings; backup v11 adds a checked hold action; older backups without a hold mapping default to home. Five reading stroke levels are sealed in backup v13.
 * 用户修订：首行缩进微调-20..20px单独持久化，备份v12追加校验块；旧备份恢复0，不更改密码凭据。
 * User revision: persist a -20..20px indent adjustment; v12 adds a checked block, legacy backups default to zero and leave PIN credentials intact.
 * NVS load/store. A failed open keeps the deep-sleep default; the
 * partition is not erased.
 */

#include "settings.h"
#include "esp_app_desc.h"
#include "dpreader_board_config.h"
#define READER_TURN_EFFECT_MAX DPREADER_BOARD_READER_TURN_EFFECT_MAX
#if DPREADER_BOARD_METALIO47
#define MAIN_REFRESH_DEFAULT APP_MAIN_REFRESH_WATER
#define SYSTEM_CONTRAST_DEFAULT 130
#else
#define MAIN_REFRESH_DEFAULT APP_MAIN_REFRESH_NORMAL
#define SYSTEM_CONTRAST_DEFAULT 100
#endif
static bool reader_full_pages_valid(uint8_t pages) {
    return pages == 0 || pages == 1 || pages == 3 || pages == 5 || pages == 8 || pages == 10 || pages == 15 || pages == 20 || pages == 30;
}
#include "book/book_font_size.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <strings.h>

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "read_pico_sd.h"
#include "read_pico_transfer.h"
#include "book/book_history_backup.h"
#if defined(DPREADER_FONT_FLASH_TEST) || (defined(ESP_PLATFORM) && DPREADER_BOARD_METALIO47)
#include "book/book_store.h"
#include "font/font_settings_flash.h"
#define FONT_FLASH_FALLBACK 1
#endif


#if defined(ESP_PLATFORM) && DPREADER_BOARD_METALIO47
#include "metalio_preferences_backup.h"
#include "anti_fade.h"
#include "ble_pt_backup.h"
#endif

#if defined(ESP_PLATFORM) && DPREADER_BOARD_METALIO47
#include "esp_heap_caps.h"
#include "esp_attr.h"
#include "esp_mac.h"
#define BACKUP_STATE_RAM EXT_RAM_BSS_ATTR
#ifndef backup_alloc
#define backup_alloc(size) heap_caps_calloc(1, (size), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
#endif
#else
#define BACKUP_STATE_RAM
#ifndef backup_alloc
#define backup_alloc(size) calloc(1, (size))
#endif
#endif

#if CONFIG_DPREADER_AUTO_BACKUP
#if defined(ESP_PLATFORM) && (!DPREADER_BOARD_METALIO47 || !CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY)
#error "Automatic backup requires S31 with external BSS enabled"
#endif
#include "book/book_progress.h"
static BACKUP_STATE_RAM struct {
    uint32_t revision, saved_revision, saved_progress, saved_wifi, saved_preferences;
    bool healthy, failed, saved, enabled;
#if CONFIG_DPREADER_AUTO_RESTORE
    bool restore_pending, boot_checked;
    app_restore_status_t restore_status;
#endif
} s_auto_backup;
static esp_err_t settings_backup_commit(nvs_handle_t h) {
    esp_err_t err = nvs_commit(h);
    if (err == ESP_OK) ++s_auto_backup.revision;
    else s_auto_backup.failed = true;
    return err;
}
static esp_err_t backup_check_nvs(esp_err_t err) {
    if (err != ESP_OK) s_auto_backup.failed = true;
    return err;
}
static esp_err_t backup_nvs_open(const char *ns, int mode, nvs_handle_t *h) {
    return backup_check_nvs(nvs_open(ns, mode, h));
}
static esp_err_t backup_nvs_set_u8(nvs_handle_t h, const char *key, uint8_t value) {
    return backup_check_nvs(nvs_set_u8(h, key, value));
}
static esp_err_t backup_nvs_set_u32(nvs_handle_t h, const char *key, uint32_t value) {
    return backup_check_nvs(nvs_set_u32(h, key, value));
}
static esp_err_t backup_nvs_set_str(nvs_handle_t h, const char *key, const char *value) {
    return backup_check_nvs(nvs_set_str(h, key, value));
}
#else
#define settings_backup_commit nvs_commit
#define backup_nvs_open nvs_open
#define backup_nvs_set_u8 nvs_set_u8
#define backup_nvs_set_u32 nvs_set_u32
#define backup_nvs_set_str nvs_set_str
#endif

#define TAG "settings"
#define NVS_NS DPREADER_BOARD_SETTINGS_NAMESPACE
#define SIMPLE_UI_NS DPREADER_BOARD_SETTINGS_NAMESPACE "_ui"
#define NVS_KEY_SLEEP "sleep"
#define NVS_KEY_SLEEP_OFF_HOURS "slp_off_h"
#define NVS_KEY_SHUTDOWN_MODE "shutdown"
#define NVS_KEY_WAKE_PRESSES "wake_press"
#if DPREADER_BOARD_METALIO47 && !DPREADER_LOVE_BOOK
#define NVS_KEY_PIN_GRACE "pin_grace"
#define NVS_KEY_PIN_LOCK_AT "pin_lck_at"
#endif
#define NVS_KEY_HOME_FULL "home_full"
#define NVS_KEY_HOME_COMPONENT "home_widget"
#define NVS_KEY_HOME_GOAL "home_goal"
#define NVS_KEY_MAIN_REFRESH "main_mode"
#define NVS_KEY_AUTO_LOCK "idle_lock"
#define NVS_KEY_WIFI_TIMEOUT "wifi_tout"
#define NVS_KEY_BLE_TIMEOUT "ble_tout"
#define NVS_KEY_DEVICE_NAME "dev_name"
#define NVS_KEY_AVATAR "dev_avatar"
#define NVS_KEY_STATUS_SIGNATURE "status_sig"
#define NVS_KEY_FONT "font"
#define NVS_KEY_SYS_FONT "sys_font"
#define NVS_KEY_SYS_SIZE "sys_size"
#define NVS_KEY_INVERT "screen_invert"
#define NVS_KEY_SYS_CONTRAST "sys_contrast"
#define NVS_KEY_LOCK_STYLE "lock_ui"
#define NVS_KEY_WALLPAPER "wallpaper"
#define NVS_KEY_BOOKMARK "lock_png"
#define NVS_KEY_WAKE "lwake"
#define NVS_KEY_BOOT "lboot"
#define NVS_KEY_PICKUP "pickup"
#define NVS_KEY_BOOK_PX "bk_px"
#define NVS_KEY_BOOK_WEIGHT "bk_weight"
#define NVS_KEY_BOOK_WEIGHT_FINE "bk_wfine"
#define NVS_KEY_BOOK_AA "bk_aa"
#define NVS_KEY_BOOK_TEXT_CONTRAST "bk_txt_contrast"
#define NVS_KEY_BOOK_DROP_CAP "bk_dropcap"
#define NVS_KEY_BOOK_BACKGROUND "bk_bg"
#define NVS_KEY_READER_FULL "rd_gc16"
#define NVS_KEY_READER_TURN "rd_turn"
#define NVS_KEY_POWER_TURN "rd_power"
#define NVS_KEY_IMMERSIVE "rd_immersive"
#define NVS_KEY_HIDE_IMAGES "rd_no_image"
#define NVS_KEY_HOLD_REFRESH "rd_hold_full"
#define NVS_KEY_HOLD_ACTION "rd_key_hold"
#define NVS_KEY_READER_KEYS_V2 "rd_keys_v2"
#define NVS_KEY_VERTICAL_TURN "rd_vertical"
#define NVS_KEY_BLE_TURNER "ble_turn"
#define NVS_KEY_SHELF_RECENT "shelf_recent"
#define NVS_KEY_BOOK_LINE "bk_line"
#define NVS_KEY_BOOK_PARA "bk_para"
#define NVS_KEY_BOOK_MARGIN "bk_margin"
#define NVS_KEY_BOOK_TRACK "bk_track"
#define NVS_KEY_BOOK_INDENT "bk_indent"
#define NVS_KEY_BOOK_INDENT_ADJUST "bk_indent_adj"
#define NVS_KEY_BOOK_ALIGN "bk_align"
#define NVS_KEY_BOOK_PROGRESS "bk_prog"
// 页脚新增“隐藏”档，改用独立键避免与旧 0..2 存档混淆。/ The footer gained a hidden stop; a new key keeps it apart from legacy 0..2.
#define NVS_KEY_BOOK_PROGRESS_V2 "bk_prog2"
#define NVS_KEY_BOOK_FOOTER_PAGE "bk_ft_page"
#define NVS_KEY_BOOK_FOOTER_CHAPTER "bk_ft_chap"
#define NVS_KEY_BOOK_FOOTER_PCT "bk_ft_pct"
#define NVS_KEY_BOOK_RULE "bk_rule"
#define NVS_KEY_BOOK_RULE_OFFSET "bk_rule_y"
#define NVS_KEY_SHELF_STYLE "shelf_ui"
#define NVS_KEY_SHELF_V22 "shelf_v22"
#define NVS_KEY_BOOKS_DIR "books_dir"
#define NVS_KEY_FONTS_DIR "fonts_dir"
#define NVS_KEY_BOOK_FONTS "bk_fonts"
#define NVS_KEY_BOOK_VERTICAL "bk_vert"
#define NVS_KEY_KOSYNC_URL "ks_url"
#define NVS_KEY_KOSYNC_USER "ks_user"
#define NVS_KEY_KOSYNC_KEY "ks_key"
#define NVS_KEY_KOSYNC_AUTO "ks_auto"
#define NVS_KEY_OPDS_URL "opds_url"
#define NVS_KEY_OPDS_USER "opds_user"
#define NVS_KEY_OPDS_PASS "opds_pass"
#define FONT_PATH_MAX 160
#define MEDIA_DIR_MAX 96

static app_sleep_mode_t s_sleep = APP_SLEEP_DEEP;
static uint8_t s_sleep_off_hours=APP_SLEEP_OFF_HOURS_DEFAULT;
#if DPREADER_BOARD_METALIO47
static bool s_staged_shutdown = true;
#else
static bool s_staged_shutdown;
#endif
static uint8_t s_wake_presses = 1;
#if DPREADER_BOARD_METALIO47 && !DPREADER_LOVE_BOOK
static uint8_t s_pin_grace_code_v;
static uint32_t s_pin_locked_at;
#endif
static bool s_home_full_refresh;
static bool s_simple_ui;
static bool s_simple_full_ui;
static bool s_simple_choice_known;
static uint8_t s_simple_utilities[2];
static bool s_simple_labels=true,s_simple_border=true;
_Static_assert(APP_HOME_UTIL_COUNT<=16,"Simple utility backup uses two checked nibbles");
static const char *s_simple_utility_keys[]={"simple_util2","simple_util3"};
static uint8_t s_home_utilities[APP_HOME_UTILITY_SLOTS];
static bool s_home_utility_labels=true;
static bool s_home_utility_border=true;
static const char *s_home_utility_keys[]={"home_util0","home_util1","home_util2","home_util3"};
static app_home_component_t s_home_component = APP_HOME_FOLDER;
static uint8_t s_home_goal_minutes = 30;
static app_main_refresh_mode_t s_main_refresh = MAIN_REFRESH_DEFAULT;
#if DPREADER_BOARD_METALIO47
static char s_device_name[64] = "dpReader";
#else
static char s_device_name[64] = "dpReader";
#endif
static char s_kosync_url[128] = DPREADER_DEFAULT_KOSYNC_URL;
static char s_kosync_user[64];
static char s_kosync_key[36];
static bool s_kosync_auto;
static uint8_t s_ota_edition = 255;
static char s_opds_url[160] = DPREADER_DEFAULT_OPDS_URL;
static char s_opds_user[64];
static char s_opds_pass[64];
#if !DPREADER_LOVE_BOOK
static char s_avatar[288];
#endif
static char s_status_signature[96];
#define DEFAULT_READER_FONT "/builtin/Bitter-Regular.ttf"
static char s_font[FONT_PATH_MAX] = DEFAULT_READER_FONT;
static char s_system_font[FONT_PATH_MAX];

#if FONT_FLASH_FALLBACK
#ifndef FONT_SETTINGS_FLASH_ROOT
#define FONT_SETTINGS_FLASH_ROOT "/flash"
#endif
static const font_settings_files_t reader_font_files = {
    FONT_SETTINGS_FLASH_ROOT "/.reader-font", FONT_SETTINGS_FLASH_ROOT "/.reader-font.old", FONT_SETTINGS_FLASH_ROOT "/.reader-font.tmp",
};
static const font_settings_files_t system_font_files = {
    FONT_SETTINGS_FLASH_ROOT "/.system-font", FONT_SETTINGS_FLASH_ROOT "/.system-font.old", FONT_SETTINGS_FLASH_ROOT "/.system-font.tmp",
};

static void load_flash_fonts(void) {
    if (book_store_mount_flash() != ESP_OK) return;
    char path[FONT_PATH_MAX];
    if (font_settings_read(&reader_font_files, path, sizeof(path)))
        strlcpy(s_font, path, sizeof(s_font));
    if (font_settings_read(&system_font_files, path, sizeof(path)))
        strlcpy(s_system_font, path, sizeof(s_system_font));
}

esp_err_t app_settings_clear_flash_fonts(void) {
    if (book_store_mount_flash() != ESP_OK) return ESP_OK;
    return font_settings_clear(&reader_font_files) && font_settings_clear(&system_font_files)
        ? ESP_OK : ESP_FAIL;
}
#endif

static uint8_t s_system_size = 100;
static bool s_screen_inverted;
static uint8_t s_system_contrast = SYSTEM_CONTRAST_DEFAULT;
static uint8_t s_lock_style;
static char s_wallpaper[288];
static char s_bookmark[288];
static uint8_t s_last_wake;
static uint8_t s_last_boot;
static bool s_pickup_wake;
static uint8_t s_book_px = BOOK_READER_PX_DEFAULT;
static uint8_t s_book_weight = 2;
static int8_t s_book_weight_fine;
static bool s_book_antialias = true;
static bool s_book_text_contrast;
static bool s_book_drop_cap;
static char s_book_background[288];
static uint8_t s_reader_full_pages = 3;
static uint8_t s_reader_turn_effect;
static uint8_t s_book_line = 130, s_book_para = 50, s_book_margin = 24;
static bool s_reader_power_turn;
static bool s_reader_immersive;
static bool s_reader_hide_images;
static bool s_reader_hold_refresh;
static const char *s_reader_key_names[] = {"rd_key_l", "rd_key_m", "rd_key_r"};
static uint8_t s_reader_hold_action = APP_READER_KEY_HOME;
static uint8_t s_reader_keys[3] = {APP_READER_KEY_PREV, APP_READER_KEY_TOOLS, APP_READER_KEY_NEXT};
static uint8_t s_reader_turn_mode = 0;
static uint8_t s_volume_keys[2] = {APP_READER_KEY_PREV, APP_READER_KEY_NEXT};
static uint8_t s_control_holds[7]={255,APP_READER_KEY_NONE,APP_READER_KEY_NONE,255,255,APP_READER_KEY_NONE,APP_READER_KEY_NONE};
static const char *const s_control_hold_names[]={"hold_ai","hold_vup","hold_vdown","hold_power","hold_mid","hold_left","hold_right"};
static bool s_reader_swipe_up_tools = true;
static bool s_reader_edge_brightness=true,s_reader_edge_warmth=true;
static bool s_reader_swipe_bottom_home = true;
static bool s_ble_turner;
#if DPREADER_BOARD_METALIO47
static app_ai_action_t s_ai_short=APP_AI_REFRESH;
static uint32_t s_action_menu=APP_ACTION_MENU_DEFAULT;
static bool s_power_action_menu;
#endif
static bool s_shelf_recent_sort;
// 书内自带字体默认开启：多数带字体的书排版就是靠它才对得上版。
// Embedded book faces are on by default: most books that ship a face only typeset correctly
// with it.
static bool s_book_fonts = true;
// 竖排默认关：只有台湾、日本等竖排读者会主动打开，横排排版不受影响。
// Vertical setting defaults off: only readers who want it turn it on, and horizontal type is
// untouched.
static bool s_book_vertical;
static int8_t s_book_word_spacing;
static uint8_t s_book_tracking = -APP_BOOK_TRACK_MIN, s_book_reading_line, s_book_rule_offset = 4;
static uint8_t s_book_indent = 2;
static uint8_t s_book_indent_adjust = 20;
// 正文对齐方式与页脚进度条外观，默认沿用旧行为（两端对齐、细圆头进度条）。
// Body alignment and footer progress-bar look; defaults keep the previous behaviour
// (justified text, thin round-capped bar).
static uint8_t s_book_align;
// 默认细条；0 现在是“隐藏”，不能当默认值。/ Default to the thin bar; 0 now means hidden.
static uint8_t s_book_progress_style = APP_BOOK_PROGRESS_STYLE_THIN;
static bool s_book_footer_enabled = true;
static bool s_book_status_hidden;
static bool s_book_footer_page = true;
static bool s_book_footer_chapter = true;
static bool s_book_footer_pct = true;
static uint8_t s_auto_lock_minutes = 5;
static uint8_t s_wifi_timeout_minutes = 3;
static uint8_t s_ble_timeout_minutes = 10;
static uint8_t s_shelf_style = 2;
static char s_books_dir[MEDIA_DIR_MAX] = "/sdcard/books";
static char s_fonts_dir[MEDIA_DIR_MAX] = "/sdcard/fonts";
static void nvs_put_u8(const char* key, uint8_t value);

// 旧存档里 bk_prog 是 0..2（细/粗/极细）。新枚举在最前面插入了“隐藏”档，
// 所以只有“还没有 bk_prog2”的旧记录才需要 +1；新键一旦写过就以它为准，
// 否则用户选中的“隐藏”(0) 会在重启后被误读成“细条”。
// Legacy bk_prog stored 0..2 (thin/thick/hairline). The new enum inserts "hidden" first, so
// only records without bk_prog2 shift by one. Once the new key exists it wins, otherwise a
// saved "hidden" (0) would read back as "thin" after a reboot.
static uint8_t book_progress_style_from_legacy(uint8_t stored) {
    return stored <= APP_BOOK_PROGRESS_STYLE_HAIRLINE - 1
        ? (uint8_t)(stored + 1) : APP_BOOK_PROGRESS_STYLE_THIN;
}
#if DPREADER_BOARD_METALIO47 && !DPREADER_LOVE_BOOK
static void nvs_put_u32(const char* key,uint32_t value);
#endif

static bool valid_media_dir(const char* path) {
    if (!path || strncmp(path, "/sdcard/", 8) || !path[8] ||
        strlen(path) >= MEDIA_DIR_MAX || path[strlen(path) - 1] == '/') return false;
    const char *segment = path + 8;
    for (const unsigned char *p = (const unsigned char *)segment; *p; ++p) {
        if (*p < 32 || *p == 127 || *p == '\\' || *p == ':') return false;
        if (*p == '/' && (p == (const unsigned char *)segment || p[-1] == '/')) return false;
    }
    for (const char *p = segment; *p;) {
        const char *end = strchr(p, '/');
        size_t n = end ? (size_t)(end - p) : strlen(p);
        if ((n == 1 && p[0] == '.') || (n == 2 && p[0] == '.' && p[1] == '.')) return false;
        if (!end) break;
        p = end + 1;
    }
    return true;
}

static bool set_media_dir(char *dst, const char *key, const char *path) {
    if (!valid_media_dir(path)) return false;
    nvs_handle_t h;
    if (backup_nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return false;
    esp_err_t err = backup_nvs_set_str(h, key, path);
    if (err == ESP_OK) err = settings_backup_commit(h);
    nvs_close(h);
    if (err != ESP_OK) return false;
    strlcpy(dst, path, MEDIA_DIR_MAX);
    return true;
}

static uint8_t valid_book_px(uint8_t px) {
    return px >= BOOK_READER_PX_MIN && px <= BOOK_READER_PX_MAX ? px : BOOK_READER_PX_DEFAULT;
}

/* Reset Sleep once per firmware version, then retain the user's choices.
 * Marker and policy share a commit; failures keep the safe live default. */
static void sleep_policy_load(void) {
    s_sleep_off_hours=APP_SLEEP_OFF_HOURS_DEFAULT;
    nvs_handle_t h;
    if(backup_nvs_open(NVS_NS,NVS_READWRITE,&h)!=ESP_OK)return;
    const char *version=esp_app_get_description()->version;
    char previous[32]={0};size_t size=sizeof(previous);
    if(nvs_get_str(h,"sleep_fw",previous,&size)==ESP_OK&&!strcmp(previous,version)) {
        uint8_t hours=0;
        if(nvs_get_u8(h,NVS_KEY_SLEEP_OFF_HOURS,&hours)==ESP_OK&&hours<=APP_SLEEP_OFF_HOURS_MAX)s_sleep_off_hours=hours;
    }else {
        esp_err_t err=backup_nvs_set_u8(h,NVS_KEY_SLEEP_OFF_HOURS,APP_SLEEP_OFF_HOURS_DEFAULT);
        if(err==ESP_OK)err=backup_nvs_set_str(h,"sleep_fw",version);
        if(err==ESP_OK)err=settings_backup_commit(h);
        if(err!=ESP_OK)ESP_LOGW(TAG,"Sleep policy upgrade reset deferred: %s",esp_err_to_name(err));
    }
    nvs_close(h);
}

/* Apply the requested larger UI once; later explicit choices remain persistent. */
static void system_size_load(void) {
    s_system_size=100;
    nvs_handle_t h;
    if(backup_nvs_open(NVS_NS,NVS_READWRITE,&h)!=ESP_OK)return;
    uint8_t revision=0,value=100;
    if(nvs_get_u8(h,"sys_size_rev",&revision)==ESP_OK && revision==2) {
        if(nvs_get_u8(h,NVS_KEY_SYS_SIZE,&value)==ESP_OK && value>=100 &&
           value<=202 && (!(value%10) || value==144 || value==202))s_system_size=value;
    }else {
        esp_err_t err=backup_nvs_set_u8(h,NVS_KEY_SYS_SIZE,100);
        if(err==ESP_OK)err=backup_nvs_set_u8(h,"sys_size_rev",2);
        if(err==ESP_OK)(void)settings_backup_commit(h);
    }
    nvs_close(h);
}

/* Local UI choice is independent of profile restores and namespace resets.
 * Only a valid persisted value can replace the in-memory choice. */
static void simple_mode_load(void) {
    nvs_handle_t h;uint8_t value=0;
    esp_err_t err=nvs_open(SIMPLE_UI_NS,NVS_READONLY,&h);
    if(err==ESP_OK){err=nvs_get_u8(h,"mode",&value);nvs_close(h);}
    if(err==ESP_OK&&value<=2){s_simple_ui=value!=0;s_simple_full_ui=value!=0;s_simple_choice_known=true;return;}
    if(err!=ESP_ERR_NVS_NOT_FOUND){if(!s_simple_choice_known)s_simple_ui=true;return;}
    if(s_simple_choice_known)return;
    err=nvs_open(NVS_NS,NVS_READONLY,&h);
    if(err!=ESP_OK){if(err!=ESP_ERR_NVS_NOT_FOUND)s_simple_ui=true;return;}
    err=nvs_get_u8(h,"simple_ui",&value);nvs_close(h);
    if(err!=ESP_OK||value>1){if(err!=ESP_ERR_NVS_NOT_FOUND)s_simple_ui=true;return;}
    s_simple_ui=value==1;s_simple_full_ui=s_simple_ui;s_simple_choice_known=true;
    if(nvs_open(SIMPLE_UI_NS,NVS_READWRITE,&h)!=ESP_OK)return;
    err=nvs_set_u8(h,"mode",value);
    if(err==ESP_OK)(void)nvs_commit(h);
    nvs_close(h);
}
#if defined(ESP_PLATFORM) && DPREADER_BOARD_METALIO47
#include "esp_partition.h"
static void nvs_layout_check_and_report(bool report) {
    const esp_partition_t *part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_NVS, "nvs");
    // 旧布局禁止启动，避免误用TF/OTA迁移。/ Reject old layouts; migration requires the USB installer.
    ESP_ERROR_CHECK(part && part->address == 0xfb0000 && part->size == 0x40000 ? ESP_OK : ESP_ERR_INVALID_STATE);
    if (!report) return;
    nvs_stats_t stats;
    if (nvs_get_stats("nvs", &stats) == ESP_OK)
        ESP_LOGI(TAG, "NVS256 used=%u free=%u total=%u namespaces=%u", (unsigned)stats.used_entries,
                 (unsigned)stats.free_entries, (unsigned)stats.total_entries, (unsigned)stats.namespace_count);
    nvs_iterator_t it = NULL;
    esp_err_t err = nvs_entry_find("nvs", NULL, NVS_TYPE_ANY, &it);
    char previous[16] = {0};
    while (err == ESP_OK) {
        nvs_entry_info_t info;
        if (nvs_entry_info(it, &info) != ESP_OK) break;
        if (strcmp(previous, info.namespace_name)) {
            nvs_handle_t handle;
            size_t used = 0;
            if (nvs_open(info.namespace_name, NVS_READONLY, &handle) == ESP_OK) {
                if (nvs_get_used_entry_count(handle, &used) == ESP_OK)
                    ESP_LOGI(TAG, "NVS namespace=%s entries=%u", info.namespace_name, (unsigned)used);
                nvs_close(handle);
            }
            snprintf(previous, sizeof(previous), "%s", info.namespace_name);
        }
        err = nvs_entry_next(&it);
    }
    nvs_release_iterator(it);
}
#endif

static bool book_line_adjustment_valid(uint8_t value) {
    const int percent = (int)(int8_t)value;
    return percent >= -70 && percent <= 30 && percent % 10 == 0;
}

void app_settings_init(void) {
#if CONFIG_DPREADER_AUTO_BACKUP
    memset(&s_auto_backup, 0, sizeof(s_auto_backup));
    s_auto_backup.enabled = true;
#endif
#if defined(ESP_PLATFORM) && DPREADER_BOARD_METALIO47
    nvs_layout_check_and_report(false);
#endif
    esp_err_t err = nvs_flash_init();
    // 设置分区异常时保留原数据供恢复，绝不因空间不足自动擦除用户资料。
    // Preserve settings on NVS errors; a full partition must never silently erase profile data.
    if (err != ESP_OK) {
        if(!s_simple_choice_known)s_simple_ui=true;
#if CONFIG_DPREADER_AUTO_RESTORE
        s_auto_backup.restore_status = APP_RESTORE_BLOCKED;
#endif
        ESP_LOGW(TAG, "nvs init %s, use deep sleep", esp_err_to_name(err));
        return;
    }

#if defined(ESP_PLATFORM) && DPREADER_BOARD_METALIO47
    nvs_layout_check_and_report(true);
#endif
    simple_mode_load();
#if CONFIG_DPREADER_AUTO_BACKUP
    // Persist quarantine before migrations recreate defaults on an erased device.
    nvs_handle_t existing;
    err = nvs_open(NVS_NS, NVS_READONLY, &existing);
    if (err == ESP_OK) {
        uint8_t guard = 0;
        esp_err_t state = nvs_get_u8(existing, "backup_guard", &guard);
        s_auto_backup.healthy = state == ESP_ERR_NVS_NOT_FOUND || (state == ESP_OK && guard == 1);
        if (state != ESP_OK && state != ESP_ERR_NVS_NOT_FOUND) s_auto_backup.failed = true;
        uint8_t enabled = 1;
        esp_err_t preference = nvs_get_u8(existing, "auto_backup", &enabled);
        if (preference == ESP_OK && enabled <= 1) s_auto_backup.enabled = enabled == 1;
        else if (preference != ESP_ERR_NVS_NOT_FOUND) s_auto_backup.failed = true;
        nvs_close(existing);
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        if (backup_nvs_open(NVS_NS, NVS_READWRITE, &existing) == ESP_OK) {
            esp_err_t guard_err = backup_nvs_set_u8(existing, "backup_guard", 0);
#if CONFIG_DPREADER_AUTO_RESTORE
            if (guard_err == ESP_OK) guard_err = backup_nvs_set_u8(existing, "restore_state", 0);
#endif
            if (guard_err == ESP_OK) (void)settings_backup_commit(existing);
            nvs_close(existing);
        }
    } else s_auto_backup.failed = true;
#endif
#if CONFIG_DPREADER_AUTO_RESTORE
    if (s_auto_backup.failed) {
        s_auto_backup.restore_status = APP_RESTORE_BLOCKED;
        return;
    }
    // An interrupted journal overrides even a surviving guard=1 key.
    err = nvs_open(NVS_NS, NVS_READONLY, &existing);
    if (err != ESP_OK) { s_auto_backup.restore_status = APP_RESTORE_BLOCKED; return; }
    uint8_t restore_state = 0;
    esp_err_t journal = nvs_get_u8(existing, "restore_state", &restore_state);
    nvs_close(existing);
    if ((journal != ESP_OK && journal != ESP_ERR_NVS_NOT_FOUND) ||
        (journal == ESP_OK && restore_state != 0 && restore_state != 2)) {
        s_auto_backup.healthy = false;
        s_auto_backup.restore_status = APP_RESTORE_BLOCKED;
        return;
    }
    if (!s_auto_backup.healthy) {
        s_auto_backup.restore_pending = restore_state == 0;
        if (!s_auto_backup.restore_pending) s_auto_backup.restore_status = APP_RESTORE_BLOCKED;
        return; // Do not persist/reload defaults over an unfinished recovery.
    }
#endif
    sleep_policy_load();
    system_size_load();
    nvs_handle_t h;
    if (backup_nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) {
#if CONFIG_DPREADER_AUTO_BACKUP
        s_auto_backup.healthy = false;
#endif
#if FONT_FLASH_FALLBACK
        load_flash_fonts();
#endif
        return;
    }
    uint8_t raw = APP_SLEEP_DEEP;
    if (nvs_get_u8(h, NVS_KEY_SLEEP, &raw) == ESP_OK && raw <= APP_SLEEP_OFF) {
        s_sleep = (app_sleep_mode_t)raw;
    }
    raw = 0;
#if DPREADER_BOARD_METALIO47
    if (nvs_get_u8(h, NVS_KEY_SHUTDOWN_MODE, &raw) == ESP_OK && raw <= 1)
#else
    if (nvs_get_u8(h, NVS_KEY_SHUTDOWN_MODE, &raw) == ESP_OK)
#endif
        s_staged_shutdown = raw == 1;
    raw = 1;
    if (nvs_get_u8(h, NVS_KEY_WAKE_PRESSES, &raw) != ESP_OK || (raw != 1 && raw != 2)) raw = 1;
    s_wake_presses = raw;
#if DPREADER_BOARD_METALIO47 && !DPREADER_LOVE_BOOK
    s_pin_grace_code_v=0;raw=0;
    if(nvs_get_u8(h,NVS_KEY_PIN_GRACE,&raw)==ESP_OK && raw<=4)s_pin_grace_code_v=raw;
    s_pin_locked_at=0;
    (void)nvs_get_u32(h,NVS_KEY_PIN_LOCK_AT,&s_pin_locked_at);
#endif
    raw = 0;
    for(unsigned i=0;i<2;++i) {
        raw=0;s_simple_utilities[i]=nvs_get_u8(h,s_simple_utility_keys[i],&raw)==ESP_OK&&raw<APP_HOME_UTIL_COUNT?raw:APP_HOME_UTIL_NONE;
    }
    raw=1;s_simple_labels=nvs_get_u8(h,"simple_labels",&raw)!=ESP_OK||raw!=0;
    raw=1;s_simple_border=nvs_get_u8(h,"simple_border",&raw)!=ESP_OK||raw!=0;
    if (nvs_get_u8(h, NVS_KEY_HOME_FULL, &raw) == ESP_OK) s_home_full_refresh = raw == 1;
    for(unsigned i=0;i<APP_HOME_UTILITY_SLOTS;++i) {
        raw=0;
        s_home_utilities[i]=nvs_get_u8(h,s_home_utility_keys[i],&raw)==ESP_OK&&raw<APP_HOME_UTIL_COUNT?raw:APP_HOME_UTIL_NONE;
    }
    raw=1;
    s_home_utility_labels=nvs_get_u8(h,"home_util_lbl",&raw)!=ESP_OK||raw!=0;
    raw=1;
    s_home_utility_border=nvs_get_u8(h,"home_util_bdr",&raw)!=ESP_OK||raw!=0;
    raw = 0;
    s_home_component = nvs_get_u8(h, NVS_KEY_HOME_COMPONENT, &raw) == ESP_OK && raw < APP_HOME_COMPONENT_COUNT
        ? (raw == 3 ? APP_HOME_CLOCK : (app_home_component_t)raw) : APP_HOME_FOLDER;
    raw = 30;
    s_home_goal_minutes = nvs_get_u8(h, NVS_KEY_HOME_GOAL, &raw) == ESP_OK && raw >= 5 && raw <= 180 && raw % 5 == 0 ? raw : 30;
    raw = 0;
    s_main_refresh = nvs_get_u8(h, NVS_KEY_MAIN_REFRESH, &raw) == ESP_OK && raw <= APP_MAIN_REFRESH_WATER
        ? (app_main_refresh_mode_t)raw : MAIN_REFRESH_DEFAULT;
    raw = 0;

    raw = 5;
    if (nvs_get_u8(h, NVS_KEY_AUTO_LOCK, &raw) == ESP_OK && (raw == 0 || raw == 1 || raw == 5 || raw == 10)) s_auto_lock_minutes = raw;
    raw = 255;
    s_ota_edition = nvs_get_u8(h, "ota_edition", &raw) == ESP_OK && raw <= 1 ? raw : 255;
    raw = 3;
    if (nvs_get_u8(h, NVS_KEY_WIFI_TIMEOUT, &raw) == ESP_OK && (raw == 0 || raw == 1 || raw == 3 || raw == 5 || raw == 10)) s_wifi_timeout_minutes = raw;
    raw = 10;
    if (nvs_get_u8(h, NVS_KEY_BLE_TIMEOUT, &raw) == ESP_OK && (raw == 0 || raw == 5 || raw == 10 || raw == 15 || raw == 30)) s_ble_timeout_minutes = raw;
    size_t value_len = sizeof(s_device_name);
    if (nvs_get_str(h, NVS_KEY_DEVICE_NAME, s_device_name, &value_len) != ESP_OK || !s_device_name[0])
        strlcpy(s_device_name, "dpReader", sizeof(s_device_name));
    // 仅迁移旧默认名字，保留用户填写的姓名。/ Migrate only the old default, preserving custom names.
    if (!strcmp(s_device_name, "Pico") || !strcmp(s_device_name, "kiikoread") || !strcmp(s_device_name, "Metalio S31"))
        strlcpy(s_device_name, "dpReader", sizeof(s_device_name));
    value_len = sizeof(s_kosync_url);
    if (nvs_get_str(h, NVS_KEY_KOSYNC_URL, s_kosync_url, &value_len) != ESP_OK || !s_kosync_url[0] ||
        !strcmp(s_kosync_url, "https://sync.nemmanquy.com") || !strcmp(s_kosync_url, "sync.nemmanquy.com"))
        strlcpy(s_kosync_url, DPREADER_DEFAULT_KOSYNC_URL, sizeof(s_kosync_url));
    value_len = sizeof(s_kosync_user);
    if (nvs_get_str(h, NVS_KEY_KOSYNC_USER, s_kosync_user, &value_len) != ESP_OK) s_kosync_user[0] = 0;
    value_len = sizeof(s_kosync_key);
    if (nvs_get_str(h, NVS_KEY_KOSYNC_KEY, s_kosync_key, &value_len) != ESP_OK) s_kosync_key[0] = 0;
    raw = 0;
    if (nvs_get_u8(h, NVS_KEY_KOSYNC_AUTO, &raw) == ESP_OK) s_kosync_auto = raw == 1;
    value_len = sizeof(s_opds_url);
    if (nvs_get_str(h, NVS_KEY_OPDS_URL, s_opds_url, &value_len) != ESP_OK || !s_opds_url[0] ||
        !strcmp(s_opds_url, "https://sach.nemmanquy.com/opds") || !strcmp(s_opds_url, "sach.nemmanquy.com/opds"))
        strlcpy(s_opds_url, DPREADER_DEFAULT_OPDS_URL, sizeof(s_opds_url));
    value_len = sizeof(s_opds_user);
    if (nvs_get_str(h, NVS_KEY_OPDS_USER, s_opds_user, &value_len) != ESP_OK) s_opds_user[0] = 0;
    value_len = sizeof(s_opds_pass);
    if (nvs_get_str(h, NVS_KEY_OPDS_PASS, s_opds_pass, &value_len) != ESP_OK) s_opds_pass[0] = 0;
#if !DPREADER_LOVE_BOOK
    value_len = sizeof(s_avatar);
    if (nvs_get_str(h, NVS_KEY_AVATAR, s_avatar, &value_len) != ESP_OK ||
        (s_avatar[0] && strncmp(s_avatar, "/sdcard/", 8))) s_avatar[0] = 0;
#endif
    value_len = sizeof(s_status_signature);
    if (nvs_get_str(h, NVS_KEY_STATUS_SIGNATURE, s_status_signature, &value_len) != ESP_OK)
        s_status_signature[0] = 0;
    size_t dir_len=sizeof(s_books_dir);
    if(nvs_get_str(h,NVS_KEY_BOOKS_DIR,s_books_dir,&dir_len)!=ESP_OK||!valid_media_dir(s_books_dir))
        strlcpy(s_books_dir,"/sdcard/books",sizeof(s_books_dir));
    dir_len=sizeof(s_fonts_dir);
    if(nvs_get_str(h,NVS_KEY_FONTS_DIR,s_fonts_dir,&dir_len)!=ESP_OK||!valid_media_dir(s_fonts_dir))
        strlcpy(s_fonts_dir,"/sdcard/fonts",sizeof(s_fonts_dir));
    size_t font_len = sizeof(s_font);
    if (nvs_get_str(h, NVS_KEY_FONT, s_font, &font_len) != ESP_OK) {
        strlcpy(s_font, DEFAULT_READER_FONT, sizeof(s_font));
    }
    font_len = sizeof(s_system_font);
    if (nvs_get_str(h, NVS_KEY_SYS_FONT, s_system_font, &font_len) != ESP_OK)
        s_system_font[0] = 0;
#if FONT_FLASH_FALLBACK
    load_flash_fonts();
#endif
    uint8_t inverted=0;
    s_screen_inverted=nvs_get_u8(h,NVS_KEY_INVERT,&inverted)==ESP_OK && inverted==1;
    uint8_t system_contrast;
    s_system_contrast = nvs_get_u8(h, NVS_KEY_SYS_CONTRAST, &system_contrast) == ESP_OK &&
        system_contrast >= 100 && system_contrast <= 140 && system_contrast % 10 == 0
        ? system_contrast : SYSTEM_CONTRAST_DEFAULT;
    uint8_t lock_style = 0;
    if (nvs_get_u8(h, NVS_KEY_LOCK_STYLE, &lock_style) == ESP_OK && lock_style <= 3)
        s_lock_style = lock_style;
    size_t wallpaper_len = sizeof(s_wallpaper);
    if (nvs_get_str(h, NVS_KEY_WALLPAPER, s_wallpaper, &wallpaper_len) != ESP_OK ||
        strncmp(s_wallpaper, "/sdcard/", 8)) s_wallpaper[0] = 0;
    size_t bookmark_len = sizeof(s_bookmark);
    if (nvs_get_str(h,NVS_KEY_BOOKMARK,s_bookmark,&bookmark_len)!=ESP_OK || strncmp(s_bookmark,"/sdcard/",8)) s_bookmark[0]=0;
    uint8_t wake = 0;
    if (nvs_get_u8(h, NVS_KEY_WAKE, &wake) == ESP_OK) s_last_wake = wake;
    uint8_t boot = 0;
    if (nvs_get_u8(h, NVS_KEY_BOOT, &boot) == ESP_OK) s_last_boot = boot;
    uint8_t pickup = 0;
    if (nvs_get_u8(h, NVS_KEY_PICKUP, &pickup) == ESP_OK) s_pickup_wake = pickup != 0;
    uint8_t weight = 2;
    s_book_weight = nvs_get_u8(h, NVS_KEY_BOOK_WEIGHT, &weight) == ESP_OK && weight <= 4 ? weight : 2;
    s_book_weight_fine = ((int)s_book_weight-2)*APP_BOOK_WEIGHT_SUBSTEPS;
    uint8_t fine = 32;
    if(nvs_get_u8(h,NVS_KEY_BOOK_WEIGHT_FINE,&fine)==ESP_OK && fine<=64)
        s_book_weight_fine=(int)fine-APP_BOOK_WEIGHT_FINE_MAX;
    uint8_t antialias = 1;
    s_book_antialias = nvs_get_u8(h, NVS_KEY_BOOK_AA, &antialias) != ESP_OK || antialias != 0;
    uint8_t text_contrast = 0;
    s_book_text_contrast = nvs_get_u8(h, NVS_KEY_BOOK_TEXT_CONTRAST, &text_contrast) == ESP_OK &&
        text_contrast == 1;
    uint8_t drop_cap = 0;
    s_book_drop_cap = nvs_get_u8(h, NVS_KEY_BOOK_DROP_CAP, &drop_cap) == ESP_OK && drop_cap == 1;
    uint8_t book_px = BOOK_READER_PX_DEFAULT;
    s_book_px = nvs_get_u8(h, NVS_KEY_BOOK_PX, &book_px) == ESP_OK
        ? valid_book_px(book_px) : BOOK_READER_PX_DEFAULT;
    size_t background_len = sizeof(s_book_background);
    if (nvs_get_str(h, NVS_KEY_BOOK_BACKGROUND, s_book_background, &background_len) != ESP_OK ||
        (s_book_background[0] && strncmp(s_book_background, "/sdcard/pictures/", 17))) s_book_background[0] = 0;
    uint8_t reader_full_pages = 3;
    if (nvs_get_u8(h, NVS_KEY_READER_FULL, &reader_full_pages) == ESP_OK &&
        reader_full_pages_valid(reader_full_pages))
        s_reader_full_pages = reader_full_pages;
    uint8_t reader_turn_effect = 0;
    if (nvs_get_u8(h, NVS_KEY_READER_TURN, &reader_turn_effect) == ESP_OK && reader_turn_effect <= READER_TURN_EFFECT_MAX)
        s_reader_turn_effect = reader_turn_effect;
    uint8_t power_turn = 0;
    if (nvs_get_u8(h, NVS_KEY_POWER_TURN, &power_turn) == ESP_OK) s_reader_power_turn = power_turn == 1;
    uint8_t immersive = 0;
    if (nvs_get_u8(h, NVS_KEY_IMMERSIVE, &immersive) == ESP_OK) s_reader_immersive = immersive == 1;
    uint8_t hold_refresh = 0;
    if (nvs_get_u8(h, NVS_KEY_HOLD_REFRESH, &hold_refresh) == ESP_OK) s_reader_hold_refresh = hold_refresh == 1;
    for (unsigned i = 0; i < 3; ++i) {
        uint8_t value;
        if (nvs_get_u8(h, s_reader_key_names[i], &value) == ESP_OK && value < APP_READER_KEY_COUNT)
            s_reader_keys[i] = value;
    }
    uint8_t hold_action = APP_READER_KEY_HOME;
    s_reader_hold_action = nvs_get_u8(h, NVS_KEY_HOLD_ACTION, &hold_action) == ESP_OK && hold_action < APP_READER_KEY_COUNT
        ? hold_action : APP_READER_KEY_HOME;
    uint8_t keys_v2 = 0;
    bool migrate_reader_defaults = nvs_get_u8(h, NVS_KEY_READER_KEYS_V2, &keys_v2) != ESP_OK || keys_v2 != 1;
    bool old_default_hold = migrate_reader_defaults && s_reader_hold_action == APP_READER_KEY_REFRESH &&
        s_reader_keys[0] == APP_READER_KEY_PREV && s_reader_keys[1] == APP_READER_KEY_TOOLS &&
        s_reader_keys[2] == APP_READER_KEY_NEXT;
    uint8_t vertical_turn = 0;
    if (nvs_get_u8(h, NVS_KEY_VERTICAL_TURN, &vertical_turn) == ESP_OK) s_reader_turn_mode = vertical_turn <= 2 ? vertical_turn : 0;
    uint8_t hide_images = 0, recent_sort = 0, ble_turner = 0, book_fonts = 1, book_vertical = 0;
    if (nvs_get_u8(h, NVS_KEY_HIDE_IMAGES, &hide_images) == ESP_OK) s_reader_hide_images = hide_images == 1;
    if (nvs_get_u8(h, NVS_KEY_BLE_TURNER, &ble_turner) == ESP_OK) s_ble_turner = ble_turner == 1;
#if DPREADER_BOARD_METALIO47
    uint32_t menu_mask;
    if(nvs_get_u32(h,"action_menu",&menu_mask)==ESP_OK&&(menu_mask&~APP_ACTION_MENU_ALLOWED)==0)s_action_menu=menu_mask;
    uint8_t power_menu=0;
    s_power_action_menu=nvs_get_u8(h,"power_menu",&power_menu)==ESP_OK&&power_menu==1;
    uint8_t ai_short=APP_AI_REFRESH;
    s_ai_short=nvs_get_u8(h,"ai_short",&ai_short)==ESP_OK&&ai_short<APP_AI_ACTION_COUNT?(app_ai_action_t)ai_short:APP_AI_REFRESH;
    uint8_t vol_down = APP_READER_KEY_PREV, vol_up = APP_READER_KEY_NEXT;
    if (nvs_get_u8(h, "vol_down", &vol_down) == ESP_OK && vol_down < APP_READER_KEY_COUNT) s_volume_keys[0] = vol_down;
    if (nvs_get_u8(h, "vol_up", &vol_up) == ESP_OK && vol_up < APP_READER_KEY_COUNT) s_volume_keys[1] = vol_up;
    for(unsigned i=0;i<7;++i) {
        uint8_t action;
        if(i!=4 && nvs_get_u8(h,s_control_hold_names[i],&action)==ESP_OK &&
           (action==APP_CONTROL_HOLD_DEFAULT || (action<APP_READER_KEY_COUNT && action!=APP_READER_KEY_FULLSCREEN)))s_control_holds[i]=action;
    }
    uint8_t edge;
    if(nvs_get_u8(h,"r_edge_light",&edge)==ESP_OK&&edge<=1)s_reader_edge_brightness=edge!=0;
    if(nvs_get_u8(h,"r_edge_warm",&edge)==ESP_OK&&edge<=1)s_reader_edge_warmth=edge!=0;
    uint8_t r_swipe_up = 1, r_swipe_bot = 1;
    if (nvs_get_u8(h, "r_swipe_up", &r_swipe_up) == ESP_OK) s_reader_swipe_up_tools = r_swipe_up == 1;
    if (nvs_get_u8(h, "r_swipe_bot", &r_swipe_bot) == ESP_OK) s_reader_swipe_bottom_home = r_swipe_bot == 1;
#endif
    if (nvs_get_u8(h, NVS_KEY_SHELF_RECENT, &recent_sort) == ESP_OK) s_shelf_recent_sort = recent_sort == 1;
    if (nvs_get_u8(h, NVS_KEY_BOOK_FONTS, &book_fonts) == ESP_OK) s_book_fonts = book_fonts == 1;
    if (nvs_get_u8(h, NVS_KEY_BOOK_VERTICAL, &book_vertical) == ESP_OK) s_book_vertical = book_vertical == 1;
    uint8_t word_spacing=0;
    s_book_word_spacing=nvs_get_u8(h,"book_word_gap",&word_spacing)==ESP_OK&&(word_spacing<=20||word_spacing>=246)?(int8_t)word_spacing:0;
    s_book_tracking = -APP_BOOK_TRACK_MIN;
    uint8_t tracking = 2, reading_line = 0, rule_offset = 4, indent = 2, indent_adjust = 20;
    if (nvs_get_u8(h, NVS_KEY_BOOK_TRACK, &tracking) == ESP_OK) {
        if(tracking<=4)s_book_tracking=(uint8_t)((int)tracking*2-4-APP_BOOK_TRACK_MIN);
        else if(tracking>=128 && tracking<=148) {
            int px=(int)tracking-128-4;
            if(px>APP_BOOK_TRACK_MAX)px=APP_BOOK_TRACK_MAX;
            s_book_tracking=(uint8_t)(px-APP_BOOK_TRACK_MIN);
        } else if(tracking>=192 && tracking<192+APP_BOOK_TRACK_COUNT)s_book_tracking=tracking-192;
    }
    if (nvs_get_u8(h, NVS_KEY_BOOK_RULE, &reading_line) == ESP_OK && reading_line <= 2)
        s_book_reading_line = reading_line;
    if (nvs_get_u8(h, NVS_KEY_BOOK_RULE_OFFSET, &rule_offset) == ESP_OK && rule_offset <= 8)
        s_book_rule_offset = rule_offset;
    if (nvs_get_u8(h, NVS_KEY_BOOK_INDENT, &indent) == ESP_OK && indent <= 3)
        s_book_indent = indent;
    s_book_indent_adjust = nvs_get_u8(h, NVS_KEY_BOOK_INDENT_ADJUST, &indent_adjust) == ESP_OK && indent_adjust <= 200
        ? indent_adjust : 20;
    uint8_t align = 0, progress_style = 0;
    if (nvs_get_u8(h, NVS_KEY_BOOK_ALIGN, &align) == ESP_OK && align < APP_BOOK_ALIGN_COUNT)
        s_book_align = align;
    if (nvs_get_u8(h, NVS_KEY_BOOK_PROGRESS_V2, &progress_style) == ESP_OK &&
        progress_style < APP_BOOK_PROGRESS_STYLE_COUNT)
        s_book_progress_style = progress_style;
    else if (nvs_get_u8(h, NVS_KEY_BOOK_PROGRESS, &progress_style) == ESP_OK)
        s_book_progress_style = book_progress_style_from_legacy(progress_style);
    uint8_t ft_page = 1, ft_chap = 1, ft_pct = 1;
    uint8_t footer_enabled;
    if (nvs_get_u8(h, "rd_footer_on", &footer_enabled) == ESP_OK && footer_enabled <= 1)
        s_book_footer_enabled = footer_enabled != 0;
    uint8_t status_hidden;
    if (nvs_get_u8(h, "rd_status_off", &status_hidden) == ESP_OK && status_hidden <= 1)
        s_book_status_hidden = status_hidden != 0;
    if (nvs_get_u8(h, NVS_KEY_BOOK_FOOTER_PAGE, &ft_page) == ESP_OK) s_book_footer_page = ft_page == 1;
    if (nvs_get_u8(h, NVS_KEY_BOOK_FOOTER_CHAPTER, &ft_chap) == ESP_OK) s_book_footer_chapter = ft_chap == 1;
    if (nvs_get_u8(h, NVS_KEY_BOOK_FOOTER_PCT, &ft_pct) == ESP_OK) s_book_footer_pct = ft_pct == 1;
    uint8_t line = 130, para = 50, margin = 24;
    if (nvs_get_u8(h, NVS_KEY_BOOK_LINE, &line) == ESP_OK) {
        if(book_line_adjustment_valid(line)||(line>=100&&line<=130&&line%10==0))s_book_line=line;
        else if(line>=110&&line<=150)s_book_line=line>130?130:((line+5)/10)*10;
        else if (line == 180) s_book_line = 130;
    }
    if(nvs_get_u8(h,NVS_KEY_BOOK_PARA,&para)==ESP_OK){if(para==100)s_book_para=75;else if(app_settings_book_paragraph_value(app_settings_book_paragraph_level(para))==para)s_book_para=para;}
    if (nvs_get_u8(h, NVS_KEY_BOOK_MARGIN, &margin) == ESP_OK && margin <= 60)
        s_book_margin = margin > 48 ? 48 : margin;
    uint8_t shelf_style = 2;
    if (nvs_get_u8(h, NVS_KEY_SHELF_STYLE, &shelf_style) == ESP_OK && shelf_style >= 1 && shelf_style <= 5)
        s_shelf_style = shelf_style == 3 || shelf_style == 4 ? 2 : shelf_style;
    uint8_t shelf_v22 = 0;
    bool migrate_shelf = nvs_get_u8(h, NVS_KEY_SHELF_V22, &shelf_v22) != ESP_OK || shelf_v22 != 1;
    bool retired_shelf = shelf_style == 3 || shelf_style == 4;
    if (migrate_shelf) s_shelf_style = 2;
    // 固定媒体根目录；忽略旧版自定义目录键。/ Fixed media roots ignore retired custom-directory keys.
    nvs_close(h);
    if ((migrate_shelf || retired_shelf) && backup_nvs_open(NVS_NS, NVS_READWRITE, &h) == ESP_OK) {
        // 首次刷入此版只切换书架外观；随后尊重用户手动选择。/ Set acrylic once on upgrade, then preserve manual choices.
        esp_err_t saved = backup_nvs_set_u8(h, NVS_KEY_SHELF_STYLE, 2);
        if (saved == ESP_OK) saved = backup_nvs_set_u8(h, NVS_KEY_SHELF_V22, 1);
        if (saved == ESP_OK) (void)settings_backup_commit(h);
        nvs_close(h);
    }
    // Migrate only the former default preset once; keep all other user mappings.
    // Committed marker makes a later explicit full-refresh choice remain untouched.
    if (migrate_reader_defaults && backup_nvs_open(NVS_NS, NVS_READWRITE, &h) == ESP_OK) {
        esp_err_t saved = old_default_hold ? backup_nvs_set_u8(h, NVS_KEY_HOLD_ACTION, APP_READER_KEY_HOME) : ESP_OK;
        if (saved == ESP_OK) saved = backup_nvs_set_u8(h, NVS_KEY_READER_KEYS_V2, 1);
        if (saved == ESP_OK) saved = settings_backup_commit(h);
        nvs_close(h);
        if (saved == ESP_OK && old_default_hold) s_reader_hold_action = APP_READER_KEY_HOME;
    }
    ESP_LOGI(
        TAG, "sleep mode %s, font %s, reader weight %d, middle hold %u",
        app_sleep_mode_name(s_sleep),
        s_font[0] != '\0' ? s_font : "(builtin)",
        app_settings_book_weight(), (unsigned)s_reader_hold_action
    );
}

app_sleep_mode_t app_settings_sleep_mode(void) {
    return s_sleep;
}

const char* app_sleep_mode_name(app_sleep_mode_t mode) {
    switch (mode) {
        case APP_SLEEP_LIGHT: return "light";
        case APP_SLEEP_DEEP: return "deep";
        case APP_SLEEP_OFF: return "off";
        default: return "?";
    }
}

void app_settings_set_sleep_mode(app_sleep_mode_t mode) {
    if (mode > APP_SLEEP_OFF) mode = APP_SLEEP_DEEP;
    s_sleep = mode;
    nvs_handle_t h;
    if (backup_nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return;
    backup_nvs_set_u8(h, NVS_KEY_SLEEP, (uint8_t)mode);
    settings_backup_commit(h);
    nvs_close(h);
}

uint8_t app_settings_sleep_off_hours(void) { return s_sleep_off_hours; }
esp_err_t app_settings_set_sleep_off_hours(uint8_t hours) {
    if(hours>APP_SLEEP_OFF_HOURS_MAX)return ESP_ERR_INVALID_ARG;
    nvs_handle_t h;
    esp_err_t err=backup_nvs_open(NVS_NS,NVS_READWRITE,&h);
    if(err!=ESP_OK)return err;
    err=backup_nvs_set_u8(h,NVS_KEY_SLEEP_OFF_HOURS,hours);
    if(err==ESP_OK)err=backup_nvs_set_str(h,"sleep_fw",esp_app_get_description()->version);
    if(err==ESP_OK)err=settings_backup_commit(h);
    nvs_close(h);
    if(err==ESP_OK)s_sleep_off_hours=hours;
    return err;
}
bool app_settings_staged_shutdown(void) { return s_staged_shutdown; }
void app_settings_set_staged_shutdown(bool staged) {
    if (staged == s_staged_shutdown) return;
#if DPREADER_BOARD_METALIO47
    nvs_handle_t h;
    if(backup_nvs_open(NVS_NS,NVS_READWRITE,&h)!=ESP_OK) return;
    esp_err_t err=backup_nvs_set_u8(h,NVS_KEY_SHUTDOWN_MODE,staged?1:0);
    if(err==ESP_OK) err=settings_backup_commit(h);
    nvs_close(h);
    if(err==ESP_OK) s_staged_shutdown=staged;
#else
    s_staged_shutdown = staged;
    nvs_put_u8(NVS_KEY_SHUTDOWN_MODE, staged ? 1 : 0);
#endif
}
uint8_t app_settings_wake_presses(void) { return s_wake_presses; }
void app_settings_set_wake_presses(uint8_t presses) {
    if (presses != 2) presses = 1;
    if (presses == s_wake_presses) return;
    s_wake_presses = presses;
    nvs_put_u8(NVS_KEY_WAKE_PRESSES, presses);
}

#if DPREADER_BOARD_METALIO47 && !DPREADER_LOVE_BOOK
uint8_t app_settings_pin_grace_code(void) { return s_pin_grace_code_v; }
void app_settings_set_pin_grace_code(uint8_t code) {
    if (code > 4) code = 0;
    if (code == s_pin_grace_code_v) return;
    s_pin_grace_code_v = code;
    nvs_put_u8(NVS_KEY_PIN_GRACE, code);
}
unsigned app_settings_pin_grace_minutes(void) {
    static const unsigned minutes[5] = {0, 5, 60, 480, 1440};
    return s_pin_grace_code_v < 5 ? minutes[s_pin_grace_code_v] : 0;
}
uint32_t app_settings_pin_locked_at(void) { return s_pin_locked_at; }
void app_settings_record_pin_lock(uint32_t unix_seconds) {
    // 时钟无效清除旧记录，避免对时后复用旧宽限。/ Clear invalid-time records so later synchronization cannot reuse old grace.
    if (unix_seconds < 1704067200 || unix_seconds > 4102444799U) unix_seconds = 0;
    if (unix_seconds == s_pin_locked_at) return;
    s_pin_locked_at = unix_seconds;
    nvs_put_u32(NVS_KEY_PIN_LOCK_AT, unix_seconds);
}
static void nvs_put_u32(const char* key, uint32_t value) {
    nvs_handle_t h;
    if (backup_nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return;
    backup_nvs_set_u32(h, key, value);
    settings_backup_commit(h);
    nvs_close(h);
}
#endif

uint8_t app_settings_simple_utility(unsigned slot) {
    uint8_t action=slot>=2&&slot<4?s_simple_utilities[slot-2]:APP_HOME_UTIL_NONE;
    return DPREADER_LOVE_BOOK&&action==APP_HOME_UTIL_LIBRARY?APP_HOME_UTIL_FILES:action;
}
bool app_settings_simple_utility_labels(void){return s_simple_labels;}
bool app_settings_simple_utility_border(void){return s_simple_border;}
esp_err_t app_settings_set_simple_utilities(const uint8_t actions[2]) {
    return app_settings_set_simple_utilities_config(actions,s_simple_labels,s_simple_border);
}
esp_err_t app_settings_set_simple_utilities_config(const uint8_t actions[2],bool labels,bool border) {
    if(!actions||actions[0]>=APP_HOME_UTIL_COUNT||actions[1]>=APP_HOME_UTIL_COUNT)return ESP_ERR_INVALID_ARG;
    nvs_handle_t h;esp_err_t err=backup_nvs_open(NVS_NS,NVS_READWRITE,&h);if(err!=ESP_OK)return err;
    for(unsigned i=0;i<2&&err==ESP_OK;++i)err=backup_nvs_set_u8(h,s_simple_utility_keys[i],actions[i]);
    if(err==ESP_OK)err=backup_nvs_set_u8(h,"simple_labels",labels);
    if(err==ESP_OK)err=backup_nvs_set_u8(h,"simple_border",border);
    if(err==ESP_OK)err=settings_backup_commit(h);
    nvs_close(h);
    if(err==ESP_OK){memcpy(s_simple_utilities,actions,sizeof(s_simple_utilities));s_simple_labels=labels;s_simple_border=border;}
    return err;
}
uint8_t app_settings_home_utility(unsigned slot) {return slot<APP_HOME_UTILITY_SLOTS?s_home_utilities[slot]:APP_HOME_UTIL_NONE;}
bool app_settings_home_utility_labels(void){return s_home_utility_labels;}
bool app_settings_home_utility_border(void){return s_home_utility_border;}
esp_err_t app_settings_set_home_utilities(const uint8_t actions[APP_HOME_UTILITY_SLOTS]) {
    return app_settings_set_home_utilities_config(actions,s_home_utility_labels,s_home_utility_border);
}
esp_err_t app_settings_set_home_utilities_config(const uint8_t actions[APP_HOME_UTILITY_SLOTS],bool labels,bool border) {
    if(!actions)return ESP_ERR_INVALID_ARG;
    for(unsigned i=0;i<APP_HOME_UTILITY_SLOTS;++i)if(actions[i]>=APP_HOME_UTIL_COUNT)return ESP_ERR_INVALID_ARG;
    nvs_handle_t h;esp_err_t err=backup_nvs_open(NVS_NS,NVS_READWRITE,&h);if(err!=ESP_OK)return err;
    for(unsigned i=0;i<APP_HOME_UTILITY_SLOTS&&err==ESP_OK;++i)err=backup_nvs_set_u8(h,s_home_utility_keys[i],actions[i]);
    if(err==ESP_OK)err=backup_nvs_set_u8(h,"home_util_lbl",labels?1:0);
    if(err==ESP_OK)err=backup_nvs_set_u8(h,"home_util_bdr",border?1:0);
    if(err==ESP_OK)err=settings_backup_commit(h);
    nvs_close(h);
    if(err==ESP_OK){memcpy(s_home_utilities,actions,sizeof(s_home_utilities));s_home_utility_labels=labels;s_home_utility_border=border;}
    return err;
}
app_home_component_t app_settings_home_component(void) { return s_home_component; }
uint8_t app_settings_home_goal_minutes(void) { return s_home_goal_minutes; }
static esp_err_t home_preference_save(const char *key, uint8_t value) {
    nvs_handle_t h;
    esp_err_t err = backup_nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = backup_nvs_set_u8(h, key, value);
    if (err == ESP_OK) err = settings_backup_commit(h);
    nvs_close(h);
    return err;
}
esp_err_t app_settings_set_home_component(app_home_component_t component) {
    if ((unsigned)component >= APP_HOME_COMPONENT_COUNT || component == 3) return ESP_ERR_INVALID_ARG;
    esp_err_t err = home_preference_save(NVS_KEY_HOME_COMPONENT, component);
    if (err == ESP_OK) s_home_component = component;
    return err;
}
esp_err_t app_settings_set_home_goal_minutes(uint8_t minutes) {
    if (minutes < 5 || minutes > 180 || minutes % 5) return ESP_ERR_INVALID_ARG;
    esp_err_t err = home_preference_save(NVS_KEY_HOME_GOAL, minutes);
    if (err == ESP_OK) s_home_goal_minutes = minutes;
    return err;
}

#if !DPREADER_LOVE_BOOK
bool app_settings_simple_ui(void) { return s_simple_ui; }
bool app_settings_simple_full_ui(void) { return s_simple_ui; }
#endif
uint8_t app_settings_ui_mode(void) { return app_settings_simple_ui() ? 2 : 0; }
esp_err_t app_settings_set_ui_mode(uint8_t mode) {
#if DPREADER_LOVE_BOOK
    return mode==2 ? ESP_OK : ESP_ERR_NOT_SUPPORTED;
#else
    if(mode>2)return ESP_ERR_INVALID_ARG;
    if(mode==1)mode=2;
    nvs_handle_t h;
    esp_err_t err = nvs_open(SIMPLE_UI_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = nvs_set_u8(h, "mode", mode);
    if (err == ESP_OK) err = settings_backup_commit(h);
    nvs_close(h);
    if (err == ESP_OK) {s_simple_ui = mode!=0;s_simple_full_ui=mode==2;s_simple_choice_known=true;}
    return err;
#endif
}
esp_err_t app_settings_set_simple_ui(bool enabled) { return app_settings_set_ui_mode(enabled?2:0); }
bool app_settings_home_full_refresh(void) { return s_home_full_refresh; }
app_main_refresh_mode_t app_settings_main_refresh_mode(void) { return s_main_refresh; }
bool app_settings_main_fast_refresh(void) { return s_main_refresh == APP_MAIN_REFRESH_FAST; }
void app_settings_set_main_refresh_mode(app_main_refresh_mode_t mode) {
    if (mode < APP_MAIN_REFRESH_NORMAL || mode > APP_MAIN_REFRESH_WATER || mode == s_main_refresh) return;
    s_main_refresh = mode;
    nvs_put_u8(NVS_KEY_MAIN_REFRESH, (uint8_t)mode);
}
uint8_t app_settings_auto_lock_minutes(void) { return s_auto_lock_minutes; }
void app_settings_set_auto_lock_minutes(uint8_t minutes) {
    if (minutes != 0 && minutes != 1 && minutes != 5 && minutes != 10) return;
    if (s_auto_lock_minutes == minutes) return;
    s_auto_lock_minutes = minutes;
    nvs_put_u8(NVS_KEY_AUTO_LOCK, minutes);
}

uint8_t app_settings_wifi_timeout_minutes(void) { return s_wifi_timeout_minutes; }
void app_settings_set_wifi_timeout_minutes(uint8_t minutes) {
    if (minutes != 0 && minutes != 1 && minutes != 3 && minutes != 5 && minutes != 10) return;
    if (s_wifi_timeout_minutes == minutes) return;
    s_wifi_timeout_minutes = minutes;
    nvs_put_u8(NVS_KEY_WIFI_TIMEOUT, minutes);
}

uint8_t app_settings_ble_timeout_minutes(void) { return s_ble_timeout_minutes; }
void app_settings_set_ble_timeout_minutes(uint8_t minutes) {
    if (minutes != 0 && minutes != 5 && minutes != 10 && minutes != 15 && minutes != 30) return;
    if (s_ble_timeout_minutes == minutes) return;
    s_ble_timeout_minutes = minutes;
    nvs_put_u8(NVS_KEY_BLE_TIMEOUT, minutes);
}

void app_settings_set_home_full_refresh(bool enabled) {
    if (enabled == s_home_full_refresh) return;
    s_home_full_refresh = enabled;
    nvs_put_u8(NVS_KEY_HOME_FULL, enabled ? 1 : 0);
}

static bool nvs_put_str(const char *key, const char *value) {
    nvs_handle_t h;
    esp_err_t err = backup_nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) { ESP_LOGE(TAG, "save %s open: %s", key, esp_err_to_name(err)); return false; }
    err = backup_nvs_set_str(h, key, value);
    if (err == ESP_OK) err = settings_backup_commit(h);
    nvs_close(h);
    if (err != ESP_OK) ESP_LOGE(TAG, "save %s: %s", key, esp_err_to_name(err));
    return err == ESP_OK;
}

const char *app_settings_device_name(void) { return s_device_name; }
void app_settings_set_device_name(const char *name) {
    if (!name || !name[0] || strnlen(name, sizeof(s_device_name)) >= sizeof(s_device_name)) return;
    if (nvs_put_str(NVS_KEY_DEVICE_NAME, name)) strlcpy(s_device_name, name, sizeof(s_device_name));
}
#if !DPREADER_LOVE_BOOK
const char *app_settings_avatar_path(void) { return s_avatar; }
void app_settings_set_avatar_path(const char *path) {
    if (!path || (path[0] && strncmp(path, "/sdcard/", 8)) ||
        strnlen(path, sizeof(s_avatar)) >= sizeof(s_avatar)) return;
    if (nvs_put_str(NVS_KEY_AVATAR, path)) strlcpy(s_avatar, path, sizeof(s_avatar));
}
#endif
const char *app_settings_status_signature(void) { return s_status_signature; }
void app_settings_set_status_signature(const char *signature) {
    if (!signature || strnlen(signature, sizeof(s_status_signature)) >= sizeof(s_status_signature)) return;
    if (nvs_put_str(NVS_KEY_STATUS_SIGNATURE, signature))
        strlcpy(s_status_signature, signature, sizeof(s_status_signature));
}


#if FONT_FLASH_FALLBACK
static bool save_font_path(const char *key, const font_settings_files_t *files, const char *path) {
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err == ESP_OK) {
        err = nvs_set_str(h, key, path);
        if (err == ESP_OK) err = nvs_commit(h);
        nvs_close(h);
    }
    if (err == ESP_OK) {
        if (book_store_flash_ready() && !font_settings_clear(files)) err = ESP_FAIL;
    } else if (err == ESP_ERR_NVS_NOT_ENOUGH_SPACE && book_store_mount_flash() == ESP_OK) {
        char scratch[FONT_PATH_MAX];
        if (font_settings_write(files, path, scratch, sizeof(scratch))) {
            ESP_LOGI(TAG, "saved %s in internal storage (NVS out of space)", key);
            err = ESP_OK;
        }
    }
#if CONFIG_DPREADER_AUTO_BACKUP
    if (err == ESP_OK) ++s_auto_backup.revision;
    else s_auto_backup.failed = true;
#endif
    if (err != ESP_OK) ESP_LOGE(TAG, "save %s: %s", key, esp_err_to_name(err));
    return err == ESP_OK;
}
#endif

const char* app_settings_font_path(void) {
    return s_font;
}

const char* app_settings_system_font_path(void) { return s_system_font; }
void app_settings_set_system_font_path(const char* path) {
    if(!path)path="";
    if(strnlen(path,sizeof(s_system_font))>=sizeof(s_system_font))return;
#if FONT_FLASH_FALLBACK
    if(save_font_path(NVS_KEY_SYS_FONT, &system_font_files, path))
#else
    if(nvs_put_str(NVS_KEY_SYS_FONT,path))
#endif
        strlcpy(s_system_font,path,sizeof(s_system_font));
}

uint8_t app_settings_system_font_size(void) { return s_system_size; }
void app_settings_set_system_font_size(uint8_t percent) {
    if (percent < 100 || percent > 202 || (percent % 10 && percent != 144 && percent != 202) || percent == s_system_size) return;
    nvs_handle_t h;
    if(backup_nvs_open(NVS_NS,NVS_READWRITE,&h)!=ESP_OK)return;
    esp_err_t err=backup_nvs_set_u8(h,NVS_KEY_SYS_SIZE,percent);
    if(err==ESP_OK)err=backup_nvs_set_u8(h,"sys_size_rev",2);
    if(err==ESP_OK)err=settings_backup_commit(h);
    nvs_close(h);
    if(err==ESP_OK)s_system_size=percent;
}
bool app_settings_screen_inverted(void) { return s_screen_inverted; }
bool app_settings_set_screen_inverted(bool enabled) {
    if(enabled==s_screen_inverted)return true;
    nvs_handle_t h;
    if(backup_nvs_open(NVS_NS,NVS_READWRITE,&h)!=ESP_OK)return false;
    esp_err_t err=backup_nvs_set_u8(h,NVS_KEY_INVERT,enabled?1:0);
    if(err==ESP_OK)err=settings_backup_commit(h);
    nvs_close(h);
    if(err==ESP_OK)s_screen_inverted=enabled;
    return err==ESP_OK;
}
uint8_t app_settings_system_contrast(void) { return s_system_contrast; }
void app_settings_set_system_contrast(uint8_t percent) {
    if (percent < 100 || percent > 140 || percent % 10 || percent == s_system_contrast) return;
    s_system_contrast = percent;
    nvs_put_u8(NVS_KEY_SYS_CONTRAST, percent);
}
uint8_t app_settings_lock_style(void) { return DPREADER_LOVE_BOOK && s_lock_style==2 ? 0 : s_lock_style; }
void app_settings_set_lock_style(uint8_t style) {
    if (style > 3 || style == s_lock_style) return;
    s_lock_style = style;
    nvs_put_u8(NVS_KEY_LOCK_STYLE, style);
}
const char* app_settings_bookmark_path(void) { return s_bookmark; }
void app_settings_set_bookmark_path(const char *path) {
    const char *ext=path?strrchr(path,'.'):NULL;
    if(!path || (path[0] && (strncmp(path,"/sdcard/",8)||!ext||strcasecmp(ext,".png"))) ||
        strnlen(path,sizeof(s_bookmark))>=sizeof(s_bookmark)) return;
    nvs_handle_t h;
    if(backup_nvs_open(NVS_NS,NVS_READWRITE,&h)!=ESP_OK)return;
    esp_err_t err=backup_nvs_set_str(h,NVS_KEY_BOOKMARK,path);
    if(err==ESP_OK)err=settings_backup_commit(h);
    nvs_close(h);
    if(err==ESP_OK)strlcpy(s_bookmark,path,sizeof(s_bookmark));
}
const char* app_settings_wallpaper_path(void) { return s_wallpaper; }
void app_settings_set_wallpaper_path(const char* path) {
    if (!path || (path[0] && strncmp(path, "/sdcard/", 8)) ||
        strnlen(path, sizeof(s_wallpaper)) >= sizeof(s_wallpaper)) return;
    strlcpy(s_wallpaper, path, sizeof(s_wallpaper));
    nvs_handle_t h;
    if (backup_nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return;
    backup_nvs_set_str(h, NVS_KEY_WALLPAPER, s_wallpaper);
    settings_backup_commit(h);
    nvs_close(h);
}

static void nvs_put_u8(const char* key, uint8_t value) {
    nvs_handle_t h;
    if (backup_nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return;
    backup_nvs_set_u8(h, key, value);
    settings_backup_commit(h);
    nvs_close(h);
}

uint8_t app_settings_last_wake(void) {
    return s_last_wake;
}

void app_settings_set_last_wake(uint8_t src) {
    if (src == s_last_wake) return;
    s_last_wake = src;
    nvs_put_u8(NVS_KEY_WAKE, src);
}

uint8_t app_settings_last_boot(void) {
    return s_last_boot;
}

void app_settings_set_last_boot(uint8_t reason) {
    if (reason == 0 || reason == s_last_boot) return;
    s_last_boot = reason;
    nvs_put_u8(NVS_KEY_BOOT, reason);
}

bool app_settings_pickup_wake(void) {
    return s_pickup_wake;
}

void app_settings_set_pickup_wake(bool on) {
    if (s_pickup_wake == on) return;
    s_pickup_wake = on;
    nvs_put_u8(NVS_KEY_PICKUP, on ? 1 : 0);
}

void app_settings_set_font_path(const char* path) {
    if(!path)path="";
    if(strnlen(path,sizeof(s_font))>=sizeof(s_font))return;
#if FONT_FLASH_FALLBACK
    if(save_font_path(NVS_KEY_FONT, &reader_font_files, path))
#else
    if(nvs_put_str(NVS_KEY_FONT,path))
#endif
        strlcpy(s_font,path,sizeof(s_font));
}

int8_t app_settings_book_weight(void) { return (int8_t)s_book_weight - 2; }
bool app_settings_book_antialias(void) { return s_book_antialias; }
bool app_settings_set_book_antialias(bool enabled) {
    if (enabled == s_book_antialias) return true;
    nvs_handle_t h;
    if (backup_nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return false;
    esp_err_t err = backup_nvs_set_u8(h, NVS_KEY_BOOK_AA, enabled ? 1 : 0);
    if (err == ESP_OK) err = settings_backup_commit(h);
    nvs_close(h);
    if (err == ESP_OK) s_book_antialias = enabled;
    return err == ESP_OK;
}
bool app_settings_book_text_contrast(void) { return s_book_text_contrast; }
bool app_settings_set_book_text_contrast(bool enabled) {
    if (enabled == s_book_text_contrast) return true;
    nvs_handle_t h;
    if (backup_nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return false;
    esp_err_t err = backup_nvs_set_u8(h, NVS_KEY_BOOK_TEXT_CONTRAST, enabled);
    if (err == ESP_OK) err = settings_backup_commit(h);
    nvs_close(h);
    if (err == ESP_OK) s_book_text_contrast = enabled;
    return err == ESP_OK;
}
bool app_settings_book_drop_cap(void) { return s_book_drop_cap; }
bool app_settings_set_book_drop_cap(bool enabled) {
    if (enabled == s_book_drop_cap) return true;
    nvs_handle_t h;
    if (backup_nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return false;
    esp_err_t err = backup_nvs_set_u8(h, NVS_KEY_BOOK_DROP_CAP, enabled);
    if (err == ESP_OK) err = settings_backup_commit(h);
    nvs_close(h);
    if (err == ESP_OK) s_book_drop_cap = enabled;
    return err == ESP_OK;
}

int8_t app_settings_book_weight_fine(void) {return s_book_weight_fine;}
bool app_settings_set_book_weight_fine(int8_t steps) {
    if(steps < -APP_BOOK_WEIGHT_FINE_MAX || steps > APP_BOOK_WEIGHT_FINE_MAX)return false;
    if(steps==s_book_weight_fine)return true;
    uint8_t legacy=(uint8_t)(steps/APP_BOOK_WEIGHT_SUBSTEPS+2);
    nvs_handle_t h;
    if(backup_nvs_open(NVS_NS,NVS_READWRITE,&h)!=ESP_OK)return false;
    esp_err_t err=backup_nvs_set_u8(h,NVS_KEY_BOOK_WEIGHT_FINE,steps+APP_BOOK_WEIGHT_FINE_MAX);
    if(err==ESP_OK)err=backup_nvs_set_u8(h,NVS_KEY_BOOK_WEIGHT,legacy);
    if(err==ESP_OK)err=settings_backup_commit(h);
    nvs_close(h);
    if(err==ESP_OK){s_book_weight=legacy;s_book_weight_fine=steps;}
    return err==ESP_OK;
}
bool app_settings_set_book_weight(int8_t level) {
    return level>=-2&&level<=2&&app_settings_set_book_weight_fine(level*APP_BOOK_WEIGHT_SUBSTEPS);
}

uint8_t app_settings_book_px(void) {
    return s_book_px;
}

void app_settings_set_book_px(uint8_t px) {
    px = valid_book_px(px);
    if (s_book_px == px) return;
    s_book_px = px;
    nvs_put_u8(NVS_KEY_BOOK_PX, px);
}

const char *app_settings_book_background_path(void) { return s_book_background; }
bool app_settings_set_book_background_path(const char *path) {
    if (!path || (path[0] && strncmp(path, "/sdcard/pictures/", 17)) ||
        strnlen(path, sizeof(s_book_background)) >= sizeof(s_book_background) || strstr(path, "/../")) return false;
    nvs_handle_t h;
    if (backup_nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return false;
    esp_err_t err = backup_nvs_set_str(h, NVS_KEY_BOOK_BACKGROUND, path);
    if (err == ESP_OK) err = settings_backup_commit(h);
    nvs_close(h);
    if (err != ESP_OK) return false;
    strlcpy(s_book_background, path, sizeof(s_book_background));
    return true;
}

uint8_t app_settings_reader_full_pages(void) { return s_reader_full_pages; }
void app_settings_set_reader_full_pages(uint8_t pages) {
    if (!reader_full_pages_valid(pages) || pages == s_reader_full_pages) return;
    s_reader_full_pages = pages;
    nvs_put_u8(NVS_KEY_READER_FULL, pages);
}
uint8_t app_settings_reader_turn_effect(void) { return s_reader_turn_effect; }
void app_settings_set_reader_turn_effect(uint8_t effect) {
    if (effect > READER_TURN_EFFECT_MAX || effect == s_reader_turn_effect) return;
    s_reader_turn_effect = effect;
    nvs_put_u8(NVS_KEY_READER_TURN, effect);
}
bool app_settings_reader_power_turn(void) { return s_reader_power_turn; }
void app_settings_set_reader_power_turn(bool on) {
    if (s_reader_power_turn == on) return;
    s_reader_power_turn = on;
    nvs_put_u8(NVS_KEY_POWER_TURN, on ? 1 : 0);
}
bool app_settings_reader_immersive(void) { return s_reader_immersive; }
void app_settings_set_reader_immersive(bool on) {
    if (s_reader_immersive == on) return;
    s_reader_immersive = on;
    nvs_put_u8(NVS_KEY_IMMERSIVE, on ? 1 : 0);
}
bool app_settings_reader_hide_images(void) { return s_reader_hide_images; }
void app_settings_set_reader_hide_images(bool on) {
    if (s_reader_hide_images == on) return;
    s_reader_hide_images = on;
    nvs_put_u8(NVS_KEY_HIDE_IMAGES, on ? 1 : 0);
}
bool app_settings_book_fonts(void) { return s_book_fonts; }
void app_settings_set_book_fonts(bool on) {
    if (s_book_fonts == on) return;
    s_book_fonts = on;
    nvs_put_u8(NVS_KEY_BOOK_FONTS, on ? 1 : 0);
}
bool app_settings_book_vertical(void) { return s_book_vertical; }
void app_settings_set_book_vertical(bool on) {
    if (s_book_vertical == on) return;
    s_book_vertical = on;
    nvs_put_u8(NVS_KEY_BOOK_VERTICAL, on ? 1 : 0);
}
#if DPREADER_BOARD_METALIO47
uint32_t app_settings_action_menu(void){return s_action_menu;}
esp_err_t app_settings_set_action_menu(uint32_t mask){
    if(mask&~APP_ACTION_MENU_ALLOWED)return ESP_ERR_INVALID_ARG;
    if(mask==s_action_menu)return ESP_OK;
    nvs_handle_t h;esp_err_t err=backup_nvs_open(NVS_NS,NVS_READWRITE,&h);
    if(err!=ESP_OK)return err;
    err=backup_nvs_set_u32(h,"action_menu",mask);if(err==ESP_OK)err=settings_backup_commit(h);
    nvs_close(h);
    if(err==ESP_OK)s_action_menu=mask;
    return err;
}
bool app_settings_power_action_menu(void){return s_power_action_menu;}
esp_err_t app_settings_set_power_action_menu(bool enabled){
    if(enabled==s_power_action_menu)return ESP_OK;
    nvs_handle_t h;esp_err_t err=backup_nvs_open(NVS_NS,NVS_READWRITE,&h);
    if(err!=ESP_OK)return err;
    err=backup_nvs_set_u8(h,"power_menu",enabled);if(err==ESP_OK)err=settings_backup_commit(h);
    nvs_close(h);
    if(err==ESP_OK)s_power_action_menu=enabled;
    return err;
}
app_ai_action_t app_settings_ai_short(void){return s_ai_short;}
esp_err_t app_settings_set_ai_short(app_ai_action_t action){
    if((unsigned)action>=APP_AI_ACTION_COUNT)return ESP_ERR_INVALID_ARG;
    if(action==s_ai_short)return ESP_OK;
    nvs_handle_t h;esp_err_t err=backup_nvs_open(NVS_NS,NVS_READWRITE,&h);
    if(err!=ESP_OK)return err;
    err=backup_nvs_set_u8(h,"ai_short",(uint8_t)action);
    if(err==ESP_OK)err=settings_backup_commit(h);
    nvs_close(h);if(err==ESP_OK)s_ai_short=action;return err;
}
#endif
bool app_settings_ble_turner(void) { return s_ble_turner; }
app_reader_key_action_t app_settings_reader_key_action(unsigned key) {
    if (key >= 3) return APP_READER_KEY_NONE;
    return s_reader_keys[key] == APP_READER_KEY_FULLSCREEN ? APP_READER_KEY_TOOLS : (app_reader_key_action_t)s_reader_keys[key];
}
bool app_settings_reader_key_action_allowed(unsigned key, app_reader_key_action_t action) {
    return key < 4 && action >= 0 && action < APP_READER_KEY_COUNT && action != APP_READER_KEY_FULLSCREEN;
}
bool app_settings_set_reader_key_action(unsigned key, app_reader_key_action_t action) {
    if (key >= 3 || !app_settings_reader_key_action_allowed(key, action)) return false;
    if (s_reader_keys[key] == action) return true;
    nvs_handle_t h;
    if (backup_nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return false;
    esp_err_t err = backup_nvs_set_u8(h, s_reader_key_names[key], action);
    if (err == ESP_OK) err = settings_backup_commit(h);
    nvs_close(h);
    if (err == ESP_OK) s_reader_keys[key] = action;
    return err == ESP_OK;
}
app_reader_key_action_t app_settings_reader_hold_action(void) {
    return s_reader_hold_action == APP_READER_KEY_FULLSCREEN ? APP_READER_KEY_TOOLS : (app_reader_key_action_t)s_reader_hold_action;
}
bool app_settings_set_reader_hold_action(app_reader_key_action_t action) {
    if (!app_settings_reader_key_action_allowed(3, action)) return false;
    if (s_reader_hold_action == action) return true;
    nvs_handle_t h;
    if (backup_nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return false;
    esp_err_t err = backup_nvs_set_u8(h, NVS_KEY_HOLD_ACTION, action);
    if (err == ESP_OK) err = settings_backup_commit(h);
    nvs_close(h);
    if (err == ESP_OK) s_reader_hold_action = action;
    return err == ESP_OK;
}
void app_settings_set_reader_key_preset(unsigned preset) {
    static const uint8_t values[2][3] = {
        {APP_READER_KEY_PREV, APP_READER_KEY_TOOLS, APP_READER_KEY_NEXT},
        {APP_READER_KEY_HOME, APP_READER_KEY_REFRESH, APP_READER_KEY_TOOLS}
    };
    if (preset < 1 || preset > 2) return;
    nvs_handle_t h;
    esp_err_t err = backup_nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return;
    for (unsigned i = 0; i < 3 && err == ESP_OK; ++i) err = backup_nvs_set_u8(h, s_reader_key_names[i], values[preset - 1][i]);
    if (err == ESP_OK) err = backup_nvs_set_u8(h, NVS_KEY_HOLD_ACTION, preset == 1 ? APP_READER_KEY_HOME : APP_READER_KEY_REFRESH);
    if (err == ESP_OK) err = settings_backup_commit(h);
    nvs_close(h);
    if (err == ESP_OK) {
        memcpy(s_reader_keys, values[preset - 1], sizeof(s_reader_keys));
        s_reader_hold_action = preset == 1 ? APP_READER_KEY_HOME : APP_READER_KEY_REFRESH;
    }
}

bool app_settings_reader_hold_refresh(void) { return s_reader_hold_refresh; }
app_reader_key_action_t app_settings_volume_key_action(unsigned key) {
    if (key >= 2) return APP_READER_KEY_NONE;
    return s_volume_keys[key] == APP_READER_KEY_FULLSCREEN ? APP_READER_KEY_TOOLS : (app_reader_key_action_t)s_volume_keys[key];
}
bool app_settings_set_volume_key_action(unsigned key, app_reader_key_action_t action) {
    if (key >= 2 || !app_settings_reader_key_action_allowed(key, action)) return false;
    if (s_volume_keys[key] == action) return true;
    nvs_handle_t h;
    if (backup_nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return false;
    esp_err_t err = backup_nvs_set_u8(h, key == 0 ? "vol_down" : "vol_up", (uint8_t)action);
    if (err == ESP_OK) err = settings_backup_commit(h);
    nvs_close(h);
    if (err == ESP_OK) s_volume_keys[key] = action;
    return err == ESP_OK;
}
uint8_t app_settings_control_hold(unsigned key) {
#if DPREADER_BOARD_METALIO47
    if(key>=7)return APP_CONTROL_HOLD_DEFAULT;
    return key==4?(uint8_t)app_settings_reader_hold_action():s_control_holds[key];
#else
    (void)key;return APP_CONTROL_HOLD_DEFAULT;
#endif
}
bool app_settings_set_control_hold(unsigned key,uint8_t action) {
#if DPREADER_BOARD_METALIO47
    if(key>=7 || (action!=APP_CONTROL_HOLD_DEFAULT && !app_settings_reader_key_action_allowed(3,action)))return false;
    if(key==4)return app_settings_set_reader_hold_action(action==APP_CONTROL_HOLD_DEFAULT?APP_READER_KEY_HOME:action);
    if(s_control_holds[key]==action)return true;
    nvs_handle_t h;
    if(backup_nvs_open(NVS_NS,NVS_READWRITE,&h)!=ESP_OK)return false;
    esp_err_t err=backup_nvs_set_u8(h,s_control_hold_names[key],action);
    if(err==ESP_OK)err=settings_backup_commit(h);
    nvs_close(h);if(err==ESP_OK)s_control_holds[key]=action;
    return err==ESP_OK;
#else
    (void)key;(void)action;return false;
#endif
}
bool app_settings_reader_swipe_up_tools(void) { return s_reader_swipe_up_tools; }
void app_settings_set_reader_swipe_up_tools(bool on) {
    if (s_reader_swipe_up_tools == on) return;
    s_reader_swipe_up_tools = on;
    nvs_put_u8("r_swipe_up", on ? 1 : 0);
}
bool app_settings_reader_swipe_bottom_home(void) { return s_reader_swipe_bottom_home; }
void app_settings_set_reader_swipe_bottom_home(bool on) {
    if (s_reader_swipe_bottom_home == on) return;
    s_reader_swipe_bottom_home = on;
    nvs_put_u8("r_swipe_bot", on ? 1 : 0);
}
uint8_t app_settings_reader_turn_mode(void) { return s_reader_turn_mode; }
void app_settings_set_reader_turn_mode(uint8_t mode) {
    if (s_reader_turn_mode == mode) return;
    s_reader_turn_mode = mode;
    nvs_put_u8(NVS_KEY_VERTICAL_TURN, mode);
}
bool app_settings_reader_vertical_turn(void) { return s_reader_turn_mode == 1; }
void app_settings_set_reader_vertical_turn(bool on) {
    app_settings_set_reader_turn_mode(on ? 1 : 0);
}
void app_settings_set_reader_hold_refresh(bool on) {
    if (s_reader_hold_refresh == on) return;
    s_reader_hold_refresh = on;
    nvs_put_u8(NVS_KEY_HOLD_REFRESH, on ? 1 : 0);
}
void app_settings_set_ble_turner(bool on) {
    if (s_ble_turner == on) return;
    s_ble_turner = on;
    nvs_put_u8(NVS_KEY_BLE_TURNER, on ? 1 : 0);
}
bool app_settings_shelf_recent_sort(void) { return s_shelf_recent_sort; }
void app_settings_set_shelf_recent_sort(bool on) {
    if (s_shelf_recent_sort == on) return;
    s_shelf_recent_sort = on;
    nvs_put_u8(NVS_KEY_SHELF_RECENT, on ? 1 : 0);
}
int8_t app_settings_book_word_spacing(void){return s_book_word_spacing;}
bool app_settings_set_book_word_spacing(int8_t px) {
    if(px < -10 || px>20)return false;
    if(px==s_book_word_spacing)return true;
    nvs_handle_t h;if(backup_nvs_open(NVS_NS,NVS_READWRITE,&h)!=ESP_OK)return false;
    esp_err_t err=backup_nvs_set_u8(h,"book_word_gap",px);
    if(err==ESP_OK)err=settings_backup_commit(h);
    nvs_close(h);
    if(err!=ESP_OK)return false;
    s_book_word_spacing=px;return true;
}
uint8_t app_settings_book_tracking(void) { return s_book_tracking; }
void app_settings_set_book_tracking(uint8_t index) {
    if (index >= APP_BOOK_TRACK_COUNT || index == s_book_tracking) return;
    s_book_tracking = index;
    nvs_put_u8(NVS_KEY_BOOK_TRACK, 192+index);
}
uint8_t app_settings_book_indent(void) { return s_book_indent; }
void app_settings_set_book_indent(uint8_t em) {
    if (em > 3 || em == s_book_indent) return;
    s_book_indent = em;
    nvs_put_u8(NVS_KEY_BOOK_INDENT, em);
}
int8_t app_settings_book_indent_adjust(void) { return s_book_indent_adjust <= 120 ? (int8_t)((int)s_book_indent_adjust - 20) : (int8_t)(100 - (int)s_book_indent_adjust); }
void app_settings_set_book_indent_adjust(int8_t px) {
    if (px < -100 || px > 100 || px == app_settings_book_indent_adjust()) return;
    // 0..40保留旧配置语义，扩展负值使用121..200。/ Preserve legacy 0..40 encoding; extended negatives use 121..200.
    s_book_indent_adjust = (uint8_t)(px >= -20 ? px + 20 : 100 - px);
    nvs_put_u8(NVS_KEY_BOOK_INDENT_ADJUST, s_book_indent_adjust);
}
uint8_t app_settings_book_reading_line(void) { return s_book_reading_line; }
void app_settings_set_book_reading_line(uint8_t style) {
    if (style > 2 || style == s_book_reading_line) return;
    s_book_reading_line = style;
    nvs_put_u8(NVS_KEY_BOOK_RULE, style);
}
uint8_t app_settings_book_align(void) { return s_book_align; }
void app_settings_set_book_align(uint8_t align) {
    if (align >= APP_BOOK_ALIGN_COUNT || align == s_book_align) return;
    s_book_align = align;
    nvs_put_u8(NVS_KEY_BOOK_ALIGN, align);
}
uint8_t app_settings_book_progress_style(void) { return s_book_progress_style; }
void app_settings_set_book_progress_style(uint8_t style) {
    if (style >= APP_BOOK_PROGRESS_STYLE_COUNT || style == s_book_progress_style) return;
    s_book_progress_style = style;
    nvs_put_u8(NVS_KEY_BOOK_PROGRESS_V2, style);
}
bool app_settings_book_footer_page(void) { return s_book_footer_page; }
void app_settings_set_book_footer_page(bool on) {
    if (s_book_footer_page == on) return;
    s_book_footer_page = on;
    nvs_put_u8(NVS_KEY_BOOK_FOOTER_PAGE, on ? 1 : 0);
}
bool app_settings_book_footer_chapter(void) { return s_book_footer_chapter; }
void app_settings_set_book_footer_chapter(bool on) {
    if (s_book_footer_chapter == on) return;
    s_book_footer_chapter = on;
    nvs_put_u8(NVS_KEY_BOOK_FOOTER_CHAPTER, on ? 1 : 0);
}
bool app_settings_book_footer_pct(void) { return s_book_footer_pct; }
void app_settings_set_book_footer_pct(bool on) {
    if (s_book_footer_pct == on) return;
    s_book_footer_pct = on;
    nvs_put_u8(NVS_KEY_BOOK_FOOTER_PCT, on ? 1 : 0);
}
int8_t app_settings_book_reading_line_offset(void) { return (int8_t)((int)s_book_rule_offset - 4) * 2; }
void app_settings_set_book_reading_line_offset(int8_t offset_px) {
    if (offset_px < -8 || offset_px > 8 || offset_px % 2) return;
    uint8_t index = (uint8_t)(offset_px / 2 + 4);
    if (index == s_book_rule_offset) return;
    s_book_rule_offset = index;
    nvs_put_u8(NVS_KEY_BOOK_RULE_OFFSET, index);
}
uint8_t app_settings_book_line_spacing(void) {
    // Map old 100/110/120/130% pitches to the signed adjustment at 40 px.
    if (s_book_line >= 100 && s_book_line <= 130) return (uint8_t)(s_book_line - 130);
    return s_book_line;
}
void app_settings_set_book_line_spacing(uint8_t percent) {
    if (!book_line_adjustment_valid(percent)) return;
    if (s_book_line == percent) return;
    s_book_line = percent;
    nvs_put_u8(NVS_KEY_BOOK_LINE, percent);
}
uint8_t app_settings_book_margin(void) { return s_book_margin; }
void app_settings_set_book_margin(uint8_t px) {
    if (px > 48 || s_book_margin == px) return;
    s_book_margin = px;
    nvs_put_u8(NVS_KEY_BOOK_MARGIN, px);
}
// 非线性段距保留旧档位，并将25%置于几何中点。/ Nonlinear gaps retain legacy stops with 25% at the geometric midpoint.
uint8_t app_settings_book_paragraph_value(uint8_t level) {
    static const uint8_t values[APP_BOOK_PARAGRAPH_COUNT]={186,196,206,216,226,236,(uint8_t)-10,(uint8_t)-5,0,5,10,15,20,25,30,40,50,75};
    return values[level<APP_BOOK_PARAGRAPH_COUNT?level:APP_BOOK_PARAGRAPH_COUNT-1];
}
uint8_t app_settings_book_paragraph_level(uint8_t percent) {
    uint8_t best=0;
    unsigned distance=255;
    for(uint8_t i=0;i<APP_BOOK_PARAGRAPH_COUNT;++i) {
        int delta=(int)(int8_t)app_settings_book_paragraph_value(i)-(int)(int8_t)percent;
        unsigned d=(unsigned)(delta<0?-delta:delta);
        if(d<distance){distance=d;best=i;}
    }
    return best;
}
uint8_t app_settings_book_paragraph_spacing(void) { return s_book_para; }
void app_settings_set_book_paragraph_spacing(uint8_t percent) {
    if (app_settings_book_paragraph_value(app_settings_book_paragraph_level(percent)) != percent) return;
    if (s_book_para == percent) return;
    s_book_para = percent;
    nvs_put_u8(NVS_KEY_BOOK_PARA, percent);
}
uint8_t app_settings_shelf_style(void) { return s_shelf_style; }
void app_settings_set_shelf_style(uint8_t style) {
    if (style < 1 || style > 5) return;
    if (style == 3 || style == 4) style = 2;
    if (s_shelf_style == style) return;
    s_shelf_style = style;
    nvs_put_u8(NVS_KEY_SHELF_STYLE, style);
}
const char* app_settings_books_dir(void) { return s_books_dir; }
const char* app_settings_fonts_dir(void) { return s_fonts_dir; }
bool app_settings_set_books_dir(const char* path) { return set_media_dir(s_books_dir,NVS_KEY_BOOKS_DIR,path); }
bool app_settings_set_fonts_dir(const char* path) { return set_media_dir(s_fonts_dir,NVS_KEY_FONTS_DIR,path); }

const char *app_settings_kosync_url(void) { return s_kosync_url; }
bool app_settings_set_kosync_url(const char *url) {
    char normalized[sizeof(s_kosync_url)];
    if (!url || !url[0]) {
        strlcpy(normalized, DPREADER_DEFAULT_KOSYNC_URL, sizeof(normalized));
    } else if (strncmp(url, "http://", 7) && strncmp(url, "https://", 8)) {
        if (strnlen(url, sizeof(normalized)) + 8 >= sizeof(normalized)) return false;
        strlcpy(normalized, "https://", sizeof(normalized));
        strlcat(normalized, url, sizeof(normalized));
    } else {
        if (strnlen(url, sizeof(normalized)) >= sizeof(normalized)) return false;
        strlcpy(normalized, url, sizeof(normalized));
    }
    if (!nvs_put_str(NVS_KEY_KOSYNC_URL, normalized)) return false;
    strlcpy(s_kosync_url, normalized, sizeof(s_kosync_url));
    return true;
}
const char *app_settings_kosync_user(void) { return s_kosync_user; }
bool app_settings_set_kosync_user(const char *user) {
    if (!user) user = "";
    if (strnlen(user, sizeof(s_kosync_user)) >= sizeof(s_kosync_user)) return false;
    if (!nvs_put_str(NVS_KEY_KOSYNC_USER, user)) return false;
    strlcpy(s_kosync_user, user, sizeof(s_kosync_user));
    return true;
}
const char *app_settings_kosync_key(void) { return s_kosync_key; }
bool app_settings_set_kosync_key(const char *key) {
    if (!key) key = "";
    if (strnlen(key, sizeof(s_kosync_key)) >= sizeof(s_kosync_key)) return false;
    if (!nvs_put_str(NVS_KEY_KOSYNC_KEY, key)) return false;
    strlcpy(s_kosync_key, key, sizeof(s_kosync_key));
    return true;
}
uint8_t app_settings_ota_edition(void) { return s_ota_edition; }
esp_err_t app_settings_set_ota_edition(uint8_t edition) {
    if (edition > 1) return ESP_ERR_INVALID_ARG;
    nvs_handle_t h;
    esp_err_t err = backup_nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = backup_nvs_set_u8(h, "ota_edition", edition);
    if (err == ESP_OK) err = settings_backup_commit(h);
    nvs_close(h);
    if (err == ESP_OK) s_ota_edition = edition;
    return err;
}

bool app_settings_kosync_auto(void) { return s_kosync_auto; }
void app_settings_set_kosync_auto(bool on) {
    if (s_kosync_auto == on) return;
    s_kosync_auto = on;
    nvs_put_u8(NVS_KEY_KOSYNC_AUTO, on ? 1 : 0);
}

const char *app_settings_opds_url(void) { return s_opds_url; }
bool app_settings_set_opds_url(const char *url) {
    char normalized[sizeof(s_opds_url)];
    if (!url || !url[0]) {
        strlcpy(normalized, DPREADER_DEFAULT_OPDS_URL, sizeof(normalized));
    } else if (strncmp(url, "http://", 7) && strncmp(url, "https://", 8)) {
        if (strnlen(url, sizeof(normalized)) + 8 >= sizeof(normalized)) return false;
        strlcpy(normalized, "https://", sizeof(normalized));
        strlcat(normalized, url, sizeof(normalized));
    } else {
        if (strnlen(url, sizeof(normalized)) >= sizeof(normalized)) return false;
        strlcpy(normalized, url, sizeof(normalized));
    }
    if (!nvs_put_str(NVS_KEY_OPDS_URL, normalized)) return false;
    strlcpy(s_opds_url, normalized, sizeof(s_opds_url));
    return true;
}
const char *app_settings_opds_user(void) { return s_opds_user; }
bool app_settings_set_opds_user(const char *user) {
    if (!user) user = "";
    if (strnlen(user, sizeof(s_opds_user)) >= sizeof(s_opds_user)) return false;
    if (!nvs_put_str(NVS_KEY_OPDS_USER, user)) return false;
    strlcpy(s_opds_user, user, sizeof(s_opds_user));
    return true;
}
const char *app_settings_opds_pass(void) { return s_opds_pass; }
bool app_settings_set_opds_pass(const char *pass) {
    if (!pass) pass = "";
    if (strnlen(pass, sizeof(s_opds_pass)) >= sizeof(s_opds_pass)) return false;
    if (!nvs_put_str(NVS_KEY_OPDS_PASS, pass)) return false;
    strlcpy(s_opds_pass, pass, sizeof(s_opds_pass));
    return true;
}

#ifndef APP_SETTINGS_BACKUP_ROOT
#define APP_SETTINGS_BACKUP_ROOT "/sdcard"
#endif
#if DPREADER_BOARD_METALIO47
#define BACKUP_FILE APP_SETTINGS_BACKUP_ROOT "/dpReader-settings.backup"
#define BACKUP_TEMP APP_SETTINGS_BACKUP_ROOT "/dpReader-settings.backup.tmp"
#define BACKUP_PREVIOUS APP_SETTINGS_BACKUP_ROOT "/dpReader-settings.backup.previous"
#else
#define BACKUP_FILE APP_SETTINGS_BACKUP_ROOT "/dpReader-settings.backup"
#define BACKUP_TEMP APP_SETTINGS_BACKUP_ROOT "/dpReader-settings.backup.tmp"
#define BACKUP_PREVIOUS APP_SETTINGS_BACKUP_ROOT "/dpReader-settings.backup.previous"
#endif

// All fields are bytes, so the v1 disk layout is independent of structure padding.
// 所有字段均为字节，v1 磁盘格式不依赖编译器的结构体填充。
typedef struct {
    char magic[8];
    uint8_t flags[17];
    char font[FONT_PATH_MAX];
    char system_font[FONT_PATH_MAX];
    char wallpaper[288];
    char books_dir[MEDIA_DIR_MAX];
    char fonts_dir[MEDIA_DIR_MAX];
    uint8_t checksum[4];
} settings_backup_v1_t;

typedef struct {
    char device_name[64];
    char avatar[288];
    char status_signature[96];
    uint8_t home_full_refresh;
    uint8_t checksum[4];
} settings_backup_profile_t;

// V31 appends account credentials and the footer master switch before reading history.
typedef struct {
    char kosync_url[128], kosync_user[64], kosync_key[36];
    char opds_url[160], opds_user[64], opds_pass[64];
    uint8_t kosync_auto, footer_enabled;
    uint8_t checksum[4];
} settings_backup_accounts_t;
typedef struct { const char *key; char *state; size_t offset, capacity; } account_backup_field_t;
#define ACCOUNT_FIELD(member, key, state) {key, state, offsetof(settings_backup_accounts_t, member), sizeof(((settings_backup_accounts_t *)0)->member)}
static const account_backup_field_t s_account_backup_fields[] = {
    ACCOUNT_FIELD(kosync_url, NVS_KEY_KOSYNC_URL, s_kosync_url),
    ACCOUNT_FIELD(kosync_user, NVS_KEY_KOSYNC_USER, s_kosync_user),
    ACCOUNT_FIELD(kosync_key, NVS_KEY_KOSYNC_KEY, s_kosync_key),
    ACCOUNT_FIELD(opds_url, NVS_KEY_OPDS_URL, s_opds_url),
    ACCOUNT_FIELD(opds_user, NVS_KEY_OPDS_USER, s_opds_user),
    ACCOUNT_FIELD(opds_pass, NVS_KEY_OPDS_PASS, s_opds_pass),
};
#undef ACCOUNT_FIELD
static uint32_t backup_accounts_checksum(const settings_backup_accounts_t *accounts) {
    const uint8_t *bytes = (const uint8_t *)accounts;
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < offsetof(settings_backup_accounts_t, checksum); ++i)
        hash = (hash ^ bytes[i]) * 16777619u;
    return hash;
}
static bool backup_accounts_valid(const settings_backup_accounts_t *accounts) {
    uint32_t stored = 0;
    for (unsigned i = 0; i < 4; ++i) stored |= (uint32_t)accounts->checksum[i] << (i * 8);
    if (stored != backup_accounts_checksum(accounts) || accounts->kosync_auto > 1 || accounts->footer_enabled > 1)
        return false;
    for (unsigned i = 0; i < sizeof(s_account_backup_fields)/sizeof(s_account_backup_fields[0]); ++i) {
        const account_backup_field_t *field = &s_account_backup_fields[i];
        const char *value = (const char *)accounts + field->offset;
        if (strnlen(value, field->capacity) >= field->capacity) return false;
    }
    return (!strncmp(accounts->kosync_url, "http://", 7) || !strncmp(accounts->kosync_url, "https://", 8)) &&
        (!strncmp(accounts->opds_url, "http://", 7) || !strncmp(accounts->opds_url, "https://", 8));
}

typedef struct {
    read_pico_transfer_wifi_backup_t credentials;
    uint8_t checksum[4];
} settings_backup_wifi_t;

// PICOSETA为第10版八字节标识；映射块独立校验，不改旧网络/阅读数据块。
// PICOSETA is the eight-byte v10 identifier; independently check mappings without changing old network/history blocks.
typedef struct { uint8_t actions[3], checksum[4]; } settings_backup_keys_t;
static uint32_t backup_keys_checksum(const settings_backup_keys_t *keys) {
    uint32_t hash = 2166136261u;
    for (unsigned i = 0; i < 3; ++i) hash = (hash ^ keys->actions[i]) * 16777619u;
    return hash;
}
static bool backup_keys_valid(const settings_backup_keys_t *keys) {
    uint32_t stored = 0;
    for (unsigned i = 0; i < 4; ++i) stored |= (uint32_t)keys->checksum[i] << (8 * i);
    for (unsigned i = 0; i < 3; ++i) if (keys->actions[i] >= APP_READER_KEY_COUNT) return false;
    return stored == backup_keys_checksum(keys);
}

typedef struct { uint8_t action, checksum[4]; } settings_backup_hold_t;
static uint32_t backup_hold_checksum(const settings_backup_hold_t *hold) {
    return (2166136261u ^ hold->action) * 16777619u;
}
static bool backup_hold_valid(const settings_backup_hold_t *hold) {
    uint32_t stored = 0;
    for (unsigned i = 0; i < 4; ++i) stored |= (uint32_t)hold->checksum[i] << (8 * i);
    return hold->action < APP_READER_KEY_COUNT && stored == backup_hold_checksum(hold);
}
// v12 追加独立校验的微调块，保留旧字段偏移与旧备份读取。/ V12 appends a sealed adjustment block, preserving legacy fields and readers.
typedef struct { uint8_t encoded, checksum[4]; } settings_backup_indent_adjust_t;
static uint32_t backup_indent_adjust_checksum(const settings_backup_indent_adjust_t *adjust) {
    return (2166136261u ^ adjust->encoded) * 16777619u;
}
static bool backup_indent_adjust_valid(const settings_backup_indent_adjust_t *adjust) {
    uint32_t stored = 0;
    for (unsigned i = 0; i < 4; ++i) stored |= (uint32_t)adjust->checksum[i] << (8 * i);
    return adjust->encoded <= 200 && stored == backup_indent_adjust_checksum(adjust);
}
static bool backup_word_spacing_valid(const settings_backup_indent_adjust_t *v) {
    uint32_t sum=0;for(unsigned i=0;i<4;i++)sum|=(uint32_t)v->checksum[i]<<(8*i);
    return sum==backup_indent_adjust_checksum(v);
}
// v13 adds a checked reading-weight block; older backups restore native strokes.
typedef struct { uint8_t encoded, checksum[4]; } settings_backup_weight_t;
static uint32_t backup_weight_checksum(const settings_backup_weight_t *weight) {
    return (2166136261u ^ weight->encoded) * 16777619u;
}
static bool backup_weight_valid(const settings_backup_weight_t *weight) {
    uint32_t stored = 0;
    for (unsigned i = 0; i < 4; ++i) stored |= (uint32_t)weight->checksum[i] << (8 * i);
    return (weight->encoded <= 4 || (weight->encoded>=128&&weight->encoded<=192)) && stored == backup_weight_checksum(weight);
}
typedef struct { char path[288]; uint8_t checksum[4]; } settings_backup_background_t;
static uint32_t backup_background_checksum(const settings_backup_background_t *background) {
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < sizeof(background->path); ++i) hash = (hash ^ (uint8_t)background->path[i]) * 16777619u;
    return hash;
}
static bool backup_background_valid(const settings_backup_background_t *background) {
    uint32_t stored = 0;
    for (unsigned i = 0; i < 4; ++i) stored |= (uint32_t)background->checksum[i] << (i * 8);
    return strnlen(background->path, sizeof(background->path)) < sizeof(background->path) &&
        (!background->path[0] || !strncmp(background->path, "/sdcard/pictures/", 17)) &&
        stored == backup_background_checksum(background);
}
typedef struct { uint8_t component, minutes, checksum[4]; } settings_backup_home_t;
static uint32_t backup_home_checksum(const settings_backup_home_t *v) {
    return ((2166136261u ^ v->component) * 16777619u ^ v->minutes) * 16777619u;
}
static bool backup_home_valid(const settings_backup_home_t *v) {
    uint32_t crc = 0;
    for (unsigned i = 0; i < 4; ++i) crc |= (uint32_t)v->checksum[i] << (8*i);
    return v->component < APP_HOME_COMPONENT_COUNT && v->minutes >= 5 && v->minutes <= 180 && !(v->minutes % 5) && crc == backup_home_checksum(v);
}
static unsigned backup_version(const settings_backup_v1_t *backup) {
    if (memcmp(backup->magic, "PICOSET", 7)) return 0;
    unsigned char c = backup->magic[7];
    return c >= '1' && c <= '9' ? c - '0' : c == 'A' ? 10 : c == 'B' ? 11 : c == 'C' ? 12 : c == 'D' ? 13 : c == 'E' ? 14 : c == 'F' ? 15 : c == 'G' ? 16 : c == 'H' ? 17 : c == 'I' ? 18 : c == 'J' ? 19 : c == 'K' ? 20 : c == 'L' ? 21 : c == 'M' ? 22 : c == 'N' ? 23 : c == 'O' ? 24 : c == 'P' ? 25 : c == 'Q' ? 26 : c == 'R' ? 27 : c == 'S' ? 28 : c == 'T' ? 29 : c == 'U' ? 30 : c == 'V' ? 31 : c == 'W' ? 32 : 0;
}

static void backup_erase_secret(void *ptr, size_t size) {
    volatile uint8_t *bytes = ptr;
    while (size--) *bytes++ = 0;
}

static uint32_t backup_wifi_checksum(const settings_backup_wifi_t *wifi) {
    const uint8_t *bytes = (const uint8_t *)wifi;
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < offsetof(settings_backup_wifi_t, checksum); ++i)
        hash = (hash ^ bytes[i]) * 16777619u;
    return hash;
}

static void backup_wifi_seal(settings_backup_wifi_t *wifi) {
    uint32_t hash = backup_wifi_checksum(wifi);
    for (int i = 0; i < 4; ++i) wifi->checksum[i] = (uint8_t)(hash >> (i * 8));
}

static bool backup_wifi_valid(const settings_backup_wifi_t *wifi) {
    uint32_t stored = 0;
    for (int i = 0; i < 4; ++i) stored |= (uint32_t)wifi->checksum[i] << (i * 8);
    return stored == backup_wifi_checksum(wifi) &&
           read_pico_transfer_wifi_backup_valid(&wifi->credentials);
}

enum {
    BK_SLEEP, BK_PICKUP, BK_SYS_SIZE, BK_SYS_CONTRAST, BK_LOCK,
    BK_BOOK_PX, BK_SHAKE, BK_FULL_PAGES, BK_TURN, BK_POWER_TURN,
    BK_IMMERSIVE, BK_TRACKING, BK_READING_LINE, BK_LINE_SPACING,
    BK_MARGIN, BK_PARAGRAPH, BK_SHELF,
};

static uint32_t backup_checksum(const settings_backup_v1_t *backup) {
    const uint8_t *data = (const uint8_t *)backup;
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < offsetof(settings_backup_v1_t, checksum); ++i)
        hash = (hash ^ data[i]) * 16777619u;
    return hash;
}

static uint32_t backup_indent_checksum(const settings_backup_v1_t *backup, uint8_t indent) {
    const uint8_t *data = (const uint8_t *)backup;
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < sizeof(*backup); ++i) hash = (hash ^ data[i]) * 16777619u;
    return (hash ^ indent) * 16777619u;
}
static uint32_t backup_rule_offset_checksum(const settings_backup_v1_t *backup, uint8_t indent,
                                            uint8_t rule_offset) {
    return (backup_indent_checksum(backup, indent) ^ rule_offset) * 16777619u;
}
static uint32_t backup_shutdown_checksum(const settings_backup_v1_t *backup, uint8_t indent,
                                         uint8_t rule_offset, uint8_t staged_shutdown) {
    return (backup_rule_offset_checksum(backup, indent, rule_offset) ^ staged_shutdown) * 16777619u;
}
static uint32_t backup_profile_checksum(const settings_backup_v1_t *backup, uint8_t indent,
                                        uint8_t rule_offset, uint8_t staged_shutdown,
                                        const settings_backup_profile_t *profile) {
    uint32_t hash = backup_shutdown_checksum(backup, indent, rule_offset, staged_shutdown);
    const uint8_t *data = (const uint8_t *)profile;
    for (size_t i = 0; i < offsetof(settings_backup_profile_t, checksum); ++i)
        hash = (hash ^ data[i]) * 16777619u;
    return hash;
}

static void backup_seal(settings_backup_v1_t *backup) {
    uint32_t value = backup_checksum(backup);
    for (int i = 0; i < 4; ++i) backup->checksum[i] = (uint8_t)(value >> (i * 8));
}

static bool backup_card_ready(void) {
    read_pico_sd_info_t info = {0};
    return read_pico_sd_get_info(&info) == ESP_OK && info.mounted;
}

static bool backup_path_valid(const char *path,size_t capacity);
static bool backup_font_path_valid(const char *path,size_t capacity) {
    return backup_path_valid(path,capacity) ||
        (strnlen(path,capacity)<capacity && !strcmp(path,DEFAULT_READER_FONT));
}
typedef struct { const char *key; void *state; int low,high; bool fine; } reader_backup_field_t;
static const reader_backup_field_t s_reader_backup_fields[]={
    {NVS_KEY_BOOK_PX,&s_book_px,18,90,false},
    {NVS_KEY_BOOK_WEIGHT_FINE,&s_book_weight_fine,0,64,true},
    {NVS_KEY_BOOK_AA,&s_book_antialias,0,1,false},
    {NVS_KEY_READER_FULL,&s_reader_full_pages,0,30,false},
    {NVS_KEY_READER_TURN,&s_reader_turn_effect,0,READER_TURN_EFFECT_MAX,false},
    {NVS_KEY_POWER_TURN,&s_reader_power_turn,0,1,false},
    {NVS_KEY_HIDE_IMAGES,&s_reader_hide_images,0,1,false},
    {NVS_KEY_HOLD_REFRESH,&s_reader_hold_refresh,0,1,false},
    {NVS_KEY_HOLD_ACTION,&s_reader_hold_action,0,APP_READER_KEY_COUNT-1,false},
    {"rd_key_l",&s_reader_keys[0],0,APP_READER_KEY_COUNT-1,false},
    {"rd_key_m",&s_reader_keys[1],0,APP_READER_KEY_COUNT-1,false},
    {"rd_key_r",&s_reader_keys[2],0,APP_READER_KEY_COUNT-1,false},
    {NVS_KEY_VERTICAL_TURN,&s_reader_turn_mode,0,2,false},
    {NVS_KEY_BOOK_FONTS,&s_book_fonts,0,1,false},
    {NVS_KEY_BOOK_VERTICAL,&s_book_vertical,0,1,false},
    {NVS_KEY_BOOK_LINE,&s_book_line,0,255,false},
    {NVS_KEY_BOOK_PARA,&s_book_para,0,255,false},
    {NVS_KEY_BOOK_MARGIN,&s_book_margin,0,48,false},
    {NVS_KEY_BOOK_TRACK,&s_book_tracking,0,APP_BOOK_TRACK_MAX-APP_BOOK_TRACK_MIN,false},
    {NVS_KEY_BOOK_RULE,&s_book_reading_line,0,2,false},
    {NVS_KEY_BOOK_RULE_OFFSET,&s_book_rule_offset,0,8,false},
    {NVS_KEY_BOOK_INDENT,&s_book_indent,0,3,false},
    {NVS_KEY_BOOK_INDENT_ADJUST,&s_book_indent_adjust,0,200,false},
    {NVS_KEY_BOOK_ALIGN,&s_book_align,0,APP_BOOK_ALIGN_COUNT-1,false},
    {NVS_KEY_BOOK_PROGRESS_V2,&s_book_progress_style,0,APP_BOOK_PROGRESS_STYLE_COUNT-1,false},
    {NVS_KEY_BOOK_FOOTER_PAGE,&s_book_footer_page,0,1,false},
    {NVS_KEY_BOOK_FOOTER_CHAPTER,&s_book_footer_chapter,0,1,false},
    {NVS_KEY_BOOK_FOOTER_PCT,&s_book_footer_pct,0,1,false},
    {"book_word_gap",&s_book_word_spacing,0,255,false},
    {"vol_down",&s_volume_keys[0],0,APP_READER_KEY_COUNT-1,false},
    {"vol_up",&s_volume_keys[1],0,APP_READER_KEY_COUNT-1,false},
    {"r_swipe_up",&s_reader_swipe_up_tools,0,1,false},
    {"r_swipe_bot",&s_reader_swipe_bottom_home,0,1,false},
    {"r_edge_light",&s_reader_edge_brightness,0,1,false},
    {"r_edge_warm",&s_reader_edge_warmth,0,1,false},
};
enum {READER_BACKUP_FIELDS=sizeof(s_reader_backup_fields)/sizeof(s_reader_backup_fields[0])};
typedef struct {
    char magic[8];uint8_t values[READER_BACKUP_FIELDS];
    char font[FONT_PATH_MAX],background[288];uint8_t checksum[4];
} reader_backup_t;
_Static_assert(sizeof(bool)==1,"Reader profile byte flags require 1-byte bool");
static uint32_t reader_backup_checksum(const reader_backup_t *profile) {
    const unsigned char *data=(const unsigned char*)profile;
    uint32_t hash=2166136261u;
    for(size_t i=0;i<offsetof(reader_backup_t,checksum);++i)hash=(hash^data[i])*16777619u;
    return hash;
}
static bool reader_backup_valid(const reader_backup_t *profile) {
    uint32_t stored=0;for(unsigned i=0;i<4;++i)stored|=(uint32_t)profile->checksum[i]<<(8*i);
    if(memcmp(profile->magic,"DPRREAD1",8)||stored!=reader_backup_checksum(profile)||
       !backup_font_path_valid(profile->font,sizeof(profile->font))||!backup_path_valid(profile->background,sizeof(profile->background)))return false;
    for(unsigned i=0;i<READER_BACKUP_FIELDS;++i) {
        int value=profile->values[i];const reader_backup_field_t *field=&s_reader_backup_fields[i];
        if(value<field->low||value>field->high)return false;
        if(!strcmp(field->key,NVS_KEY_BOOK_PX)&&valid_book_px(value)!=value)return false;
        if(!strcmp(field->key,NVS_KEY_READER_FULL)&&!reader_full_pages_valid(value))return false;
        if(!strcmp(field->key,NVS_KEY_BOOK_LINE)&&!(book_line_adjustment_valid(value)||(value>=100&&value<=130&&value%10==0)))return false;
        if(!strcmp(field->key,NVS_KEY_BOOK_PARA)&&app_settings_book_paragraph_value(app_settings_book_paragraph_level(value))!=value)return false;
        if(!strcmp(field->key,"book_word_gap")&&!(value<=20||value>=246))return false;
    }
    return true;
}
static void reader_backup_capture(reader_backup_t *profile) {
    memset(profile,0,sizeof(*profile));memcpy(profile->magic,"DPRREAD1",8);
    for(unsigned i=0;i<READER_BACKUP_FIELDS;++i) {
        memcpy(&profile->values[i],s_reader_backup_fields[i].state,1);
        if(s_reader_backup_fields[i].fine)profile->values[i]=s_book_weight_fine+APP_BOOK_WEIGHT_FINE_MAX;
    }
    strlcpy(profile->font,!strcmp(s_font,"builtin")?"":s_font,sizeof(profile->font));
    strlcpy(profile->background,s_book_background,sizeof(profile->background));
    uint32_t hash=reader_backup_checksum(profile);
    for(unsigned i=0;i<4;++i)profile->checksum[i]=hash>>(8*i);
}
static esp_err_t reader_backup_stage(nvs_handle_t h,const reader_backup_t *profile) {
    esp_err_t err=ESP_OK;
    for(unsigned i=0;i<READER_BACKUP_FIELDS&&err==ESP_OK;++i) {
        uint8_t value=profile->values[i];
        if(!strcmp(s_reader_backup_fields[i].key,NVS_KEY_BOOK_TRACK))value+=192;
        err=backup_nvs_set_u8(h,s_reader_backup_fields[i].key,value);
    }
    return err;
}
static void reader_backup_apply(const reader_backup_t *profile) {
    for(unsigned i=0;i<READER_BACKUP_FIELDS;++i) {
        uint8_t value=profile->values[i];
        if(s_reader_backup_fields[i].fine)value=(uint8_t)(int8_t)((int)value-APP_BOOK_WEIGHT_FINE_MAX);
        memcpy(s_reader_backup_fields[i].state,&value,1);
    }
    s_book_weight=s_book_weight_fine/APP_BOOK_WEIGHT_SUBSTEPS+2;
    s_control_holds[4]=s_reader_hold_action;
}

typedef struct {uint8_t actions[APP_HOME_UTILITY_SLOTS],checksum[4];} settings_backup_utilities_t;
static uint32_t backup_utilities_checksum(const settings_backup_utilities_t *v) {
    uint32_t hash=2166136261u;for(unsigned i=0;i<APP_HOME_UTILITY_SLOTS;++i)hash=(hash^v->actions[i])*16777619u;return hash;
}
static bool backup_utilities_valid(const settings_backup_utilities_t *v,unsigned version) {
    uint32_t stored=0;for(unsigned i=0;i<4;++i)stored|=(uint32_t)v->checksum[i]<<(8*i);
    for(unsigned i=0;i<APP_HOME_UTILITY_SLOTS;++i)if((i==0&&version>=24?(v->actions[i]&(version>=25?0x3f:0x7f)):v->actions[i])>=APP_HOME_UTIL_COUNT)return false;
    return stored==backup_utilities_checksum(v);
}
// V26 binds the device, V27 seals backup policy, V29 packs the two independent simple shortcuts.
typedef struct { uint8_t magic[8], mac[6], reserved[2], checksum[4]; } backup_identity_t;
#ifndef backup_device_mac
static esp_err_t backup_device_mac(uint8_t mac[6]) {
#ifdef ESP_PLATFORM
    return esp_efuse_mac_get_default(mac);
#else
    const uint8_t host_mac[6] = {2, 0, 0, 0, 0, 1};
    memcpy(mac, host_mac, sizeof(host_mac));
    return ESP_OK;
#endif
}
#endif
static bool backup_mac_valid(const uint8_t mac[6]) {
    uint8_t any = 0, all = 0xff;
    for (unsigned i = 0; i < 6; ++i) { any |= mac[i]; all &= mac[i]; }
    return any && all != 0xff && !(mac[0] & 1);
}
static uint32_t backup_identity_checksum(const backup_identity_t *identity) {
    uint32_t hash = 2166136261u;
    const uint8_t *bytes = (const uint8_t *)identity;
    for (size_t i = 0; i < offsetof(backup_identity_t, checksum); ++i)
        hash = (hash ^ bytes[i]) * 16777619u;
    return hash;
}
static bool backup_identity_valid(const backup_identity_t *identity, unsigned version) {
    uint32_t sum = 0;
    for (unsigned i = 0; i < 4; ++i) sum |= (uint32_t)identity->checksum[i] << (8*i);
    return !memcmp(identity->magic, "S31BIND1", 8) &&
        (version>=30?identity->reserved[0]<=7:version>=27?identity->reserved[0]<=1:!identity->reserved[0]) &&
        (version>=29?((identity->reserved[1]&15)<APP_HOME_UTIL_COUNT&&(identity->reserved[1]>>4)<APP_HOME_UTIL_COUNT):!identity->reserved[1]) && backup_mac_valid(identity->mac) &&
        sum == backup_identity_checksum(identity);
}

// The V32 holds slot for the round button is unused on restore. Its previous
// default sentinel preserves standard contrast when restoring an older backup.
enum { BACKUP_TEXT_CONTRAST_SLOT = 4 };

/* V32 appends user preferences without changing the earlier reader and account blocks. */
typedef struct {
    uint8_t sleep_off_hours, wake_presses, wifi_timeout, ble_timeout, ota_edition;
    uint8_t status_hidden, pin_grace, power_menu, ai_short;
    uint8_t holds[7], action_menu[4];
    uint8_t hardware[24], anti_fade;
    uint8_t ble_bindings[180];
    read_pico_transfer_wifi_backup_t networks[READ_PICO_TRANSFER_WIFI_PROFILES_MAX];
    uint8_t networks_present;
    uint8_t checksum[4];
} settings_backup_preferences_t;

static uint32_t preferences_checksum(const settings_backup_preferences_t *p) {
    const uint8_t *bytes = (const uint8_t *)p;
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < offsetof(settings_backup_preferences_t, checksum); ++i)
        hash = (hash ^ bytes[i]) * 16777619u;
    return hash;
}

static uint32_t preferences_u32(const uint8_t bytes[4]) {
    return (uint32_t)bytes[0] | ((uint32_t)bytes[1] << 8) |
        ((uint32_t)bytes[2] << 16) | ((uint32_t)bytes[3] << 24);
}

static bool preferences_valid(const settings_backup_preferences_t *p) {
    if (preferences_u32(p->checksum) != preferences_checksum(p) ||
        p->sleep_off_hours > APP_SLEEP_OFF_HOURS_MAX || p->wake_presses < 1 || p->wake_presses > 2 ||
        p->status_hidden > 1 || p->pin_grace > 4 || p->power_menu > 1 ||
        (p->ota_edition > 1 && p->ota_edition != 255) ||
        (p->wifi_timeout != 0 && p->wifi_timeout != 1 && p->wifi_timeout != 3 &&
         p->wifi_timeout != 5 && p->wifi_timeout != 10) ||
        (p->ble_timeout != 0 && p->ble_timeout != 5 && p->ble_timeout != 10 &&
         p->ble_timeout != 15 && p->ble_timeout != 30) || p->networks_present > 1)
        return false;
#if DPREADER_BOARD_METALIO47
    if (p->ai_short >= APP_AI_ACTION_COUNT || (preferences_u32(p->action_menu) & ~APP_ACTION_MENU_ALLOWED))
        return false;
    for (unsigned i = 0; i < sizeof(p->holds); ++i)
        if (p->holds[i] != APP_CONTROL_HOLD_DEFAULT &&
            (p->holds[i] >= APP_READER_KEY_COUNT || p->holds[i] == APP_READER_KEY_FULLSCREEN))
            return false;
#endif
#if defined(ESP_PLATFORM) && DPREADER_BOARD_METALIO47
    if (p->networks_present && !read_pico_transfer_wifi_profiles_backup_valid(p->networks)) return false;
#endif
#if defined(ESP_PLATFORM) && DPREADER_BOARD_METALIO47
    _Static_assert(sizeof(p->ble_bindings) == sizeof(ble_pt_bindings_backup_t), "V32 BLE block size");
    ble_pt_bindings_backup_t bindings;
    memcpy(&bindings, p->ble_bindings, sizeof(bindings));
    if (!metalio_preferences_backup_valid(p->hardware) || !anti_fade_minutes_valid(p->anti_fade) ||
        !ble_pt_bindings_backup_valid(&bindings)) return false;
#endif
    return true;
}

static esp_err_t preferences_capture(settings_backup_preferences_t *p) {
    p->sleep_off_hours = s_sleep_off_hours;
    p->wake_presses = s_wake_presses;
    p->wifi_timeout = s_wifi_timeout_minutes;
    p->ble_timeout = s_ble_timeout_minutes;
    p->ota_edition = s_ota_edition;
    p->status_hidden = s_book_status_hidden;
#if DPREADER_BOARD_METALIO47 && !DPREADER_LOVE_BOOK
    p->pin_grace = s_pin_grace_code_v;
#endif
#if DPREADER_BOARD_METALIO47
    p->power_menu = s_power_action_menu;
    p->ai_short = s_ai_short;
    memcpy(p->holds, s_control_holds, sizeof(p->holds));
    p->holds[BACKUP_TEXT_CONTRAST_SLOT] = s_book_text_contrast ? 1 : APP_CONTROL_HOLD_DEFAULT;
    for (unsigned i = 0; i < 4; ++i) p->action_menu[i] = s_action_menu >> (8*i);
#endif
#if defined(ESP_PLATFORM) && DPREADER_BOARD_METALIO47
    p->anti_fade = anti_fade_minutes();
    esp_err_t hardware_err = metalio_preferences_backup_capture(p->hardware);
    if (hardware_err != ESP_OK) return hardware_err;
    ble_pt_bindings_backup_t bindings;
    esp_err_t ble_err = ble_pt_bindings_backup_capture(&bindings);
    if (ble_err != ESP_OK) return ble_err;
    memcpy(p->ble_bindings, &bindings, sizeof(bindings));
#endif
#if defined(ESP_PLATFORM) && DPREADER_BOARD_METALIO47
    esp_err_t wifi_err = read_pico_transfer_export_wifi_profiles_backup(p->networks);
    if (wifi_err != ESP_OK) return wifi_err;
    p->networks_present = 1;
#endif
    uint32_t hash = preferences_checksum(p);
    for (unsigned i = 0; i < 4; ++i) p->checksum[i] = hash >> (8*i);
    return preferences_valid(p) ? ESP_OK : ESP_ERR_INVALID_RESPONSE;
}

static esp_err_t preferences_stage(nvs_handle_t h, const settings_backup_preferences_t *p) {
    esp_err_t err = ESP_OK;
#define STAGE(key, value) do { if (err == ESP_OK) err = backup_nvs_set_u8(h, key, value); } while (0)
    STAGE(NVS_KEY_SLEEP_OFF_HOURS, p->sleep_off_hours);
#if DPREADER_BOARD_METALIO47
    if (err == ESP_OK) err = backup_nvs_set_str(h, "sleep_fw", esp_app_get_description()->version);
#endif
    STAGE(NVS_KEY_WAKE_PRESSES, p->wake_presses);
    STAGE(NVS_KEY_WIFI_TIMEOUT, p->wifi_timeout);
    STAGE(NVS_KEY_BLE_TIMEOUT, p->ble_timeout);
    STAGE("ota_edition", p->ota_edition);
    STAGE("rd_status_off", p->status_hidden);
#if DPREADER_BOARD_METALIO47 && !DPREADER_LOVE_BOOK
    STAGE(NVS_KEY_PIN_GRACE, p->pin_grace);
#endif
#if DPREADER_BOARD_METALIO47
    STAGE(NVS_KEY_BOOK_TEXT_CONTRAST, p->holds[BACKUP_TEXT_CONTRAST_SLOT] == 1);
    STAGE("power_menu", p->power_menu);
    STAGE("ai_short", p->ai_short);
    for (unsigned i = 0; i < sizeof(p->holds); ++i)
        if (i != BACKUP_TEXT_CONTRAST_SLOT) STAGE(s_control_hold_names[i], p->holds[i]);
    if (err == ESP_OK) err = backup_nvs_set_u32(h, "action_menu", preferences_u32(p->action_menu));
#endif
#undef STAGE
    return err;
}

static esp_err_t preferences_apply(const settings_backup_preferences_t *p) {
    s_sleep_off_hours = p->sleep_off_hours;
    s_wake_presses = p->wake_presses;
    s_wifi_timeout_minutes = p->wifi_timeout;
    s_ble_timeout_minutes = p->ble_timeout;
    s_ota_edition = p->ota_edition;
    s_book_status_hidden = p->status_hidden != 0;
#if DPREADER_BOARD_METALIO47 && !DPREADER_LOVE_BOOK
    s_pin_grace_code_v = p->pin_grace;
#endif
#if DPREADER_BOARD_METALIO47
    s_book_text_contrast = p->holds[BACKUP_TEXT_CONTRAST_SLOT] == 1;
    s_power_action_menu = p->power_menu != 0;
    s_ai_short = (app_ai_action_t)p->ai_short;
    s_action_menu = preferences_u32(p->action_menu);
    for (unsigned i = 0; i < sizeof(p->holds); ++i)
        if (i != BACKUP_TEXT_CONTRAST_SLOT) s_control_holds[i] = p->holds[i];
#endif
#if defined(ESP_PLATFORM) && DPREADER_BOARD_METALIO47
    esp_err_t err = metalio_preferences_backup_restore(p->hardware);
    if (err == ESP_OK && p->hardware[0]) err = anti_fade_set_minutes(p->anti_fade);
    if (err != ESP_OK) return err;
    ble_pt_bindings_backup_t bindings;
    memcpy(&bindings, p->ble_bindings, sizeof(bindings));
    return ble_pt_bindings_backup_restore(&bindings);
#else
    return ESP_OK;
#endif
}

typedef struct {
    settings_backup_v1_t backup;
    uint8_t extension[7];
    settings_backup_profile_t profile;
    settings_backup_keys_t keys;
    settings_backup_hold_t hold;
    settings_backup_indent_adjust_t adjust;
    settings_backup_weight_t weight;
    settings_backup_wifi_t wifi;
    settings_backup_accounts_t accounts;
    settings_backup_preferences_t preferences;
    settings_backup_background_t background;
    settings_backup_home_t home;
    settings_backup_background_t bookmark;
    settings_backup_indent_adjust_t word_spacing;
    reader_backup_t reader;
    settings_backup_utilities_t utilities;
    uint8_t io[1024];
    struct stat current;
    uint32_t wifi_hash;
    backup_identity_t identity;
    uint8_t device_mac[6];
    bool restore_started;
} backup_workspace_t;

static esp_err_t backup_save_workspace(backup_workspace_t *work) {
    if (!backup_card_ready()) return ESP_ERR_INVALID_STATE;

    memcpy(work->backup.magic, "PICOSETW", sizeof(work->backup.magic));
    memcpy(work->identity.magic, "S31BIND1", 8);
    esp_err_t identity_err = backup_device_mac(work->identity.mac);
    if (identity_err != ESP_OK) return identity_err;
    if (!backup_mac_valid(work->identity.mac)) return ESP_ERR_INVALID_RESPONSE;
    work->identity.reserved[0] = (app_settings_autobackup_enabled()?1:0) | (s_simple_labels?0:2) | (s_simple_border?0:4);
    work->identity.reserved[1] = s_simple_utilities[0] | (s_simple_utilities[1] << 4);
    uint32_t identity_hash = backup_identity_checksum(&work->identity);
    for (unsigned i = 0; i < 4; ++i) work->identity.checksum[i] = identity_hash >> (8*i);
    uint8_t *f = work->backup.flags;
    f[BK_SLEEP] = s_sleep;
    f[BK_PICKUP] = s_pickup_wake;
    f[BK_SYS_SIZE] = s_system_size;
    f[BK_SYS_CONTRAST] = s_system_contrast;
    f[BK_LOCK] = s_lock_style;
    f[BK_BOOK_PX] = s_book_px;
    // v15复用已退役的晃动标志保存关闭抗锯齿；旧备份始终恢复开启。
    // v15 reuses the retired shake flag for disabled AA; older backups always restore AA on.
    f[BK_SHAKE] = s_book_antialias ? 0 : 1;
    f[BK_FULL_PAGES] = s_reader_full_pages;
    f[BK_TURN] = s_reader_turn_effect;
    f[BK_POWER_TURN] = s_reader_power_turn;
    f[BK_IMMERSIVE] = s_reader_immersive;
    f[BK_TRACKING] = s_book_tracking;
    f[BK_READING_LINE] = s_book_reading_line;
    f[BK_LINE_SPACING] = s_book_line;
    f[BK_MARGIN] = s_book_margin;
    f[BK_PARAGRAPH] = s_book_para;
    f[BK_SHELF] = s_shelf_style;
    strlcpy(work->backup.font, !strcmp(s_font, "builtin") ? "" : s_font, sizeof(work->backup.font));
    strlcpy(work->backup.system_font, s_system_font, sizeof(work->backup.system_font));
    strlcpy(work->backup.wallpaper, s_wallpaper, sizeof(work->backup.wallpaper));
    strlcpy(work->backup.books_dir, s_books_dir, sizeof(work->backup.books_dir));
    strlcpy(work->backup.fonts_dir, s_fonts_dir, sizeof(work->backup.fonts_dir));
    backup_seal(&work->backup);
    uint8_t idle_index = s_auto_lock_minutes == 1 ? 1 : s_auto_lock_minutes == 5 ? 2 : s_auto_lock_minutes == 10 ? 3 : 0;
    work->extension[0] = s_book_indent; work->extension[1] = s_book_rule_offset;
    work->extension[2] = (s_staged_shutdown ? 1 : 0) | (idle_index << 1) | ((uint8_t)s_main_refresh << 3);
    uint32_t extension_hash = backup_shutdown_checksum(&work->backup, s_book_indent,
                                                        s_book_rule_offset, work->extension[2]);
    for (int i = 0; i < 4; ++i) work->extension[i + 3] = (uint8_t)(extension_hash >> (i * 8));

    strlcpy(work->profile.device_name, s_device_name, sizeof(work->profile.device_name));
#if !DPREADER_LOVE_BOOK
    strlcpy(work->profile.avatar, s_avatar, sizeof(work->profile.avatar));
#endif
    strlcpy(work->profile.status_signature, s_status_signature, sizeof(work->profile.status_signature));
    // v5 及之后复用此字段的预留位；v7 在其后追加 WiFi 与阅读资料。
    // V5+ share this byte; v28 assigns the retired bit 6 to simple UI. Older backups ignore bit 6.
    work->profile.home_full_refresh = (s_home_full_refresh ? 1 : 0) |
                                (s_reader_hide_images ? 2 : 0) |
                                (s_shelf_recent_sort ? 4 : 0) |
                                (s_ble_turner ? 8 : 0) |
                                (s_reader_hold_refresh ? 16 : 0) |
                                (s_simple_ui ? 64 : 0) |
                                ((s_reader_turn_mode == 1) ? 32 : 0) |
                                (app_settings_main_fast_refresh() ? 128 : 0);
    uint32_t profile_hash = backup_profile_checksum(&work->backup, work->extension[0], work->extension[1], work->extension[2], &work->profile);
    for (int i = 0; i < 4; ++i) work->profile.checksum[i] = (uint8_t)(profile_hash >> (i * 8));


    memcpy(work->keys.actions, s_reader_keys, sizeof(work->keys.actions));
    uint32_t keys_hash = backup_keys_checksum(&work->keys);
    for (unsigned i = 0; i < 4; ++i) work->keys.checksum[i] = (uint8_t)(keys_hash >> (8 * i));
    work->hold.action = s_reader_hold_action;
    uint32_t hold_hash = backup_hold_checksum(&work->hold);
    for (unsigned i = 0; i < 4; ++i) work->hold.checksum[i] = (uint8_t)(hold_hash >> (8 * i));
    work->adjust.encoded = s_book_indent_adjust;
    uint32_t adjust_hash = backup_indent_adjust_checksum(&work->adjust);
    for (unsigned i = 0; i < 4; ++i) work->adjust.checksum[i] = (uint8_t)(adjust_hash >> (8 * i));
    work->weight.encoded = 128+s_book_weight_fine+APP_BOOK_WEIGHT_FINE_MAX;
    uint32_t weight_hash = backup_weight_checksum(&work->weight);
    for (unsigned i = 0; i < 4; ++i) work->weight.checksum[i] = (uint8_t)(weight_hash >> (8 * i));

    esp_err_t preferences_err = preferences_capture(&work->preferences);
    if (preferences_err != ESP_OK) return preferences_err;
    esp_err_t wifi_err;
    if (work->preferences.networks_present) {
        work->wifi.credentials = work->preferences.networks[0];
        wifi_err = ESP_OK;
    } else {
        wifi_err = read_pico_transfer_export_wifi_backup(&work->wifi.credentials);
    }
    if (wifi_err != ESP_OK) { backup_erase_secret(&work->wifi, sizeof(work->wifi)); return wifi_err; }
    backup_wifi_seal(&work->wifi);
    work->wifi_hash = backup_wifi_checksum(&work->wifi);

    strlcpy(work->background.path, s_book_background, sizeof(work->background.path));
    uint32_t background_hash = backup_background_checksum(&work->background);
    for (unsigned i = 0; i < 4; ++i) work->background.checksum[i] = (uint8_t)(background_hash >> (8 * i));

    FILE *file = fopen(BACKUP_TEMP, "wb");
    if (!file) { backup_erase_secret(&work->wifi, sizeof(work->wifi)); return ESP_FAIL; }
    if (setvbuf(file, (char *)work->io, _IOFBF, sizeof(work->io)) != 0) {
        fclose(file); (void)remove(BACKUP_TEMP); return ESP_FAIL;
    }
    bool ok = fwrite(&work->backup, 1, sizeof(work->backup), file) == sizeof(work->backup);
    if (ok) ok = fwrite(work->extension, 1, sizeof(work->extension), file) == sizeof(work->extension);
    if (ok) ok = fwrite(&work->profile, 1, sizeof(work->profile), file) == sizeof(work->profile);
    if (ok) ok = fwrite(&work->keys, 1, sizeof(work->keys), file) == sizeof(work->keys);
    if (ok) ok = fwrite(&work->hold, 1, sizeof(work->hold), file) == sizeof(work->hold);
    if (ok) ok = fwrite(&work->adjust, 1, sizeof(work->adjust), file) == sizeof(work->adjust);
    if (ok) ok = fwrite(&work->weight, 1, sizeof(work->weight), file) == sizeof(work->weight);
    if (ok) ok = fwrite(&work->wifi, 1, sizeof(work->wifi), file) == sizeof(work->wifi);
    if (ok) ok = fwrite(&work->background, 1, sizeof(work->background), file) == sizeof(work->background);
    work->home.component = s_home_component; work->home.minutes = s_home_goal_minutes;
    uint32_t home_hash = backup_home_checksum(&work->home);
    for (unsigned i = 0; i < 4; ++i) work->home.checksum[i] = home_hash >> (8*i);
    if (ok) ok = fwrite(&work->home, 1, sizeof(work->home), file) == sizeof(work->home);

    strlcpy(work->bookmark.path,s_bookmark,sizeof(work->bookmark.path));
    uint32_t bookmark_hash=backup_background_checksum(&work->bookmark);
    for(unsigned i=0;i<4;++i)work->bookmark.checksum[i]=bookmark_hash>>(8*i);
    if(ok)ok=fwrite(&work->bookmark,1,sizeof(work->bookmark),file)==sizeof(work->bookmark);
    backup_erase_secret(&work->wifi, sizeof(work->wifi));
    work->word_spacing.encoded = s_book_word_spacing;
    uint32_t word_hash=backup_indent_adjust_checksum(&work->word_spacing);
    for(unsigned i=0;i<4;i++)work->word_spacing.checksum[i]=word_hash>>(8*i);
    if(ok)ok=fwrite(&work->word_spacing,1,sizeof(work->word_spacing),file)==sizeof(work->word_spacing);
    reader_backup_capture(&work->reader);
    if(ok)ok=reader_backup_valid(&work->reader)&&fwrite(&work->reader,1,sizeof(work->reader),file)==sizeof(work->reader);
    memcpy(work->utilities.actions,s_home_utilities,sizeof(work->utilities.actions));
    /* v24 packs the hidden-label flag in the first action's unused high bit. */
    if(!s_home_utility_labels)work->utilities.actions[0]|=0x80;
    if(!s_home_utility_border)work->utilities.actions[0]|=0x40;
    uint32_t utilities_hash=backup_utilities_checksum(&work->utilities);
    for(unsigned i=0;i<4;++i)work->utilities.checksum[i]=utilities_hash>>(8*i);
    if(ok)ok=fwrite(&work->utilities,1,sizeof(work->utilities),file)==sizeof(work->utilities);
    if (ok) ok = fwrite(&work->identity, 1, sizeof(work->identity), file) == sizeof(work->identity);
    for (unsigned i = 0; i < sizeof(s_account_backup_fields)/sizeof(s_account_backup_fields[0]); ++i) {
        const account_backup_field_t *field = &s_account_backup_fields[i];
        strlcpy((char *)&work->accounts + field->offset, field->state, field->capacity);
    }
    work->accounts.kosync_auto = s_kosync_auto;
    work->accounts.footer_enabled = s_book_footer_enabled;
    uint32_t accounts_hash = backup_accounts_checksum(&work->accounts);
    for (unsigned i = 0; i < 4; ++i) work->accounts.checksum[i] = accounts_hash >> (8*i);
    if (ok) ok = backup_accounts_valid(&work->accounts) &&
        fwrite(&work->accounts, 1, sizeof(work->accounts), file) == sizeof(work->accounts);
    if (ok) ok = fwrite(&work->preferences, 1, sizeof(work->preferences), file) == sizeof(work->preferences);
    if (ok) ok = book_history_backup_write(file) == ESP_OK;
    if (ok) ok = fflush(file) == 0;
    if (ok) ok = fsync(fileno(file)) == 0;
    if (fclose(file) != 0) ok = false;
    bool rotated = false;
    if (ok && stat(BACKUP_FILE, &work->current) == 0) {
        if (remove(BACKUP_PREVIOUS) != 0 && errno != ENOENT) ok = false;
        if (ok && rename(BACKUP_FILE, BACKUP_PREVIOUS) == 0) rotated = true;
        else if (ok) ok = false;
    } else if (ok && errno != ENOENT) ok = false;
    if (ok && rename(BACKUP_TEMP, BACKUP_FILE) != 0) ok = false;
    if (!ok && rotated) (void)rename(BACKUP_PREVIOUS, BACKUP_FILE);
    // Retain the previous completed snapshot after a successful rotation.
    if (!ok) { (void)remove(BACKUP_TEMP); return ESP_FAIL; }
    return ESP_OK;
}

#if CONFIG_DPREADER_AUTO_RESTORE
// 0 waiting, 1 started, 2 complete, 3 failed. Start is durable before profile writes.
static esp_err_t backup_restore_mark(uint8_t state) {
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    if (state != 2) {
        err = nvs_set_u8(h, "restore_state", state);
        if (err == ESP_OK) err = nvs_set_u8(h, "backup_guard", 0);
    } else {
        err = nvs_set_u8(h, "backup_guard", 1);
        if (err == ESP_OK) err = nvs_set_u8(h, "restore_state", 2);
    }
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    if (err == ESP_OK) {
        s_auto_backup.healthy = state == 2;
        s_auto_backup.restore_pending = false;
        s_auto_backup.restore_status = state == 2 ? APP_RESTORE_NORMAL : APP_RESTORE_BLOCKED;
        s_auto_backup.saved = false;
    }
    return err;
}
#endif

static esp_err_t backup_rearm_after_explicit_success(void) {
#if CONFIG_DPREADER_AUTO_RESTORE
    esp_err_t err = backup_restore_mark(2);
    if (err == ESP_OK) s_auto_backup.failed = false;
    return err;
#elif CONFIG_DPREADER_AUTO_BACKUP
    if (!s_auto_backup.healthy || s_auto_backup.failed) {
        nvs_handle_t h;
        esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &h);
        if (err != ESP_OK) return err;
        err = nvs_set_u8(h, "backup_guard", 1);
        if (err == ESP_OK) err = nvs_commit(h);
        nvs_close(h);
        if (err != ESP_OK) return err;
        s_auto_backup.healthy = true;
        s_auto_backup.failed = false;
        s_auto_backup.saved = false;
    }
#endif
    return ESP_OK;
}

esp_err_t app_settings_backup_save(void) {
#if CONFIG_DPREADER_AUTO_RESTORE
    if (s_auto_backup.restore_status >= APP_RESTORE_NEED_MEDIA || s_auto_backup.restore_pending)
        return ESP_ERR_INVALID_STATE;
#endif
    if (!backup_card_ready()) return ESP_ERR_INVALID_STATE;
    backup_workspace_t *work = backup_alloc(sizeof(*work));
    if (!work) return ESP_ERR_NO_MEM;
    esp_err_t err = backup_save_workspace(work);
    backup_erase_secret(work, sizeof(*work));
    free(work);
    return err == ESP_OK ? backup_rearm_after_explicit_success() : err;
}

bool app_settings_autobackup_enabled(void) {
#if CONFIG_DPREADER_AUTO_BACKUP
    return s_auto_backup.enabled;
#else
    return false;
#endif
}
bool app_settings_autobackup_blocked(void) {
#if CONFIG_DPREADER_AUTO_BACKUP
    return !s_auto_backup.healthy || s_auto_backup.failed;
#else
    return true;
#endif
}
esp_err_t app_settings_set_autobackup_enabled(bool enabled) {
#if CONFIG_DPREADER_AUTO_BACKUP
    if (s_auto_backup.enabled == enabled) return ESP_OK;
    nvs_handle_t h;
    esp_err_t err = backup_nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = backup_nvs_set_u8(h, "auto_backup", enabled ? 1 : 0);
    if (err == ESP_OK) err = settings_backup_commit(h);
    nvs_close(h);
    if (err == ESP_OK) {
        s_auto_backup.enabled = enabled;
        s_auto_backup.saved = false;
    }
    return err;
#else
    (void)enabled;
    return ESP_ERR_INVALID_STATE;
#endif
}

void app_settings_autobackup_media_changed(void) {
#if CONFIG_DPREADER_AUTO_BACKUP
    s_auto_backup.saved = false;
#endif
}

esp_err_t app_settings_autobackup_checkpoint(void) {
#if CONFIG_DPREADER_AUTO_BACKUP
    if (!s_auto_backup.enabled) return ESP_OK;
    if (!s_auto_backup.healthy || s_auto_backup.failed || !backup_card_ready())
        return ESP_ERR_INVALID_STATE;
    backup_workspace_t *work = backup_alloc(sizeof(*work));
    if (!work) return ESP_ERR_NO_MEM;
    esp_err_t err = preferences_capture(&work->preferences);
    if (err == ESP_OK && !work->preferences.networks_present)
        err = read_pico_transfer_export_wifi_backup(&work->wifi.credentials);
    uint32_t progress = book_progress_revision();
    uint32_t revision = s_auto_backup.revision;
    if (err == ESP_OK && !(s_auto_backup.saved && revision == s_auto_backup.saved_revision &&
        progress == s_auto_backup.saved_progress &&
        (work->preferences.networks_present || backup_wifi_checksum(&work->wifi) == s_auto_backup.saved_wifi) &&
        preferences_u32(work->preferences.checksum) == s_auto_backup.saved_preferences &&
        stat(BACKUP_FILE, &work->current) == 0 && S_ISREG(work->current.st_mode))) {
        err = backup_save_workspace(work);
        if (err == ESP_OK) {
            s_auto_backup.saved_revision = revision;
            s_auto_backup.saved_progress = progress;
            s_auto_backup.saved_wifi = work->wifi_hash;
            s_auto_backup.saved_preferences = preferences_u32(work->preferences.checksum);
            s_auto_backup.saved = true;
        }
    }
    backup_erase_secret(work, sizeof(*work));
    free(work);
    if (err != ESP_OK) ESP_LOGW(TAG, "automatic backup failed: %s", esp_err_to_name(err));
    return err;
#else
    return ESP_ERR_INVALID_STATE;
#endif
}

static bool backup_path_valid(const char *path, size_t capacity) {
    size_t len = strnlen(path, capacity);
    if (len == capacity) return false;
    if (!len) return true;
    if (strncmp(path, "/sdcard/", 8) || !path[8]) return false;
    for (size_t i = 8; i < len; ++i)
        if ((unsigned char)path[i] < 32 || path[i] == '\\' || path[i] == ':') return false;
    for (const char *part = path + 8; *part;) {
        const char *end = strchr(part, '/');
        size_t n = end ? (size_t)(end - part) : strlen(part);
        if (!n || (n == 1 && part[0] == '.') || (n == 2 && part[0] == '.' && part[1] == '.')) return false;
        if (!end) break;
        part = end + 1;
    }
    return true;
}

static bool backup_valid(const settings_backup_v1_t *backup) {
    const uint8_t *f = backup->flags;
    uint32_t checksum = 0;
    for (int i = 0; i < 4; ++i) checksum |= (uint32_t)backup->checksum[i] << (i * 8);
    if (!backup_version(backup) || checksum != backup_checksum(backup)) return false;
    if (f[BK_SLEEP] > APP_SLEEP_OFF || f[BK_PICKUP] > 1 ||
        f[BK_SYS_SIZE] < 100 || f[BK_SYS_SIZE] > 202 || (f[BK_SYS_SIZE] % 10 && f[BK_SYS_SIZE] != 144 && f[BK_SYS_SIZE] != 202) ||
        f[BK_SYS_CONTRAST] < 100 || f[BK_SYS_CONTRAST] > 140 || f[BK_SYS_CONTRAST] % 10 ||
        f[BK_LOCK] > 3 || f[BK_BOOK_PX] < BOOK_READER_PX_MIN || f[BK_BOOK_PX] > BOOK_READER_PX_MAX ||
        f[BK_SHAKE] > 1 || !reader_full_pages_valid(f[BK_FULL_PAGES]) ||
        f[BK_TURN] > READER_TURN_EFFECT_MAX || f[BK_POWER_TURN] > 1 || f[BK_IMMERSIVE] > 1 ||
        f[BK_TRACKING] > (backup_version(backup)>=20 ? APP_BOOK_TRACK_COUNT-1 : backup_version(backup)==19 ? 20 : 4) || f[BK_READING_LINE] > 2 ||
        (!book_line_adjustment_valid(f[BK_LINE_SPACING])&&(f[BK_LINE_SPACING]<100||f[BK_LINE_SPACING]>150)) ||
        f[BK_MARGIN] > 60 ||
        (backup_version(backup)>=20 ?
         (f[BK_PARAGRAPH]!=100 && app_settings_book_paragraph_value(app_settings_book_paragraph_level(f[BK_PARAGRAPH])) != f[BK_PARAGRAPH]) :
         f[BK_PARAGRAPH] > 75 || f[BK_PARAGRAPH] % 25) ||
        f[BK_SHELF] < 1 || f[BK_SHELF] > 5) return false;
    if (strnlen(backup->books_dir, sizeof(backup->books_dir)) == sizeof(backup->books_dir) ||
        strnlen(backup->fonts_dir, sizeof(backup->fonts_dir)) == sizeof(backup->fonts_dir)) return false;
    return backup_font_path_valid(backup->font, sizeof(backup->font)) &&
           backup_font_path_valid(backup->system_font, sizeof(backup->system_font)) &&
           backup_path_valid(backup->wallpaper, sizeof(backup->wallpaper)) &&
           valid_media_dir(backup->books_dir) && valid_media_dir(backup->fonts_dir);
}

static bool backup_file_exists(const char *path, bool directory) {
    struct stat st;
    return stat(path, &st) == 0 && (directory ? S_ISDIR(st.st_mode) : S_ISREG(st.st_mode));
}

static esp_err_t backup_restore_workspace(backup_workspace_t *work, const char *source,
                                          bool apply, bool require_device) {
    FILE *file = fopen(source, "rb");
    if (!file) return ESP_ERR_NOT_FOUND;
    if (setvbuf(file, (char *)work->io, _IOFBF, sizeof(work->io)) != 0) {
        fclose(file); return ESP_FAIL;
    }

    bool ok = fread(&work->backup, 1, sizeof(work->backup), file) == sizeof(work->backup);
    unsigned version = ok ? backup_version(&work->backup) : 0;
    work->hold.action = APP_READER_KEY_HOME;
    uint8_t indent = 2, rule_offset = 4, staged_shutdown = 0;
    work->adjust.encoded = 20;
    work->weight.encoded = 2;
#if DPREADER_BOARD_METALIO47
    strlcpy(work->profile.device_name, "Metalio S31", sizeof(work->profile.device_name));
#else
    strlcpy(work->profile.device_name, "dpReader", sizeof(work->profile.device_name));
#endif
    work->keys.actions[0] = APP_READER_KEY_PREV; work->keys.actions[1] = APP_READER_KEY_TOOLS; work->keys.actions[2] = APP_READER_KEY_NEXT;

    bool has_wifi = false;

    work->home.component = APP_HOME_FOLDER; work->home.minutes = 30;
    app_main_refresh_mode_t main_refresh = APP_MAIN_REFRESH_NORMAL;
    if (ok && !memcmp(work->backup.magic, "PICOSET2", 8)) {
        uint8_t extension[5];
        ok = fread(extension, 1, sizeof(extension), file) == sizeof(extension);
        if (ok) {
            indent = extension[0];
            uint32_t stored = 0;
            for (int i = 0; i < 4; ++i) stored |= (uint32_t)extension[i + 1] << (i * 8);
            ok = indent <= 3 && stored == backup_indent_checksum(&work->backup, indent);
        }
    } else if (ok && !memcmp(work->backup.magic, "PICOSET3", 8)) {
        uint8_t extension[6];
        ok = fread(extension, 1, sizeof(extension), file) == sizeof(extension);
        if (ok) {
            indent = extension[0]; rule_offset = extension[1];
            uint32_t stored = 0;
            for (int i = 0; i < 4; ++i) stored |= (uint32_t)extension[i + 2] << (i * 8);
            ok = indent <= 3 && rule_offset <= 8 &&
                 stored == backup_rule_offset_checksum(&work->backup, indent, rule_offset);
        }
    } else if (ok && (version >= 4)) {
        uint8_t extension[7];
        ok = fread(extension, 1, sizeof(extension), file) == sizeof(extension);
        if (ok) {
            indent = extension[0]; rule_offset = extension[1]; staged_shutdown = extension[2];
            uint32_t stored = 0;
            for (int i = 0; i < 4; ++i) stored |= (uint32_t)extension[i + 3] << (i * 8);
            ok = indent <= 3 && rule_offset <= 8 && staged_shutdown <= ((version >= 9) ? 23 : !memcmp(work->backup.magic, "PICOSET8", 8) ? 7 : 1) &&
                 stored == backup_shutdown_checksum(&work->backup, indent, rule_offset, staged_shutdown);
        }
        if (ok && (version >= 5)) {
            ok = fread(&work->profile, 1, sizeof(work->profile), file) == sizeof(work->profile);
            if (ok) {
                uint32_t stored = 0;
                for (int i = 0; i < 4; ++i) stored |= (uint32_t)work->profile.checksum[i] << (i * 8);
                ok = stored == backup_profile_checksum(&work->backup, indent, rule_offset,
                                                        staged_shutdown, &work->profile) &&
                     strnlen(work->profile.device_name, sizeof(work->profile.device_name)) < sizeof(work->profile.device_name) &&
                     work->profile.device_name[0] &&
                     strnlen(work->profile.status_signature, sizeof(work->profile.status_signature)) < sizeof(work->profile.status_signature) &&
                     // V28 reuses retired bit 6 for simple UI; older versions retain their original interpretation.
                     backup_path_valid(work->profile.avatar, sizeof(work->profile.avatar));
            }
        }
    }
    if (ok && (version >= 9)) main_refresh = (app_main_refresh_mode_t)(staged_shutdown >> 3);
    else if (ok && (work->profile.home_full_refresh & 128)) main_refresh = APP_MAIN_REFRESH_FAST;
    if (ok && version >= 10)
        ok = fread(&work->keys, 1, sizeof(work->keys), file) == sizeof(work->keys) && backup_keys_valid(&work->keys);
    if (ok && version >= 11)
        ok = fread(&work->hold, 1, sizeof(work->hold), file) == sizeof(work->hold) && backup_hold_valid(&work->hold);
    if (ok && version >= 12)
        ok = fread(&work->adjust, 1, sizeof(work->adjust), file) == sizeof(work->adjust) && backup_indent_adjust_valid(&work->adjust);
    if (ok && version >= 13)
        ok = fread(&work->weight, 1, sizeof(work->weight), file) == sizeof(work->weight) && backup_weight_valid(&work->weight) && (version>=17 || work->weight.encoded<=4);
    long history_position = -1;
    has_wifi = ok && version >= 7;
    if (has_wifi) ok = fread(&work->wifi, 1, sizeof(work->wifi), file) == sizeof(work->wifi) && backup_wifi_valid(&work->wifi);
    if (ok && has_wifi) work->wifi_hash = backup_wifi_checksum(&work->wifi);
    if (ok && version >= 14) ok = fread(&work->background, 1, sizeof(work->background), file) == sizeof(work->background) &&
        backup_background_valid(&work->background) && backup_path_valid(work->background.path, sizeof(work->background.path));
    if (ok && version >= 16) ok = fread(&work->home, 1, sizeof(work->home), file) == sizeof(work->home) && backup_home_valid(&work->home);
    if(ok && version>=18)ok=fread(&work->bookmark,1,sizeof(work->bookmark),file)==sizeof(work->bookmark)&&
        backup_background_valid(&work->bookmark)&&backup_path_valid(work->bookmark.path,sizeof(work->bookmark.path));
    if(ok&&version>=21)ok=fread(&work->word_spacing,1,sizeof(work->word_spacing),file)==sizeof(work->word_spacing)&&
        (work->word_spacing.encoded<=20||work->word_spacing.encoded>=246)&&backup_word_spacing_valid(&work->word_spacing);

    if(ok&&version>=22)ok=fread(&work->reader,1,sizeof(work->reader),file)==sizeof(work->reader)&&reader_backup_valid(&work->reader);

    if(ok&&version>=23)ok=fread(&work->utilities,1,sizeof(work->utilities),file)==sizeof(work->utilities)&&backup_utilities_valid(&work->utilities,version);
    bool utility_labels=s_home_utility_labels,utility_border=s_home_utility_border;
    if(ok&&version>=24){utility_labels=!(work->utilities.actions[0]&0x80);if(version>=25)utility_border=!(work->utilities.actions[0]&0x40);work->utilities.actions[0]&=version>=25?0x3f:0x7f;}
    if (ok && version >= 26)
        ok = fread(&work->identity, 1, sizeof(work->identity), file) == sizeof(work->identity) &&
            backup_identity_valid(&work->identity, version);
    if (ok && version >= 31)
        ok = fread(&work->accounts, 1, sizeof(work->accounts), file) == sizeof(work->accounts) &&
            backup_accounts_valid(&work->accounts);
    if (ok && version >= 32) {
        ok = fread(&work->preferences, 1, sizeof(work->preferences), file) == sizeof(work->preferences) &&
            preferences_valid(&work->preferences);
        if (ok && work->preferences.networks_present)
            ok = has_wifi && !memcmp(&work->wifi.credentials, &work->preferences.networks[0],
                                     sizeof(work->wifi.credentials));
    }
    if (require_device) {
        ok = ok && version >= 26 && backup_device_mac(work->device_mac) == ESP_OK &&
            !memcmp(work->identity.mac, work->device_mac, sizeof(work->device_mac));
    }
    bool has_history = ok && version >= 6;
    if (has_history) {
        history_position = ftell(file);
        ok = history_position >= 0 && book_history_backup_validate(file);
    } else if (ok) ok = fgetc(file) == EOF && !ferror(file);
    if (fclose(file) != 0) ok = false;
    if (!ok || !backup_valid(&work->backup)) {
        backup_erase_secret(&work->wifi, sizeof(work->wifi));
        return ESP_ERR_INVALID_RESPONSE;
    }

    if (!apply) return ESP_OK;
    work->restore_started = true;

    if(version<20) {
        int px=version<19?(int)work->backup.flags[BK_TRACKING]*2-4:(int)work->backup.flags[BK_TRACKING]-4;
        if(px>APP_BOOK_TRACK_MAX)px=APP_BOOK_TRACK_MAX;
        work->backup.flags[BK_TRACKING]=(uint8_t)(px-APP_BOOK_TRACK_MIN);
    }
    work->backup.flags[BK_MARGIN] = work->backup.flags[BK_MARGIN] > 48 ? 48 : work->backup.flags[BK_MARGIN];
    if (version < 15) work->backup.flags[BK_SHAKE] = 0; // 旧晃动标志不改变AA。/ Old shake flags never affect AA.
    // 已删除组件的备份仍可恢复，其余字段不受影响。/ Restore retired widget backups without rejecting other preferences.
    if (work->home.component == 3) work->home.component = APP_HOME_CLOCK;
    // The backup contains paths, not the corresponding font or image bytes.
    // 备份只含资源路径；资源已被删除时回退到安全的内建选项。
    if (!require_device && work->backup.font[0] && strcmp(work->backup.font, DEFAULT_READER_FONT) && !backup_file_exists(work->backup.font, false)) work->backup.font[0] = 0;
    if (!require_device && work->backup.system_font[0] && strcmp(work->backup.system_font, DEFAULT_READER_FONT) && !backup_file_exists(work->backup.system_font, false)) work->backup.system_font[0] = 0;
    if (work->backup.wallpaper[0] && !backup_file_exists(work->backup.wallpaper, false)) {
        work->backup.wallpaper[0] = 0;
        if (work->backup.flags[BK_LOCK] == 1) work->backup.flags[BK_LOCK] = 0;
    }
    if (!work->backup.wallpaper[0] && work->backup.flags[BK_LOCK] == 1) work->backup.flags[BK_LOCK] = 0;
    // 已移除的书架样式保留旧编号的读取兼容，恢复时改为默认亚克力。
    // Accept retired IDs from old backups and restore the default acrylic style.
    if (work->backup.flags[BK_SHELF] == 3 || work->backup.flags[BK_SHELF] == 4) work->backup.flags[BK_SHELF] = 2;
    if (work->profile.avatar[0] && !backup_file_exists(work->profile.avatar, false)) work->profile.avatar[0] = 0;
    if(!backup_file_exists(work->backup.books_dir,true))strlcpy(work->backup.books_dir,"/sdcard/books",sizeof(work->backup.books_dir));
    if(!backup_file_exists(work->backup.fonts_dir,true))strlcpy(work->backup.fonts_dir,"/sdcard/fonts",sizeof(work->backup.fonts_dir));

    nvs_handle_t h;
    esp_err_t err = backup_nvs_open(NVS_NS, NVS_READWRITE, &h);
    if (err != ESP_OK) { backup_erase_secret(&work->wifi, sizeof(work->wifi)); return err; }
#define BACKUP_SET_U8(key, index) do { if (err == ESP_OK) err = backup_nvs_set_u8(h, key, work->backup.flags[index]); } while (0)
#define BACKUP_SET_STR(key, value) do { if (err == ESP_OK) err = backup_nvs_set_str(h, key, value); } while (0)
    for (unsigned i = 0; i < 3 && err == ESP_OK; ++i) err = backup_nvs_set_u8(h, s_reader_key_names[i], work->keys.actions[i]);
    if (err == ESP_OK) err = backup_nvs_set_u8(h, NVS_KEY_HOLD_ACTION, work->hold.action);
    BACKUP_SET_U8(NVS_KEY_SLEEP, BK_SLEEP);
    if (err == ESP_OK) err = backup_nvs_set_u8(h, NVS_KEY_SHUTDOWN_MODE, staged_shutdown & 1);
    uint8_t idle_values[] = {0, 1, 5, 10};
    uint8_t idle_minutes = idle_values[(staged_shutdown >> 1) & 3];
    if (err == ESP_OK) err = backup_nvs_set_u8(h, NVS_KEY_AUTO_LOCK, idle_minutes);
    if (err == ESP_OK) err = backup_nvs_set_u8(h, NVS_KEY_BLE_TURNER, (work->profile.home_full_refresh & 8) != 0);
    if (err == ESP_OK) err = backup_nvs_set_u8(h, NVS_KEY_HOLD_REFRESH, (work->profile.home_full_refresh & 16) != 0);
    if (err == ESP_OK) err = backup_nvs_set_u8(h, NVS_KEY_VERTICAL_TURN, (work->profile.home_full_refresh & 32) != 0);

    bool restore_simple=s_simple_choice_known?s_simple_ui:(s_simple_ui||(version>=28&&(work->profile.home_full_refresh&64)!=0));
    if (err == ESP_OK) err = backup_nvs_set_u8(h, "simple_ui", restore_simple);
    if (err == ESP_OK) err = backup_nvs_set_u8(h, NVS_KEY_HOME_FULL, work->profile.home_full_refresh & 1);
    if (err == ESP_OK) err = backup_nvs_set_u8(h, NVS_KEY_MAIN_REFRESH, (uint8_t)main_refresh);
    if (err == ESP_OK) err = backup_nvs_set_u8(h, NVS_KEY_HOME_COMPONENT, work->home.component);
    if (err == ESP_OK) err = backup_nvs_set_u8(h, NVS_KEY_HOME_GOAL, work->home.minutes);
    if (err == ESP_OK) err = backup_nvs_set_u8(h, NVS_KEY_HIDE_IMAGES, (work->profile.home_full_refresh & 2) != 0);
    if (err == ESP_OK) err = backup_nvs_set_u8(h, NVS_KEY_SHELF_RECENT, (work->profile.home_full_refresh & 4) != 0);
    if (err == ESP_OK) err = backup_nvs_set_str(h, NVS_KEY_DEVICE_NAME, work->profile.device_name);
#if !DPREADER_LOVE_BOOK
    if (err == ESP_OK) err = backup_nvs_set_str(h, NVS_KEY_AVATAR, work->profile.avatar);
#endif
    if (err == ESP_OK) err = backup_nvs_set_str(h, NVS_KEY_STATUS_SIGNATURE, work->profile.status_signature);
    BACKUP_SET_U8(NVS_KEY_PICKUP, BK_PICKUP);
    BACKUP_SET_U8(NVS_KEY_SYS_SIZE, BK_SYS_SIZE);
    if(err==ESP_OK)err=backup_nvs_set_u8(h,"sys_size_rev",2);
    if(err==ESP_OK)err=backup_nvs_set_u8(h,"book_word_gap",work->word_spacing.encoded);
    BACKUP_SET_U8(NVS_KEY_SYS_CONTRAST, BK_SYS_CONTRAST);
    BACKUP_SET_U8(NVS_KEY_LOCK_STYLE, BK_LOCK);
    BACKUP_SET_U8(NVS_KEY_BOOK_PX, BK_BOOK_PX);

    BACKUP_SET_U8(NVS_KEY_READER_FULL, BK_FULL_PAGES);
    BACKUP_SET_U8(NVS_KEY_READER_TURN, BK_TURN);
    BACKUP_SET_U8(NVS_KEY_POWER_TURN, BK_POWER_TURN);
    BACKUP_SET_U8(NVS_KEY_IMMERSIVE, BK_IMMERSIVE);
    if(err==ESP_OK)err=backup_nvs_set_u8(h,NVS_KEY_BOOK_TRACK,192+work->backup.flags[BK_TRACKING]);
    if (err == ESP_OK) err = backup_nvs_set_u8(h, NVS_KEY_BOOK_INDENT, indent);
    if (err == ESP_OK) err = backup_nvs_set_u8(h, NVS_KEY_BOOK_INDENT_ADJUST, work->adjust.encoded);
    int fine_weight=work->weight.encoded<=4?((int)work->weight.encoded-2)*APP_BOOK_WEIGHT_SUBSTEPS:(int)work->weight.encoded-128-APP_BOOK_WEIGHT_FINE_MAX;
    if (err == ESP_OK) err = backup_nvs_set_u8(h, NVS_KEY_BOOK_WEIGHT, fine_weight/APP_BOOK_WEIGHT_SUBSTEPS+2);
    if (err == ESP_OK) err = backup_nvs_set_u8(h, NVS_KEY_BOOK_WEIGHT_FINE, fine_weight+APP_BOOK_WEIGHT_FINE_MAX);
    const bool antialias = version < 15 || work->backup.flags[BK_SHAKE] == 0;
    if (err == ESP_OK) err = backup_nvs_set_u8(h, NVS_KEY_BOOK_AA, antialias ? 1 : 0);
    BACKUP_SET_U8(NVS_KEY_BOOK_RULE, BK_READING_LINE);
    if (err == ESP_OK) err = backup_nvs_set_u8(h, NVS_KEY_BOOK_RULE_OFFSET, rule_offset);
    if(work->backup.flags[BK_LINE_SPACING]>=100&&work->backup.flags[BK_LINE_SPACING]<=180)work->backup.flags[BK_LINE_SPACING]=work->backup.flags[BK_LINE_SPACING]>130?130:((work->backup.flags[BK_LINE_SPACING]+5)/10)*10;
    BACKUP_SET_U8(NVS_KEY_BOOK_LINE, BK_LINE_SPACING);
    BACKUP_SET_U8(NVS_KEY_BOOK_MARGIN, BK_MARGIN);
    if(work->backup.flags[BK_PARAGRAPH]==100)work->backup.flags[BK_PARAGRAPH]=75;
    BACKUP_SET_U8(NVS_KEY_BOOK_PARA, BK_PARAGRAPH);
    BACKUP_SET_U8(NVS_KEY_SHELF_STYLE, BK_SHELF);
    if (err == ESP_OK) err = backup_nvs_set_u8(h, NVS_KEY_SHELF_V22, 1);
    if (work->background.path[0] && !backup_file_exists(work->background.path, false)) work->background.path[0] = 0;
    BACKUP_SET_STR(NVS_KEY_BOOK_BACKGROUND, work->background.path);
    BACKUP_SET_STR(NVS_KEY_FONT, work->backup.font);
    BACKUP_SET_STR(NVS_KEY_SYS_FONT, work->backup.system_font);
    if(work->bookmark.path[0] && !backup_file_exists(work->bookmark.path,false))work->bookmark.path[0]=0;
    if(!work->bookmark.path[0] && work->backup.flags[BK_LOCK]==3)work->backup.flags[BK_LOCK]=0;
    BACKUP_SET_STR(NVS_KEY_BOOKMARK, work->bookmark.path);
    if(err==ESP_OK)err=backup_nvs_set_u8(h,NVS_KEY_LOCK_STYLE,work->backup.flags[BK_LOCK]);
    BACKUP_SET_STR(NVS_KEY_WALLPAPER, work->backup.wallpaper);
    BACKUP_SET_STR(NVS_KEY_BOOKS_DIR, work->backup.books_dir);
    BACKUP_SET_STR(NVS_KEY_FONTS_DIR, work->backup.fonts_dir);
#undef BACKUP_SET_U8
#undef BACKUP_SET_STR
    if(err==ESP_OK&&version>=22)err=reader_backup_stage(h,&work->reader);
    if(version>=23)for(unsigned i=0;i<APP_HOME_UTILITY_SLOTS&&err==ESP_OK;++i)err=backup_nvs_set_u8(h,s_home_utility_keys[i],work->utilities.actions[i]);
    if(err==ESP_OK&&version>=24)err=backup_nvs_set_u8(h,"home_util_lbl",utility_labels?1:0);
    if(err==ESP_OK&&version>=25)err=backup_nvs_set_u8(h,"home_util_bdr",utility_border?1:0);
    if(version>=29)for(unsigned i=0;i<2&&err==ESP_OK;++i)
        err=backup_nvs_set_u8(h,s_simple_utility_keys[i],(work->identity.reserved[1]>>(i*4))&15);
    if(version>=30&&err==ESP_OK)err=backup_nvs_set_u8(h,"simple_labels",!(work->identity.reserved[0]&2));
    if(version>=30&&err==ESP_OK)err=backup_nvs_set_u8(h,"simple_border",!(work->identity.reserved[0]&4));
    if (err == ESP_OK && version >= 27)
        err = backup_nvs_set_u8(h, "auto_backup", work->identity.reserved[0]&1);
    if (version >= 31) {
        for (unsigned i = 0; i < sizeof(s_account_backup_fields)/sizeof(s_account_backup_fields[0]) && err == ESP_OK; ++i) {
            const account_backup_field_t *field = &s_account_backup_fields[i];
            err = backup_nvs_set_str(h, field->key, (const char *)&work->accounts + field->offset);
        }
        if (err == ESP_OK) err = backup_nvs_set_u8(h, NVS_KEY_KOSYNC_AUTO, work->accounts.kosync_auto);
        if (err == ESP_OK) err = backup_nvs_set_u8(h, "rd_footer_on", work->accounts.footer_enabled);
    }
    if (err == ESP_OK && version >= 32) err = preferences_stage(h, &work->preferences);
    if (err == ESP_OK) err = settings_backup_commit(h);
    nvs_close(h);
#if FONT_FLASH_FALLBACK
    if (err == ESP_OK) err = app_settings_clear_flash_fonts();
#endif
    if (err != ESP_OK) { backup_erase_secret(&work->wifi, sizeof(work->wifi)); return err; }
#if CONFIG_DPREADER_AUTO_BACKUP
    if (version >= 27) s_auto_backup.enabled = (work->identity.reserved[0]&1) != 0;
#endif

    memcpy(s_reader_keys, work->keys.actions, sizeof(s_reader_keys));
    s_reader_hold_action = work->hold.action;
    s_book_weight = fine_weight/APP_BOOK_WEIGHT_SUBSTEPS+2;
    s_book_weight_fine = fine_weight;
    s_book_antialias = antialias;
    const uint8_t *f = work->backup.flags;
    s_sleep = (app_sleep_mode_t)f[BK_SLEEP];
    s_staged_shutdown = (staged_shutdown & 1) != 0;
    s_auto_lock_minutes = idle_minutes;
    s_ble_turner = (work->profile.home_full_refresh & 8) != 0;
    s_reader_hold_refresh = (work->profile.home_full_refresh & 16) != 0;
    s_reader_turn_mode = (work->profile.home_full_refresh & 32) != 0 ? 1 : 0;
    s_simple_ui=restore_simple;s_simple_choice_known=true;
    s_home_full_refresh = (work->profile.home_full_refresh & 1) != 0;
    s_main_refresh = main_refresh;
    s_reader_hide_images = (work->profile.home_full_refresh & 2) != 0;
    s_shelf_recent_sort = (work->profile.home_full_refresh & 4) != 0;
    strlcpy(s_device_name, work->profile.device_name, sizeof(s_device_name));
#if !DPREADER_LOVE_BOOK
    strlcpy(s_avatar, work->profile.avatar, sizeof(s_avatar));
#endif
    strlcpy(s_status_signature, work->profile.status_signature, sizeof(s_status_signature));
    s_pickup_wake = f[BK_PICKUP];
    s_system_size = f[BK_SYS_SIZE];
    s_system_contrast = f[BK_SYS_CONTRAST];
    s_lock_style = f[BK_LOCK];
    s_book_px = f[BK_BOOK_PX];

    s_reader_full_pages = f[BK_FULL_PAGES];
    s_reader_turn_effect = f[BK_TURN];
    s_reader_power_turn = f[BK_POWER_TURN];
    s_reader_immersive = f[BK_IMMERSIVE];
    s_book_word_spacing=(int8_t)work->word_spacing.encoded;
    s_book_tracking = f[BK_TRACKING];
    s_book_indent = indent;
    s_book_indent_adjust = work->adjust.encoded;
    s_book_reading_line = f[BK_READING_LINE];
    s_book_rule_offset = rule_offset;
    s_book_line = f[BK_LINE_SPACING];
    s_book_margin = f[BK_MARGIN];
    s_book_para = f[BK_PARAGRAPH];
    s_shelf_style = f[BK_SHELF];
    strlcpy(s_book_background, work->background.path, sizeof(s_book_background));
    s_home_component = (app_home_component_t)work->home.component;
    s_home_goal_minutes = work->home.minutes;
    strlcpy(s_books_dir,work->backup.books_dir,sizeof(s_books_dir));
    strlcpy(s_fonts_dir,work->backup.fonts_dir,sizeof(s_fonts_dir));
    strlcpy(s_font, work->backup.font, sizeof(s_font));
    strlcpy(s_system_font, work->backup.system_font, sizeof(s_system_font));
    strlcpy(s_wallpaper, work->backup.wallpaper, sizeof(s_wallpaper));
    strlcpy(s_bookmark,work->bookmark.path,sizeof(s_bookmark));
    if(version>=22)reader_backup_apply(&work->reader);
    if(version>=23)memcpy(s_home_utilities,work->utilities.actions,sizeof(s_home_utilities));
    if(version>=24)s_home_utility_labels=utility_labels;
    if(version>=25)s_home_utility_border=utility_border;
    if(version>=29)for(unsigned i=0;i<2;++i)s_simple_utilities[i]=(work->identity.reserved[1]>>(i*4))&15;
    if(version>=30){s_simple_labels=!(work->identity.reserved[0]&2);s_simple_border=!(work->identity.reserved[0]&4);}
    if (version >= 31) {
        for (unsigned i = 0; i < sizeof(s_account_backup_fields)/sizeof(s_account_backup_fields[0]); ++i) {
            const account_backup_field_t *field = &s_account_backup_fields[i];
            strlcpy(field->state, (const char *)&work->accounts + field->offset, field->capacity);
        }
        s_kosync_auto = work->accounts.kosync_auto != 0;
        s_book_footer_enabled = work->accounts.footer_enabled != 0;
    }
    if (version >= 32) {
        err = preferences_apply(&work->preferences);
        if (err != ESP_OK) return err;
    }
    if (has_history) {
        file = fopen(source, "rb");
        if (!file) { backup_erase_secret(&work->wifi, sizeof(work->wifi)); return ESP_FAIL; }
        if (setvbuf(file, (char *)work->io, _IOFBF, sizeof(work->io)) != 0) {
            fclose(file); return ESP_FAIL;
        }
        esp_err_t history_err = fseek(file, history_position, SEEK_SET) == 0
            ? book_history_backup_restore(file) : ESP_FAIL;
        if (fclose(file) != 0 && history_err == ESP_OK) history_err = ESP_FAIL;
        if (history_err != ESP_OK) { backup_erase_secret(&work->wifi, sizeof(work->wifi)); return history_err; }
    }
    if (has_wifi) {
        esp_err_t wifi_err;
#if defined(ESP_PLATFORM) && DPREADER_BOARD_METALIO47
        if (work->preferences.networks_present)
            wifi_err = read_pico_transfer_import_wifi_profiles_backup(work->preferences.networks);
        else
#endif
            wifi_err = read_pico_transfer_import_wifi_backup(&work->wifi.credentials);
        backup_erase_secret(&work->wifi, sizeof(work->wifi));
        if (wifi_err != ESP_OK) return wifi_err;
    }
    if (require_device) {
        // Read back the reader settings before committing the completion journal.
        esp_err_t verify = nvs_open(NVS_NS, NVS_READONLY, &h);
        if (verify != ESP_OK) return verify;
        uint8_t px = 0;
        verify = nvs_get_u8(h, NVS_KEY_BOOK_PX, &px);
        if (verify == ESP_OK && px != work->backup.flags[BK_BOOK_PX]) verify = ESP_ERR_INVALID_RESPONSE;
        size_t size = sizeof(work->io);
        if (verify == ESP_OK) verify = nvs_get_str(h, NVS_KEY_FONT, (char *)work->io, &size);
        if (verify == ESP_OK && strcmp((char *)work->io, work->backup.font)) verify = ESP_ERR_INVALID_RESPONSE;
        size = sizeof(work->io);
        if (verify == ESP_OK) verify = nvs_get_str(h, NVS_KEY_SYS_FONT, (char *)work->io, &size);
        if (verify == ESP_OK && strcmp((char *)work->io, work->backup.system_font)) verify = ESP_ERR_INVALID_RESPONSE;
        nvs_close(h);
        if (verify != ESP_OK) return verify;
#if defined(ESP_PLATFORM) && DPREADER_BOARD_METALIO47
        if (work->preferences.networks_present) {
            read_pico_transfer_wifi_backup_t networks[READ_PICO_TRANSFER_WIFI_PROFILES_MAX];
            verify = read_pico_transfer_export_wifi_profiles_backup(networks);
            if (verify == ESP_OK && memcmp(networks, work->preferences.networks, sizeof(networks)))
                verify = ESP_ERR_INVALID_RESPONSE;
            backup_erase_secret(networks, sizeof(networks));
            return verify;
        }
#endif
        if (has_wifi) {
            verify = read_pico_transfer_export_wifi_backup(&work->wifi.credentials);
            if (verify != ESP_OK || backup_wifi_checksum(&work->wifi) != work->wifi_hash)
                return verify != ESP_OK ? verify : ESP_ERR_INVALID_RESPONSE;
        }
    }
    return ESP_OK;
}

esp_err_t app_settings_backup_restore(void) {
    if (!backup_card_ready()) return ESP_ERR_INVALID_STATE;
    backup_workspace_t *work = backup_alloc(sizeof(*work));
    if (!work) return ESP_ERR_NO_MEM;
    const char *source = BACKUP_FILE;
    FILE *probe = fopen(source, "rb");
    if (probe) fclose(probe); else source = BACKUP_PREVIOUS;
#if CONFIG_DPREADER_AUTO_RESTORE
    esp_err_t err = backup_restore_workspace(work, source, false, false);
    if (err == ESP_OK) err = backup_restore_mark(1);
    if (err == ESP_OK) {
        backup_erase_secret(work, sizeof(*work));
        err = backup_restore_workspace(work, source, true, false);
    }
#else
    esp_err_t err = backup_restore_workspace(work, source, true, false);
#endif
#if CONFIG_DPREADER_AUTO_BACKUP
    // A failed multi-store restore must not replace its source at the next checkpoint.
    if (err != ESP_OK && work->restore_started) s_auto_backup.failed = true;
#endif
    backup_erase_secret(work, sizeof(*work)); free(work);
    if (err == ESP_OK) return backup_rearm_after_explicit_success();
    return err;
}

bool app_settings_autorestore_pending(void) {
#if CONFIG_DPREADER_AUTO_RESTORE
    return s_auto_backup.restore_pending && !s_auto_backup.boot_checked;
#else
    return false;
#endif
}
app_restore_status_t app_settings_autorestore_status(void) {
#if CONFIG_DPREADER_AUTO_RESTORE
    return s_auto_backup.restore_status;
#else
    return APP_RESTORE_NORMAL;
#endif
}
esp_err_t app_settings_autorestore_boot(void) {
#if CONFIG_DPREADER_AUTO_RESTORE
    if (s_auto_backup.boot_checked) return ESP_ERR_INVALID_STATE;
    s_auto_backup.boot_checked = true;
    if (!s_auto_backup.restore_pending || s_auto_backup.failed)
        return s_auto_backup.restore_status == APP_RESTORE_NORMAL ? ESP_OK : ESP_ERR_INVALID_STATE;
    if (!backup_card_ready()) {
        s_auto_backup.restore_status = APP_RESTORE_NEED_MEDIA;
        return ESP_ERR_INVALID_STATE;
    }
    bool library_empty = false;
    esp_err_t library_err = book_history_backup_empty(&library_empty);
    if (library_err != ESP_OK || !library_empty) {
        (void)backup_restore_mark(3);
        s_auto_backup.restore_status = APP_RESTORE_BLOCKED;
        return library_err != ESP_OK ? library_err : ESP_ERR_INVALID_STATE;
    }
    backup_workspace_t *work = backup_alloc(sizeof(*work));
    if (!work) {
        (void)backup_restore_mark(3);
        s_auto_backup.restore_pending = false;
        s_auto_backup.restore_status = APP_RESTORE_BLOCKED;
        return ESP_ERR_NO_MEM;
    }
    const char *source = BACKUP_FILE;
    esp_err_t err = backup_restore_workspace(work, source, false, true);
    if (err != ESP_OK) {
        backup_erase_secret(work, sizeof(*work));
        source = BACKUP_PREVIOUS;
        err = backup_restore_workspace(work, source, false, true);
    }
    if (err == ESP_OK) err = backup_restore_mark(1);
    if (err == ESP_OK) {
        backup_erase_secret(work, sizeof(*work));
        err = backup_restore_workspace(work, source, true, true);
    }
    backup_erase_secret(work, sizeof(*work)); free(work);
    if (err == ESP_OK) err = backup_restore_mark(2);
    if (err != ESP_OK) {
        (void)backup_restore_mark(3);
        s_auto_backup.healthy = false;
        s_auto_backup.failed = true;
        s_auto_backup.restore_pending = false;
        s_auto_backup.restore_status = APP_RESTORE_BLOCKED;
        ESP_LOGW(TAG, "automatic restore blocked: %s", esp_err_to_name(err));
        return err;
    }
    app_settings_init();
    s_auto_backup.boot_checked = true;
    s_auto_backup.restore_status = s_auto_backup.healthy && !s_auto_backup.failed ?
        APP_RESTORE_DONE : APP_RESTORE_BLOCKED;
    return s_auto_backup.restore_status == APP_RESTORE_DONE ? ESP_OK : ESP_ERR_INVALID_STATE;
#else
    return ESP_ERR_INVALID_STATE;
#endif
}

static bool reader_edge_setting(bool *value,const char *key,bool on) {
    if(*value==on)return true;
    nvs_handle_t h;
    if(backup_nvs_open(NVS_NS,NVS_READWRITE,&h)!=ESP_OK)return false;
    esp_err_t err=backup_nvs_set_u8(h,key,on?1:0);
    if(err==ESP_OK)err=settings_backup_commit(h);
    nvs_close(h);if(err==ESP_OK)*value=on;
    return err==ESP_OK;
}
bool app_settings_reader_edge_brightness(void){return s_reader_edge_brightness;}
bool app_settings_set_reader_edge_brightness(bool on){return reader_edge_setting(&s_reader_edge_brightness,"r_edge_light",on);}
bool app_settings_reader_edge_warmth(void){return s_reader_edge_warmth;}
bool app_settings_set_reader_edge_warmth(bool on){return reader_edge_setting(&s_reader_edge_warmth,"r_edge_warm",on);}

bool app_settings_book_footer_enabled(void) { return s_book_footer_enabled; }
bool app_settings_set_book_footer_enabled(bool on) {
    return reader_edge_setting(&s_book_footer_enabled, "rd_footer_on", on);
}

bool app_settings_book_status_enabled(void) { return !s_book_status_hidden; }
bool app_settings_set_book_status_enabled(bool on) {
    return reader_edge_setting(&s_book_status_hidden, "rd_status_off", !on);
}
