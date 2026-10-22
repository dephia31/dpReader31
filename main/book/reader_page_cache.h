/* SPDX-License-Identifier: Apache-2.0
 * 中文：若干张可重建正文光栅（槽数由板级宏给出），调用方须持有阅读绘制锁；不持有屏幕历史帧。
 * English: Rebuildable body rasters (slot count from the board macro), used under the reader draw
 * lock; never owns EPD history.
 *
 * 冻结：只缓存可重建的像素，不缓存字库或解包句柄。
 * Frozen: cached pixels only; never fonts or unpacked handles.
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
