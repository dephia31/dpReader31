#pragma once
#include "esp_err.h"
static int test_single_idle_error,test_single_idle_calls;
static inline esp_err_t metalio_sd_single_line_idle(void){++test_single_idle_calls;return test_single_idle_error;}
static inline void metalio_sd_release_hold(void){}
static inline void metalio_usb_serial_reconnect(void){}
