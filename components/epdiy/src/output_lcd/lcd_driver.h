#pragma once

#include "epd_lcd.h"

#include <stdbool.h>
#include <stdint.h>

typedef bool (*line_cb_func_t)(void*, uint8_t*);
typedef void (*frame_done_func_t)(void*);

void epd_lcd_frame_done_cb(frame_done_func_t, void* payload);
void epd_lcd_line_source_cb(line_cb_func_t, void* payload);
void epd_lcd_start_frame();

void epd_lcd_abort_frame(void);
void epd_lcd_prime_white_border(bool enable);
int epd_lcd_white_border_pixels(void);
