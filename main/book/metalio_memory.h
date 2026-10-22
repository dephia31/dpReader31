/* SPDX-License-Identifier: Apache-2.0
 * 中文：可重建缓存共用的增长边界，给字体、插图和无线留出 PSRAM 与连续块；预留量由板级宏给出。
 * English: Growth admission shared by the rebuildable caches, preserving PSRAM and contiguous
 * space for fonts, images and radio. The reserves come from the board macros.
 *
 * 冻结：这里只放阈值与判据，不放分配策略；数值不得在本文件写死。
 * Frozen: thresholds and admission rules only, never allocation policy; no hard-coded numbers.
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include "esp_heap_caps.h"
#include "kiiko_board.h"

#define METALIO_PSRAM (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
// 缓存增长后仍要余下的 PSRAM：按板取值，pico 2 MiB / S31 2.5 MiB。/ PSRAM that must stay free after growth: 2 MiB on the Pico, 2.5 MiB on the S31.
#define METALIO_CACHE_RESERVE ((size_t)KIIKO_BOARD_CACHE_PSRAM_RESERVE_KB * 1024u)
// 缓存增长后仍要余下的最大连续块：两板同为 1 MiB。/ Largest contiguous block that must stay free after growth; 1 MiB on both boards.
#define METALIO_CACHE_BLOCK_RESERVE ((size_t)KIIKO_BOARD_CACHE_BLOCK_RESERVE_KB * 1024u)

static inline bool metalio_cache_can_grow(size_t bytes) {
    const size_t free=heap_caps_get_free_size(METALIO_PSRAM);
    const size_t largest=heap_caps_get_largest_free_block(METALIO_PSRAM);
    return bytes<=free && free-bytes>=METALIO_CACHE_RESERVE &&
        bytes<=largest && largest-bytes>=METALIO_CACHE_BLOCK_RESERVE;
}
static inline size_t metalio_cache_budget(size_t used,size_t cap) {
    const size_t free=heap_caps_get_free_size(METALIO_PSRAM);
    size_t room=free>METALIO_CACHE_RESERVE?free-METALIO_CACHE_RESERVE:0;
    if(heap_caps_get_largest_free_block(METALIO_PSRAM)<METALIO_CACHE_BLOCK_RESERVE)room=0;
    if(used>=cap)return cap;
    return room<cap-used?used+room:cap;
}
