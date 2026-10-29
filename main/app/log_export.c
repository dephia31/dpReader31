/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：捕获日志并导出SD诊断文件；文件I/O必须在环形缓冲锁外。
 * English: Capture logs and export SD diagnostics; keep file I/O outside the ring-buffer lock.
 */

#include "log_export.h"

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>

#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_app_format.h"
#include "esp_ota_ops.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "read_pico_sd.h"
#include "read_pico_crashlog.h"
#include "read_pico_pmu.h"
#include "ble_page_turner.h"
#include "crash_note.h"
#include "settings.h"

#define LOG_BUF_SIZE (32 * 1024)

#if KIIKO_BOARD_METALIO47 && defined(ESP_PLATFORM)
#include "esp_attr.h"
#include "esp_private/cache_utils.h"
// PSRAM在app_main前初始化；cache关闭时不捕获/读取外部环形缓冲。
// PSRAM is ready before app_main; do not capture/read the external ring with cache suspended.
static EXT_RAM_BSS_ATTR char s_log_buf[LOG_BUF_SIZE];
static bool log_ring_accessible(void) { return spi_flash_cache_enabled(); }
#else
static char s_log_buf[LOG_BUF_SIZE];
static bool log_ring_accessible(void) { return true; }
#endif
static size_t s_log_head = 0;
static size_t s_log_total_written = 0;
static portMUX_TYPE s_log_lock = portMUX_INITIALIZER_UNLOCKED;
static vprintf_like_t s_prev_vprintf = NULL;
static bool s_in_vprintf = false;

static int log_export_vprintf(const char *fmt, va_list args) {
    if (!fmt) return 0;

    // Call through to previous vprintf first so serial/USB console is uninterrupted
    va_list args_copy;
    va_copy(args_copy, args);
    int ret = 0;
    if (s_prev_vprintf) {
        ret = s_prev_vprintf(fmt, args);
    } else {
        ret = vprintf(fmt, args);
    }

    // Guard against recursive calls
    if (s_in_vprintf || !log_ring_accessible()) {
        va_end(args_copy);
        return ret;
    }
    s_in_vprintf = true;

    char temp[256];
    int len = vsnprintf(temp, sizeof(temp), fmt, args_copy);
    va_end(args_copy);

    if (len > 0) {
        if ((size_t)len >= sizeof(temp)) len = (int)(sizeof(temp) - 1);

        portENTER_CRITICAL(&s_log_lock);
        for (int i = 0; i < len; ++i) {
            s_log_buf[s_log_head] = temp[i];
            s_log_head = (s_log_head + 1) % LOG_BUF_SIZE;
        }
        s_log_total_written += (size_t)len;
        portEXIT_CRITICAL(&s_log_lock);
    }

    s_in_vprintf = false;
    return ret;
}

void log_export_init(void) {
    if (s_prev_vprintf == NULL) {
        s_prev_vprintf = esp_log_set_vprintf(log_export_vprintf);
        ESP_LOGI("log_export", "Log capture initialized (buffer size=%u KB)", (unsigned)(LOG_BUF_SIZE / 1024));
    }
}

int log_export_get_recent(char *out, size_t max_len) {
    if (!out || max_len == 0) return 0;
    if(!log_ring_accessible()){out[0]=0;return 0;}
    out[0] = 0;

    portENTER_CRITICAL(&s_log_lock);
    size_t count = s_log_total_written < LOG_BUF_SIZE ? s_log_total_written : LOG_BUF_SIZE;
    if (count == 0) {
        portEXIT_CRITICAL(&s_log_lock);
        return 0;
    }

    size_t take = count;
    if (take >= max_len) take = max_len - 1;

    // Read backward or from head - take
    size_t start = (s_log_head + LOG_BUF_SIZE - take) % LOG_BUF_SIZE;
    for (size_t i = 0; i < take; ++i) {
        out[i] = s_log_buf[(start + i) % LOG_BUF_SIZE];
    }
    out[take] = 0;
    portEXIT_CRITICAL(&s_log_lock);

    return (int)take;
}

void log_export_clear(void) {
    if(!log_ring_accessible())return;
    portENTER_CRITICAL(&s_log_lock);
    s_log_head = 0;
    s_log_total_written = 0;
    memset(s_log_buf, 0, sizeof(s_log_buf));
    portEXIT_CRITICAL(&s_log_lock);
}

// 锁内只复制固定大小快照，不执行SD/日志调用。/ Copy a bounded snapshot under the lock, without SD or logging calls.
static size_t log_snapshot(char *out) {
    if(!log_ring_accessible())return 0;
    portENTER_CRITICAL(&s_log_lock);
    size_t count=s_log_total_written<LOG_BUF_SIZE?s_log_total_written:LOG_BUF_SIZE;
    size_t start=(s_log_head+LOG_BUF_SIZE-count)%LOG_BUF_SIZE;
    size_t first=count<LOG_BUF_SIZE-start?count:LOG_BUF_SIZE-start;
    memcpy(out,s_log_buf+start,first);
    memcpy(out+first,s_log_buf,count-first);
    portEXIT_CRITICAL(&s_log_lock);
    return count;
}

esp_err_t log_export_to_sd(char *out_path, size_t out_path_size, char *err_msg, size_t err_msg_size) {
    read_pico_sd_info_t sd = {0};
    esp_err_t err = read_pico_sd_get_info(&sd);
    if (err != ESP_OK || !sd.mounted) {
        if (err_msg && err_msg_size) {
            snprintf(err_msg, err_msg_size, "Chưa lắp thẻ nhớ hoặc thẻ nhớ lỗi");
        }
        return ESP_ERR_INVALID_STATE;
    }

    const char *target = "/sdcard/dpreader_log.txt";
    if (out_path && out_path_size) {
        snprintf(out_path, out_path_size, "%s", target);
    }

    // 不使用32KB任务栈数组；内存不足时返回错误，不能进入持锁写文件路径。
    // Avoid a 32KB task-stack array; return an allocation error rather than writing while locked.
    char *snapshot=heap_caps_malloc(LOG_BUF_SIZE,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!snapshot)snapshot=heap_caps_malloc(LOG_BUF_SIZE,MALLOC_CAP_8BIT);
    if(!snapshot){
        if(err_msg&&err_msg_size)snprintf(err_msg,err_msg_size,"Không đủ bộ nhớ để xuất log");
        return ESP_ERR_NO_MEM;
    }
    size_t snapshot_size=log_snapshot(snapshot);
    FILE *f = fopen(target, "w");
    if (!f) {
        heap_caps_free(snapshot);
        if (err_msg && err_msg_size) {
            snprintf(err_msg, err_msg_size, "Không thể tạo file trên thẻ nhớ");
        }
        return ESP_FAIL;
    }

    // 1. Header & Version
    const esp_app_desc_t *app_desc = esp_app_get_description();
    fprintf(f, "========================================================\n");
    fprintf(f, "dpReader System & Diagnostic Log\n");
    fprintf(f, "Board: Metalio E-Ink4 Plus (ESP32-S31)\n");
    if (app_desc) {
        fprintf(f, "Firmware: %s\n", app_desc->version);
        fprintf(f, "Project: %s\n", app_desc->project_name);
        fprintf(f, "IDF Version: %s\n", app_desc->idf_ver);
        fprintf(f, "Build Date/Time: %s %s\n", app_desc->date, app_desc->time);
    }
    time_t now_sec = time(NULL);
    struct tm ti;
    localtime_r(&now_sec, &ti);
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", &ti);
    fprintf(f, "Export Time: %s (UTC+7)\n", time_str);
    fprintf(f, "Uptime: %lu ms\n", (unsigned long)(esp_timer_get_time() / 1000));
    fprintf(f, "========================================================\n\n");

    // 2. Hardware & Memory
    fprintf(f, "[1. HARDWARE & MEMORY]\n");
    fprintf(f, "Free Internal Heap: %u bytes\n", (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    fprintf(f, "Min Free Internal Heap: %u bytes\n", (unsigned)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL));
    fprintf(f, "Total PSRAM: %u bytes\n", (unsigned)heap_caps_get_total_size(MALLOC_CAP_SPIRAM));
    fprintf(f, "Free PSRAM: %u bytes\n", (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

    const pmu_snapshot_t *pmu = read_pico_pmu_get();
    if (pmu) {
        int pct = pmu_battery_percent(pmu);
        bool chg = pmu_battery_charging(pmu);
        fprintf(f, "Battery: %d%% (%u mV), Charging: %s\n",
                pct >= 0 ? pct : 0, (unsigned)pmu->battery_mv, chg ? "YES" : "NO");
    } else {
        fprintf(f, "Battery: No PMU snapshot\n");
    }
    fprintf(f, "\n");

    // 3. Storage
    fprintf(f, "[2. STORAGE]\n");
    fprintf(f, "SD Card Capacity: %llu MB\n", (unsigned long long)(sd.capacity_bytes / (1024 * 1024)));
    if (sd.space_valid) {
        fprintf(f, "SD Card Free: %llu MB\n", (unsigned long long)(sd.free_bytes / (1024 * 1024)));
    }
    fprintf(f, "\n");

    // 4. Bluetooth Page Turner
    fprintf(f, "[3. BLUETOOTH PAGE TURNER]\n");
    fprintf(f, "Enabled in settings: %s\n", app_settings_ble_turner() ? "YES" : "NO");
    fprintf(f, "BLE Stack Running: %s\n", ble_pt_running() ? "YES" : "NO");
    fprintf(f, "BLE Connected: %s\n", ble_pt_connected() ? "YES" : "NO");
    if (ble_pt_connected()) {
        fprintf(f, "Connected Device: %s\n", ble_pt_connected_name());
    }
    fprintf(f, "Bonded Devices: %u\n", (unsigned)ble_pt_bond_count());
    for (uint8_t i = 0; i < ble_pt_bond_count(); ++i) {
        const ble_pt_bond_t *b = ble_pt_bond(i);
        if (b) {
            fprintf(f, "  [%u] %s (%s)\n", i, b->name, b->addr);
        }
    }
    fprintf(f, "\n");

    // 5. Crash & Panic Records
    fprintf(f, "[4. CRASH & PANIC NOTES]\n");
    if (crash_note_valid()) {
        const char *panic_line = crash_note_text();
        fprintf(f, "Last Panic: %s\n", panic_line ? panic_line : "Unknown");
    } else {
        fprintf(f, "Last Panic: None (System clean)\n");
    }

    uint8_t crash_count = rp_crash_log_count();
    fprintf(f, "RTC Crash/Boot Trail (%u lines):\n", crash_count);
    for (uint8_t i = 0; i < crash_count; ++i) {
        const char *line = rp_crash_log_line(i);
        if (line) {
            fprintf(f, "  [%u] %s\n", i, line);
        }
    }
    fprintf(f, "\n");

    // 6. Ring Buffer System Logs
    fprintf(f, "[5. SYSTEM RUNTIME LOG BUFFER (LAST %u KB)]\n", (unsigned)(LOG_BUF_SIZE / 1024));
    fprintf(f, "--------------------------------------------------------\n");

    bool payload_failed=snapshot_size && fwrite(snapshot,1,snapshot_size,f)!=snapshot_size;

    fprintf(f, "\n--------------------------------------------------------\n");
    fprintf(f, "=== END OF DIAGNOSTIC LOG ===\n");

    bool failed=payload_failed || ferror(f)!=0;
    if(fflush(f)!=0 || ferror(f))failed=true;
    if(fclose(f)!=0)failed=true;
    heap_caps_free(snapshot);
    if(failed){
        if(err_msg&&err_msg_size)snprintf(err_msg,err_msg_size,"Không thể ghi đầy đủ log lên thẻ nhớ");
        return ESP_FAIL;
    }

    ESP_LOGI("log_export", "System logs exported successfully to %s", target);
    return ESP_OK;
}

