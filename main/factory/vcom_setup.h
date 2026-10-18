/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * VCOM 标定入口。Metalio S31 上没有 Pico PMU，值改为存 ESP NVS；开机按住音量键进入。
 *
 * VCOM calibration entry. The Metalio S31 board has no Pico PMU, so the value is
 * stored in ESP NVS; hold a volume key while powering on to enter the page.
 */

#pragma once

#include "cst836u.h"
#include "epd_highlevel.h"

#ifdef __cplusplus
extern "C" {
#endif

/// 进入标定页，写入成功后返回。/ Enter the calibration page; returns after a successful write.
void vcom_setup_run(EpdiyHighlevelState* hl, uint8_t* fb, cst836u_handle_t tp);

/// 读取 NVS 里保存的 VCOM（mV，900..4000、10 的倍数）；未设置时返回 3000。
/// Load the VCOM saved in NVS (mV, 900..4000, step 10); 3000 when unset.
int vcom_setup_load_mv(void);

#ifdef __cplusplus
}
#endif
