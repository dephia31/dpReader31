/* SPDX-License-Identifier: Apache-2.0
 * 中文：S3 RTC 离线走时；在线每小时校准，离线每六小时短连已保存 WiFi。
 * English: S3 RTC keeps offline time; calibrate hourly online or briefly every six hours offline.
 */
#include "time_service.h"
#include <stdatomic.h>
#include <stdint.h>
#include <time.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include "read_pico_transfer.h"
#include "sleep.h"

#define CLOCK_VALID_MIN 1704067200ULL
#define CLOCK_VALID_MAX 4102444799ULL
#define CLOCK_ONLINE_INTERVAL 3600ULL
#define CLOCK_OFFLINE_INTERVAL (6ULL * 3600)
#define CLOCK_RETRY_US (5LL * 60 * 1000000)
static atomic_bool s_running;
static bool s_loaded;
static uint64_t s_last_sync;
static int64_t s_next_attempt_us = 15000000;

static void clock_worker(void *arg) {
    (void)arg;
    uint32_t utc = 0;
    esp_err_t err = read_pico_transfer_sync_time_auto(&utc);
    ESP_LOGI("clock", "background calibration: %s", esp_err_to_name(err));
    atomic_store(&s_running, false);
    vTaskDelete(NULL);
}

void app_time_service_poll(bool idle) {
    if (!idle) return;
    if (!s_loaded) {
        nvs_handle_t handle;
#if KIIKO_BOARD_METALIO47
        if (nvs_open("mt_clock", NVS_READONLY, &handle) == ESP_OK) {
#else
        if (nvs_open("kiiko_clock", NVS_READONLY, &handle) == ESP_OK) {
#endif
            if (nvs_get_u64(handle, "last_ntp", &s_last_sync) != ESP_OK) s_last_sync = 0;
            nvs_close(handle);
        }
        if (s_last_sync < CLOCK_VALID_MIN || s_last_sync > CLOCK_VALID_MAX) s_last_sync = 0;
        s_loaded = true;
    }
    uint32_t received;
    if (read_pico_transfer_take_time_sync(&received)) {
        // 只有真实网络成功才持久化；普通重启/手动设时间不能伪装成网络校准。
        // Persist only a real network success; reboot/manual edits do not count as calibration.
        s_last_sync = received;
        (void)app_sync_time_to_pmu();
        nvs_handle_t handle;
#if KIIKO_BOARD_METALIO47
        if (nvs_open("mt_clock", NVS_READWRITE, &handle) == ESP_OK) {
#else
        if (nvs_open("kiiko_clock", NVS_READWRITE, &handle) == ESP_OK) {
#endif
            if (nvs_set_u64(handle, "last_ntp", s_last_sync) == ESP_OK) (void)nvs_commit(handle);
            nvs_close(handle);
        }
    }
    const int64_t monotonic = esp_timer_get_time();
    if (atomic_load(&s_running) || monotonic < s_next_attempt_us) return;
    read_pico_transfer_status_t status;
    read_pico_transfer_get_status(&status);
    bool online = status.state == READ_PICO_TRANSFER_READY &&
                  status.mode == READ_PICO_TRANSFER_MODE_STA && status.network_ready;
    if (!online) return; // Mặc định tắt; chỉ đồng bộ khi người dùng hoặc tác vụ đã kết nối WiFi sẵn
    time_t now = time(NULL);
    if (now >= (time_t)s_last_sync && s_last_sync && (uint64_t)now - s_last_sync < CLOCK_ONLINE_INTERVAL) return;
    s_next_attempt_us = monotonic + CLOCK_RETRY_US;
    atomic_store(&s_running, true);
    // 仅对时期间保留任务栈；不创建常驻无线任务或页面缓存。
    // Keep a task stack only during calibration; no permanent radio task or page cache.
    if (xTaskCreate(clock_worker, "rtc_sync", 6144, NULL, 1, NULL) != pdPASS) {
        atomic_store(&s_running, false);
        ESP_LOGW("clock", "background calibration deferred: no task memory");
    }
}
