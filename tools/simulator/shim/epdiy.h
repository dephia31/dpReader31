/* SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：主机显示接口替身。/ English: Host display interface shim.
 * 冻结：仅用于测试。/ Frozen: Tests only.
 */
#pragma once
#include <stdint.h>
typedef struct { int x, y, width, height; } EpdRect;
enum EpdFontFlags { EPD_DRAW_ALIGN_LEFT, EPD_DRAW_ALIGN_CENTER, EPD_DRAW_ALIGN_RIGHT };
enum EpdRotation { EPD_ROT_LANDSCAPE, EPD_ROT_PORTRAIT, EPD_ROT_INVERTED_LANDSCAPE, EPD_ROT_INVERTED_PORTRAIT };
void epd_draw_pixel(int x, int y, uint8_t color, uint8_t *fb);
uint8_t epd_get_pixel(int x, int y, int width, int height, const uint8_t *fb);
void epd_draw_line(int x0, int y0, int x1, int y1, uint8_t color, uint8_t *fb);
void epd_draw_circle(int x, int y, int radius, uint8_t color, uint8_t *fb);
void epd_fill_rect(EpdRect rect, uint8_t color, uint8_t *fb);
void epd_fill_circle_helper(int x, int y, int radius, int corners, int delta, uint8_t color, uint8_t *fb);
void epd_draw_rotated_image(EpdRect rect, const uint8_t *image, uint8_t *fb);
int epd_rotated_display_width(void);
int epd_rotated_display_height(void);
int epd_width(void);
int epd_height(void);
enum EpdRotation epd_get_rotation(void);

#include <stdbool.h>
#include <stddef.h>
#define EPD_BUILTIN_WAVEFORM 0
#define BLACK_ON_WHITE 0
#define WHITE_ON_BLACK 1
enum EpdDrawMode{MODE_INIT=0,MODE_DU=1,MODE_GC16=2,MODE_GL16=5};
enum EpdDrawError{EPD_DRAW_SUCCESS=0,EPD_DRAW_EMPTY_LINE_QUEUE=1};
typedef struct{int dummy;} EpdWaveform;
EpdRect epd_full_screen(void);
void epd_draw_hline(int,int,int,uint8_t,uint8_t*);
void epd_draw_vline(int,int,int,uint8_t,uint8_t*);
void epd_fill_circle(int,int,int,uint8_t,uint8_t*);
void epd_draw_rect(EpdRect,uint8_t,uint8_t*);
void epd_draw_image(EpdRect,const uint8_t*,uint8_t*);
void epd_set_rotation(enum EpdRotation);
void epd_poweron(void);void epd_poweroff(void);
void epd_copy_to_framebuffer(EpdRect,const uint8_t*,uint8_t*);
