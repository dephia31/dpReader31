/* SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
/// 由短按释放事件驱动，60ms去抖、600ms双击窗口；按下和长按不计数。/ Consume short releases with a 60ms debounce and 600ms double-click window; DOWN/holds do not count.
typedef struct {uint32_t last_ms;bool armed,ready;} power_double_click_t;
static inline void power_double_click_push(power_double_click_t *s,uint32_t at) {
    uint32_t gap=at-s->last_ms;
    if(s->armed && gap>=60u && gap<=600u) {s->ready=true;s->armed=false;}
    else if(!s->armed || gap>=60u){s->last_ms=at;s->armed=true;}
}
static inline bool power_double_click_take(power_double_click_t *s) {
    bool ready=s->ready;s->ready=false;return ready;
}
