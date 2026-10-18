/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 开机按住音量键进入的调试面板：VCOM 校准、防褪色重刷间隔、立即整屏重刷、继续开机。
 *
 * Debug panel opened by holding a volume key while powering on: VCOM calibration,
 * anti-fade refresh interval, a manual full-frame refresh, and continue-to-boot.
 */

#pragma once

#include "cst836u.h"
#include "epd_highlevel.h"
#include "stdint.h"

#ifdef __cplusplus
extern "C" {
#endif

/// 显示调试面板并处理点按，直到用户选择"继续开机"返回。
/// Show the debug panel and handle taps until "Continue" returns.
void debug_panel_run(EpdiyHighlevelState* hl, uint8_t* fb, cst836u_handle_t tp);

#ifdef __cplusplus
}
#endif
