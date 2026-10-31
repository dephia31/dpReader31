/* SPDX-License-Identifier: Apache-2.0
 * S31原厂SDK浅睡修复的C入口；不修改波形或永久配置。
 * C entry points for the vendor S31 sleep fix; no waveform or permanent configuration changes.
 */
#pragma once
#include "esp_err.h"
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
/// 初始化原厂唤醒校准修复。/ Initialize the vendor wake calibration fix.
esp_err_t metalio_sleep_sdk_init(void);
/// 保留原睡眠参数，使用原厂兼容路径进入浅睡。/ Enter light sleep through vendor compatibility with existing sleep parameters.
esp_err_t metalio_light_sleep_start(void);
/// 有界重放保留的复位/睡眠阶段证据，不写TF。/ Bounded replay of retained reset/sleep evidence, without TF writes.
void metalio_sleep_report_reset(void);
void metalio_sleep_previous_summary(char *out, size_t size);
/// Record a retained peripheral-resume checkpoint; no storage writes.
typedef enum {
    METALIO_SLEEP_TRACE_BEFORE_SLEEP = 4,
    METALIO_SLEEP_TRACE_SD = 7,
    METALIO_SLEEP_TRACE_LCD = 8,
    METALIO_SLEEP_TRACE_RESTORED = 9,
    METALIO_SLEEP_TRACE_RELEASE_PINS = 10,
    METALIO_SLEEP_TRACE_RAILS = 11,
    METALIO_SLEEP_TRACE_KEYS = 12,
    METALIO_SLEEP_TRACE_TOUCH = 13,
    METALIO_SLEEP_TRACE_MEDIA = 14,
} metalio_sleep_trace_phase_t;
void metalio_sleep_trace_phase(metalio_sleep_trace_phase_t phase);
#ifdef __cplusplus
}
#endif
