/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 锁屏、浅睡等待、软睡/关机下电。
 *
 * Lock, light-sleep wait, and soft-sleep / power-off rail drop.
 */

#pragma once

#include <stdint.h>

#include "epd_highlevel.h"
#include "esp_err.h"
#include "sc7a20h.h"
#include "cst836u.h"
#include "settings.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    APP_WAKE_NONE = 0,
    APP_WAKE_KEY,
    APP_WAKE_PICKUP,
    APP_WAKE_TIMEOUT,
} app_wake_source_t;

/// 主动拼贴锁屏先灭灯并显示圆锁，保留后续短按。/ Manual collage feedback retains later wake clicks.
void app_lock_feedback(EpdiyHighlevelState *hl);
/// 深睡必须已有成功保存的恢复点。/ Require a persisted resume checkpoint before lock deep sleep.
void app_lock_set_resume_ready(bool ready);

void enter_lock_and_sleep(
    EpdiyHighlevelState* hl, int64_t* ignore_until_ms, sc7a20h_handle_t acc, cst836u_handle_t touch,
    bool reader_background
);

/// 在消费恢复/开书前进行冷启动与深睡密码验证。/ Gate cold/deep starts before consuming resume or opening books.
void app_lock_boot_gate(EpdiyHighlevelState *hl, cst836u_handle_t touch, sc7a20h_handle_t acc);

/// 等电源键松开，避免进睡瞬间被同一下按住立刻唤醒。
/// Wait for the power key to release so the same press does not wake immediately.
void app_lock_wait_key_idle(int timeout_ms);
// 中文：test21暂停自动深睡；0表示保留书籍/字体/PIN内存等待真实唤醒。
// English: test21 suspends automatic deep sleep; zero retains book/font/PIN context until real input.
#define APP_LOCK_IDLE_TIMEOUT_MS 0U
/// 按所选省电方式等待按键；拿起/USB保留待机；acc为空则仅按键。
/// Use the selected lock policy; pickup/USB retain standby, NULL acc accepts keys only.
app_wake_source_t app_light_sleep_wait(sc7a20h_handle_t acc);
/// 等待按键、拿起或超时；0不限。默认保留TF；显式浅睡临时暂停/恢复句柄。
/// Zero means no deadline. Default retains TF; explicit light sleep pauses/reopens its handles.
app_wake_source_t app_light_sleep_wait_timed(sc7a20h_handle_t acc, uint32_t timeout_ms);
app_wake_source_t app_last_wake_source(void);
/// S31深睡准入失败时返回，调用方必须继续锁屏；成功深睡或关机不会返回。
/// S31 deep-sleep refusal returns and callers must remain locked; admitted sleep/off does not return.
esp_err_t app_enter_host_sleep(app_sleep_mode_t mode);
/// 将当前系统时间写入常供电 PMU RTC；主控断电后由 PMU 继续走时。
/// Sync system time to the always-on PMU RTC so it keeps ticking while the host is off.
esp_err_t app_sync_time_to_pmu(void);
/// 让 PMU 正常重启主控；先保存 RTC，调用后不会返回。
/// Ask the PMU for a normal host restart after preserving RTC; does not return.
void app_restart_host(void);
/// 准备S31下载口，等待电脑发起USJ硬件复位；不关闭字体/TF/显示。
/// Prepare the S31 download port for host-triggered USJ reset; retain fonts/TF/display.
esp_err_t app_enter_download(void);
bool app_download_active(void);
void app_cancel_download(void);

#ifdef __cplusplus
}
#endif
