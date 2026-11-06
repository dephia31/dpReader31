/* SPDX-License-Identifier: Apache-2.0
 * 中文：最多四张可重建正文光栅，调用方须持有阅读绘制锁；不持有屏幕历史帧。
 * English: Up to four rebuildable body rasters, under the reader draw lock; never own EPD history.
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
const uint8_t *reader_page_cache_find(size_t page,uint32_t generation,size_t bytes);
uint8_t *reader_page_cache_begin(size_t page,uint32_t generation,size_t bytes,size_t focus);
void reader_page_cache_finish(size_t page,uint32_t generation);
void reader_page_cache_cancel(void);
void reader_page_cache_invalidate(void);
void reader_page_cache_clear(void);
bool reader_page_cache_store(size_t page,uint32_t generation,const uint8_t *pixels,size_t bytes,size_t focus);
size_t reader_page_cache_bytes(void);
