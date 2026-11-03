#pragma once
#include <stdint.h>
#include "common.h"
enum {METALIO_UI_WAKE_TOUCH=1,METALIO_UI_WAKE_KEY=2,METALIO_UI_WAKE_BLE=4,METALIO_UI_WAKE_PREP=8};
static inline void metalio_ui_wake_init(void){}
static inline void metalio_ui_wake_post(uint32_t bits){(void)bits;}
static inline void metalio_ui_wake_iteration(void){}
static inline uint32_t metalio_ui_wake_wait(uint32_t ms){vTaskDelay(ms);return 0;}

static inline uint32_t metalio_ui_wake_pending(void){return 0;}
