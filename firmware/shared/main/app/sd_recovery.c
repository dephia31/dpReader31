/* SPDX-License-Identifier: Apache-2.0 */
#include "sd_recovery.h"
#include <string.h>
#include "read_pico_sd.h"
#include "read_pico_transfer.h"
#include "usb_storage.h"
#include "ota_online.h"
#include "online_font.h"
#include "sleep.h"
#include "book_cover_jobs.h"
#include "book_source.h"
#include "book_store.h"
#include "book_lock_collage.h"
#include "metalio_cover_cache.h"
#include "ui_avatar.h"
#include "app_font_context.h"
#include "ttf_font.h"
#include "settings.h"
#include "ui_kit.h"

static app_sd_recovery_status_t s_status;
static int64_t s_deadline_ms;

bool app_sd_recovery_busy(void) {
    return s_status.phase >= APP_SD_RECOVERY_PREPARE && s_status.phase <= APP_SD_RECOVERY_FONTS;
}
app_sd_recovery_status_t app_sd_recovery_status(void) { return s_status; }

static bool owner_busy(void) {
    read_pico_transfer_status_t transfer = {0};
    online_font_status_t font = {0};
    read_pico_transfer_get_status(&transfer);
    online_font_status(&font);
    // Even an idle HTTP server can accept a new request. Its page must stop/join it first.
    return usb_storage_active() || app_download_active() || pico_online_busy() || font.busy ||
        transfer.state != READ_PICO_TRANSFER_STOPPED;
}
esp_err_t app_sd_recovery_begin(int64_t now_ms) {
    if (app_sd_recovery_busy() || owner_busy()) return ESP_ERR_INVALID_STATE;
    s_status = (app_sd_recovery_status_t){.phase=APP_SD_RECOVERY_PREPARE};
    s_deadline_ms = now_ms + 5000;
    return ESP_OK;
}
static bool fail(esp_err_t error) {
    s_status.phase = APP_SD_RECOVERY_FAILED;
    s_status.error = error;
    return true;
}
bool app_sd_recovery_tick(int64_t now_ms) {
    if (!app_sd_recovery_busy()) return false;
    if (s_status.phase == APP_SD_RECOVERY_PREPARE) {
        if (owner_busy()) return fail(ESP_ERR_INVALID_STATE);
        bool covers = book_cover_jobs_quiesce(0), avatar = ui_avatar_quiesce(0);
        if (!covers || !avatar) {
            if (now_ms >= s_deadline_ms) return fail(ESP_ERR_TIMEOUT);
            return false;
        }
        // Settings is active: reader exit has already joined pre-rendering and saved progress.
        // Drop every font slot, including suspended descriptors, before unmounting the old VFS.
        book_close();
        ttf_font_close_embedded();
        ttf_font_unload();
        app_sleep_discard_media_io();
        ui_text_set_system_font(false);
        (void)ttf_font_open_builtin();
        ui_avatar_invalidate();
        book_lock_collage_invalidate();
        metalio_cover_cache_clear();
        book_store_notify_changed();
        s_status.phase = APP_SD_RECOVERY_WAIT;
        s_deadline_ms = now_ms + 10000;
        return true;
    }
    if (s_status.phase == APP_SD_RECOVERY_WAIT) {
        esp_err_t error = read_pico_sd_start_manual_recovery();
        if (error == ESP_ERR_NOT_FINISHED) {
            if (now_ms >= s_deadline_ms) return fail(ESP_ERR_TIMEOUT);
            return false;
        }
        if (error != ESP_OK) return fail(error);
        s_status.phase = APP_SD_RECOVERY_MOUNT;
        return true;
    }
    if (s_status.phase == APP_SD_RECOVERY_MOUNT) {
        read_pico_sd_info_t info = {0};
        esp_err_t error = read_pico_sd_get_info(&info);
        // A timeout here must never tear down a live SDK mount task or its DMA buffer.
        if (error == ESP_ERR_NOT_FINISHED) return false;
        if (error != ESP_OK || !info.mounted) return fail(error != ESP_OK ? error : ESP_FAIL);
        s_status.phase = APP_SD_RECOVERY_FONTS;
        return true;
    }
    s_status.fonts = ttf_font_scan();
    (void)app_font_recover_media();
    const char *chosen = app_settings_system_font_path();
    s_status.selected_font_ready = !chosen[0] ||
        (ttf_font_ready() && !strcmp(chosen, ttf_font_path()));
    s_status.phase = APP_SD_RECOVERY_DONE;
    return true;
}
