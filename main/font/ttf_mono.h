/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef struct ttf_mono_face ttf_mono_face_t;
typedef bool (*ttf_mono_read_t)(void *context,uint32_t offset,void *buffer,size_t bytes);
typedef struct {const uint8_t *pixels;int width,height,left,top,pitch;} ttf_mono_bitmap_t;
/// Borrow the original source; close this face before releasing/replacing that source.
ttf_mono_face_t *ttf_mono_open(ttf_mono_read_t read,void *context,uint32_t length);
void ttf_mono_close(ttf_mono_face_t *face);
/// Pixels are packed MSB-first and borrowed until the next render/close. Layout owns advances.
bool ttf_mono_render(ttf_mono_face_t *face,uint32_t cp,float scale,int weight,ttf_mono_bitmap_t *bitmap);
/// Signed contour offset in 1/16-pixel steps, before monochrome rasterization.
bool ttf_mono_render_stroke(ttf_mono_face_t *face,uint32_t cp,float scale,int weight,int steps,ttf_mono_bitmap_t *bitmap);
size_t ttf_mono_bytes(void);
size_t ttf_mono_peak_bytes(void);
