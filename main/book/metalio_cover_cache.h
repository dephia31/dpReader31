/* SPDX-License-Identifier: Apache-2.0
 * 中文：封面像素缓存，键包含文件版本和绘制参数，不向调用方借出指针；只有 KIIKO_BOARD_HAS_COVER_CACHE 的板提供实现。
 * English: Cover pixel cache with versioned raster keys and copy-out ownership; implemented only on
 * boards whose KIIKO_BOARD_HAS_COVER_CACHE is 1.
 *
 * 冻结：pico 侧调用点在 book_cover.c 里被同一宏关掉，这里的声明不得让 pico 产生外部引用。
 * Frozen: the Pico call sites are compiled out by the same macro in book_cover.c; these declarations
 * must never leave the Pico with an external reference.
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "kiiko_board.h"
#if KIIKO_BOARD_HAS_COVER_CACHE
typedef struct { uint64_t field[8]; } metalio_cover_key_t;
typedef struct { unsigned width,height; bool white_edge; } metalio_cover_meta_t;
typedef struct { size_t bytes,entries; uint64_t hits,misses,evictions; } metalio_cover_stats_t;
bool metalio_cover_cache_get(const metalio_cover_key_t *key,void *out,size_t capacity,metalio_cover_meta_t *meta);
bool metalio_cover_cache_put(const metalio_cover_key_t *key,const void *pixels,size_t bytes,metalio_cover_meta_t meta);
void metalio_cover_cache_trim(size_t budget);
void metalio_cover_cache_clear(void);
metalio_cover_stats_t metalio_cover_cache_stats(void);
#endif
