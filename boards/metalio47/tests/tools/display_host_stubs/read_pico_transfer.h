/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include <stdbool.h>
extern bool mock_radio_busy;
static inline bool read_pico_transfer_radio_busy(void){return mock_radio_busy;}
