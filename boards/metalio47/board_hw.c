/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：Metalio S31 的板级门面实现。TPS65185 电源轨、FCA9555 扩展口、前光与振动都由
 * components/metalio_hw 提供，metalio_hw_init() 由 read_pico 的板级初始化调用，
 * 所以这里只回答板型身份。
 *
 * English: the Metalio S31 side of the board facade. The TPS65185 rails, the FCA9555 expander,
 * the front light and the haptic motor live in components/metalio_hw; metalio_hw_init() is
 * called from read_pico's board init, so all that is left here is the board identity.
 */
#include "kiiko_board.h"

const char *kiiko_board_id(void) { return KIIKO_BOARD_ID; }

esp_err_t kiiko_board_extras_init(void) { return ESP_OK; }
