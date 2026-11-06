/* SPDX-License-Identifier: Apache-2.0
 * S31手动挂卡恢复：先释放消费者，再重挂；不格式化，不更改保存的配置。
 * S31 manual recovery closes consumers before remounting, without formatting or changing saved settings.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

typedef enum {
    APP_SD_RECOVERY_IDLE, APP_SD_RECOVERY_PREPARE, APP_SD_RECOVERY_WAIT,
    APP_SD_RECOVERY_MOUNT, APP_SD_RECOVERY_FONTS,
    APP_SD_RECOVERY_DONE, APP_SD_RECOVERY_FAILED,
} app_sd_recovery_phase_t;
typedef struct {
    app_sd_recovery_phase_t phase;
    esp_err_t error;
    int fonts;
    bool selected_font_ready;
} app_sd_recovery_status_t;

/// Only start from the settings page after leaving the reader and other file-owning pages.
esp_err_t app_sd_recovery_begin(int64_t now_ms);
bool app_sd_recovery_busy(void);
app_sd_recovery_status_t app_sd_recovery_status(void);
/// UI-thread polling; false means no display change. Never cancel an in-flight SD operation.
bool app_sd_recovery_tick(int64_t now_ms);
