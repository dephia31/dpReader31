/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include <stdint.h>

enum {
    METALIO_UI_WAKE_TOUCH=1u<<0,
    METALIO_UI_WAKE_KEY=1u<<1,
    METALIO_UI_WAKE_BLE=1u<<2,
    METALIO_UI_WAKE_PREP=1u<<3
};
typedef struct {
    uint32_t iterations,posts,waits,event_wakes,timeouts,empty_wakes,reasons;
} metalio_ui_wake_stats_t;
/* One UI consumer. Producers publish payload before posting, from task context. */
void metalio_ui_wake_init(void);
void metalio_ui_wake_post(uint32_t reasons);
uint32_t metalio_ui_wake_wait(uint32_t timeout_ms);
void metalio_ui_wake_iteration(void);
metalio_ui_wake_stats_t metalio_ui_wake_stats(void);

uint32_t metalio_ui_wake_pending(void);
