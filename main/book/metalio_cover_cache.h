/* SPDX-License-Identifier: Apache-2.0
 * 中文：S31 共用封面缓存，键包含文件版本和绘制参数，不向调用方借出指针。
 * English: Shared S31 cover cache uses versioned raster keys and copy-out ownership.
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef struct { uint64_t field[8]; } metalio_cover_key_t;
typedef struct { unsigned width,height; bool white_edge; } metalio_cover_meta_t;
typedef struct { size_t bytes,entries; uint64_t hits,misses,evictions; } metalio_cover_stats_t;
bool metalio_cover_cache_get(const metalio_cover_key_t *key,void *out,size_t capacity,metalio_cover_meta_t *meta);
bool metalio_cover_cache_put(const metalio_cover_key_t *key,const void *pixels,size_t bytes,metalio_cover_meta_t meta);
void metalio_cover_cache_trim(size_t budget);
void metalio_cover_cache_clear(void);
metalio_cover_stats_t metalio_cover_cache_stats(void);
