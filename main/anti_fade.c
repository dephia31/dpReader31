/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 防褪色重刷间隔的 NVS 存取。V1.5 没有 Pico PMU，调试参数统一放在
 * metalio_hw 命名空间（VCOM 也在这里），不参与设置备份格式。
 *
 * NVS storage for the anti-fade refresh interval. The V1.5 board has no Pico PMU,
 * so debug parameters live in the metalio_hw namespace (VCOM is there too) and do
 * not take part in the settings backup format.
 */

#include "anti_fade.h"

#include "nvs.h"

#define AF_NAMESPACE "metalio_hw"
#define AF_KEY "antifade_min"
#define AF_DEFAULT_MINUTES 5

static bool af_allowed(unsigned minutes) {
    return minutes == 0 || minutes == 1 || minutes == 3 || minutes == 5 || minutes == 10
        || minutes == 30;
}

unsigned anti_fade_minutes(void) {
    nvs_handle_t handle;
    uint8_t value = 0;
    if (nvs_open(AF_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) return AF_DEFAULT_MINUTES;
    const esp_err_t err = nvs_get_u8(handle, AF_KEY, &value);
    nvs_close(handle);
    if (err != ESP_OK || !af_allowed(value)) return AF_DEFAULT_MINUTES;
    return value;
}

void anti_fade_set_minutes(unsigned minutes) {
    if (!af_allowed(minutes)) return;
    nvs_handle_t handle;
    if (nvs_open(AF_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK) return;
    (void)nvs_set_u8(handle, AF_KEY, (uint8_t)minutes);
    (void)nvs_commit(handle);
    nvs_close(handle);
}
