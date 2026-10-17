/* SPDX-License-Identifier: Apache-2.0
 * 中文：S31 可重建缓存共用增长边界，给字体、插图和无线留出 PSRAM 与连续块。
 * English: S31 rebuildable caches share growth admission, preserving PSRAM/contiguous space.
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include "esp_heap_caps.h"
#define METALIO_PSRAM (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
#define METALIO_CACHE_RESERVE (2560u * 1024u)
#define METALIO_CACHE_BLOCK_RESERVE (1024u * 1024u)

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
