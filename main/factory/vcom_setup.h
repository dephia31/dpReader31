/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：出厂 VCOM 标定入口。Pico 的标定值在 CW32 PMU 里，未标定由 app_main 拦住开机；
 * Metalio S31 的值存本机 NVS，开机按住音量键进页。
 *
 * English: factory VCOM calibration entry. On the Pico the value lives in the CW32 PMU and an
 * unset value gates boot in app_main; on the Metalio S31 it lives in local NVS and the page opens
 * by holding a volume key at power-on.
 */
#pragma once

#include "cst836u.h"
#include "epd_highlevel.h"
#include "kiiko_board.h"

#ifdef __cplusplus
extern "C" {
#endif

/// 进入标定页；写入成功（Pico 写 PMU / S31 写 NVS）后返回。
/// Enter the calibration page; returns after a successful write (PMU on the Pico, NVS on the S31).
void vcom_setup_run(EpdiyHighlevelState* hl, uint8_t* fb, cst836u_handle_t tp);

#if KIIKO_BOARD_METALIO47
/// 读取 NVS 里保存的 VCOM（mV，900..4000，10mV 步进）；未设置时返回 3000。
/// Load the VCOM saved in NVS (mV, 900..4000, 10 mV steps); 3000 when unset.
int vcom_setup_load_mv(void);
#endif

#ifdef __cplusplus
}
#endif
