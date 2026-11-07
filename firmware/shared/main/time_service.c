/* SPDX-License-Identifier: Apache-2.0
 * 中文：RTC 离线走时；S31每日仅在已有联网时校准，跳过后不重试。
 * English: RTC keeps offline time; S31 calibrates daily on existing WiFi, without retries.
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
#if DPREADER_BOARD_METALIO47
#define CLOCK_ONLINE_INTERVAL 86400ULL
#define CLOCK_RETRY_US (86400LL * 1000000)
static uint64_t s_last_attempt;
#else
#define CLOCK_ONLINE_INTERVAL 3600ULL
#define CLOCK_RETRY_US (5LL * 60 * 1000000)
#endif
#define CLOCK_OFFLINE_INTERVAL (6ULL * 3600)
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
#if DPREADER_BOARD_METALIO47
        if (nvs_open("mt_clock", NVS_READONLY, &handle) == ESP_OK) {
#else
        if (nvs_open("kiiko_clock", NVS_READONLY, &handle) == ESP_OK) {
#endif
            if (nvs_get_u64(handle, "last_ntp", &s_last_sync) != ESP_OK) s_last_sync = 0;
#if DPREADER_BOARD_METALIO47
            if (nvs_get_u64(handle, "last_try", &s_last_attempt) != ESP_OK) s_last_attempt = 0;
#endif
            nvs_close(handle);
        }
#if DPREADER_BOARD_METALIO47
        if (s_last_attempt < CLOCK_VALID_MIN || s_last_attempt > CLOCK_VALID_MAX) s_last_attempt = 0;
#endif
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
#if DPREADER_BOARD_METALIO47
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
#if DPREADER_BOARD_METALIO47
    time_t checked_at = time(NULL);
    uint64_t last = s_last_sync > s_last_attempt ? s_last_sync : s_last_attempt;
    if (last && checked_at >= (time_t)last && (uint64_t)checked_at - last < CLOCK_ONLINE_INTERVAL) return;
    // Consume today's opportunity even offline or on failure. No reconnect/retry job.
    s_next_attempt_us = monotonic + CLOCK_RETRY_US;
    if (checked_at >= (time_t)CLOCK_VALID_MIN && checked_at <= (time_t)CLOCK_VALID_MAX) {
        s_last_attempt = (uint64_t)checked_at;
        nvs_handle_t handle;
        if (nvs_open("mt_clock", NVS_READWRITE, &handle) == ESP_OK) {
            if (nvs_set_u64(handle, "last_try", s_last_attempt) == ESP_OK) (void)nvs_commit(handle);
            nvs_close(handle);
        }
    }
#endif
    read_pico_transfer_status_t status;
    read_pico_transfer_get_status(&status);
    bool online = status.state == READ_PICO_TRANSFER_READY &&
                  status.mode == READ_PICO_TRANSFER_MODE_STA && status.network_ready;
#if DPREADER_BOARD_METALIO47
    // Automatic calibration borrows an existing connection; never wakes WiFi.
    if(!online)return;
#else
    if(!online && status.state!=READ_PICO_TRANSFER_STOPPED)return;
#endif
    time_t now = time(NULL);
    uint64_t interval=online?CLOCK_ONLINE_INTERVAL:CLOCK_OFFLINE_INTERVAL;
    if (now >= (time_t)s_last_sync && s_last_sync && (uint64_t)now - s_last_sync < interval) return;
    if(!online) {
        char ssid[33];bool configured=false;
        if(read_pico_transfer_get_saved_wifi(ssid,&configured)!=ESP_OK||!configured) {
            s_next_attempt_us=monotonic+CLOCK_RETRY_US;return;
        }
    }
    s_next_attempt_us = monotonic + CLOCK_RETRY_US;
    atomic_store(&s_running, true);
    // 仅对时期间保留任务栈；不创建常驻无线任务或页面缓存。
    // Keep a task stack only during calibration; no permanent radio task or page cache.
    if (xTaskCreate(clock_worker, "rtc_sync", 6144, NULL, 1, NULL) != pdPASS) {
        atomic_store(&s_running, false);
        ESP_LOGW("clock", "background calibration deferred: no task memory");
    }
}
