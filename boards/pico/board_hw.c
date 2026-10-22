/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：小纸 Pico 的板级门面实现。Pico 的板级外设都在 components/read_pico 的
 * read_pico_init() 里一次拉起（面板、TF、加速度计、CW32 PMU、触摸），这里没有额外步骤。
 *
 * English: the Read Pico side of the board facade. Everything this board needs comes up inside
 * read_pico_init() in components/read_pico (panel, card, accelerometer, CW32 PMU, touch), so
 * there is no extra step here.
 */
#include "kiiko_board.h"

const char *kiiko_board_id(void) { return KIIKO_BOARD_ID; }

esp_err_t kiiko_board_extras_init(void) { return ESP_OK; }
