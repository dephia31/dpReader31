/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 出厂 VCOM 标定入口。PMU 未标定时由 app_main 拦住开机。
 *
 * Factory VCOM calibration entry. app_main gates boot when the PMU is unset.
 */

#pragma once

#include "cst836u.h"
#include "epd_highlevel.h"

#ifdef __cplusplus
extern "C" {
#endif

/// PMU 未标定 VCOM 时拦住开机，写进 PMU 后才返回。/ Gate boot when VCOM is unset; returns only after it is written to the PMU.
void vcom_setup_run(EpdiyHighlevelState* hl, uint8_t* fb, cst836u_handle_t tp);
#if KIIKO_BOARD_METALIO47

/// 读取 NVS 里保存的 VCOM（mV，900..2000、10 的倍数）；未设置时返回厂家默认 1200。
/// Load the VCOM saved in NVS (mV, 900..2000, step 10); the vendor default 1200 when unset.
int vcom_setup_load_mv(void);
#else
#endif

#ifdef __cplusplus
}
#endif
