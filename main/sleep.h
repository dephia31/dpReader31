/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 锁屏、浅睡等待、软睡/关机下电。
 *
 * Lock, light-sleep wait, and soft-sleep / power-off rail drop.
 * 板级：Pico 的待机交给 CW32 PMU；Metalio S31 的待机方式来自锁屏待机设置与 TPS65185。
 * Boards: the Pico hands standby to the CW32 PMU; the Metalio S31 follows its lock standby
 * setting and the TPS65185 rails.
 */

#pragma once

#include <stdint.h>

#include "epd_highlevel.h"
#include "esp_err.h"
#include "sc7a20h.h"
#include "cst836u.h"
#include "settings.h"
#include "kiiko_board.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    APP_WAKE_NONE = 0,
    APP_WAKE_KEY,
    APP_WAKE_PICKUP,
    APP_WAKE_TIMEOUT,
} app_wake_source_t;

/// 登记常开供电的触摸与加速度计，并准备 CPU 浅睡保持区；S31 侧只保存句柄。
/// Bind always-on touch/accelerometer and prepare CPU light-sleep retention; the S31 only keeps
/// the handles.
void app_sleep_bind(cst836u_handle_t tp, sc7a20h_handle_t acc);

/// 主动拼贴锁屏先灭灯并显示圆锁，保留后续短按；S31 锁屏反馈期间的新按键留给唤醒。
/// Manual collage lock lights off and shows the badge first, retaining later clicks; on the S31
/// new presses during feedback are kept for waking.
void app_lock_feedback(EpdiyHighlevelState *hl);
/// 深睡必须已有成功保存的恢复点；S31 的锁屏待机设置靠它决定是否允许深睡。
/// Require a persisted resume checkpoint before lock deep sleep; the S31 lock standby setting
/// uses it to admit deep sleep.
#if KIIKO_BOARD_METALIO47
void app_lock_set_resume_ready(bool ready);
#endif

void enter_lock_and_sleep(
    EpdiyHighlevelState* hl, int64_t* ignore_until_ms, sc7a20h_handle_t acc, cst836u_handle_t touch,
    bool reader_background
);

/// 在消费恢复/开书前进行冷启动与深睡密码验证。
/// Gate cold/deep starts before consuming resume or opening books.
void app_lock_boot_gate(EpdiyHighlevelState *hl, cst836u_handle_t touch, sc7a20h_handle_t acc);

/// 等电源键松开，避免进睡瞬间被同一下按住立刻唤醒。
/// Wait for the power key to release so the same press does not wake immediately.
void app_lock_wait_key_idle(int timeout_ms);
/// 按所选省电方式等待按键；拿起/USB保留待机；acc为空则仅按键。
/// Use the selected lock policy; pickup/USB retain standby, NULL acc accepts keys only.
app_wake_source_t app_light_sleep_wait(sc7a20h_handle_t acc);
/// 等待按键、拿起或超时；0不限。默认保留TF；显式浅睡临时暂停/恢复句柄；超时以 APP_WAKE_TIMEOUT 返回且绝不解锁。
/// Wait for key, pickup or timeout; 0 means no deadline. TF is retained by default and an
/// explicit light sleep pauses/reopens its handles; a timeout returns APP_WAKE_TIMEOUT and
/// never unlocks.
app_wake_source_t app_light_sleep_wait_timed(sc7a20h_handle_t acc, uint32_t timeout_ms);
app_wake_source_t app_last_wake_source(void);
/// 软睡/关机前停用外设并落盘；成功不返回，交接失败恢复外设并返回错误。保留调用方的阅读/密码上下文。
/// Park peripherals and sync before host sleep/off; success does not return, failure restores
/// peripherals and returns an error with reader/PIN context intact.
esp_err_t app_enter_host_sleep(app_sleep_mode_t mode);
/// 将当前系统时间写入常供电 RTC；主控断电后由它继续走时。
/// Sync system time to the always-on RTC so it keeps ticking while the host is off.
esp_err_t app_sync_time_to_pmu(void);
/// 重启主控；先保存 RTC 与卡上状态，调用后不会返回。
/// Restart the host after preserving RTC and card state; does not return.
void app_restart_host(void);

#if KIIKO_BOARD_METALIO47
/// S31 锁屏保留待机：0 表示不设超时，返回 APP_WAKE_TIMEOUT 后不解锁也不重启。
/// S31 retained lock standby: 0 means no deadline, and APP_WAKE_TIMEOUT neither unlocks nor reboots.
#define APP_LOCK_IDLE_TIMEOUT_MS 0U
#endif

/// 准备下载口，等电脑发起硬件复位；只有板上带 USB 路由的 S31 真正交接，Pico 固定不可用。
/// Prepare the download port for a host-triggered reset; only the S31 with its on-board USB
/// route hands off, the Pico stays fixed-unavailable.
#if KIIKO_BOARD_HAS_USB_DOWNLOAD
esp_err_t app_enter_download(void);
bool app_download_active(void);
void app_cancel_download(void);
#endif

#ifdef __cplusplus
}
#endif
