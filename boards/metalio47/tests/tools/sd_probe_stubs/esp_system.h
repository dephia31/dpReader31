#pragma once
#define ESP_RST_DEEPSLEEP 5
extern int test_reset_reason;
static inline int esp_reset_reason(void){return test_reset_reason;}
