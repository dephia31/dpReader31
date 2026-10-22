/* SPDX-License-Identifier: Apache-2.0
 * 中文：封面像素缓存按板级预算（S31 4MiB / 96 槽，pico 不参与）；低内存时回收最久未使用项，并始终给前台留出板级 PSRAM 预留。
 * English: Bound cover pixels by the board budget (4 MiB / 96 slots on the S31, unused on the Pico);
 * evict the least recently used entries under pressure while keeping the board's PSRAM reserve free.
 *
 * 冻结：只有 KIIKO_BOARD_HAS_COVER_CACHE 为 1 的板才编译本文件；数值不得写死。
 * Frozen: compiled only when KIIKO_BOARD_HAS_COVER_CACHE is 1; no hard-coded budget numbers here.
 */
#include "metalio_cover_cache.h"
#include "kiiko_board.h"
#if KIIKO_BOARD_HAS_COVER_CACHE

#include "metalio_memory.h"
#include "esp_heap_caps.h"
#include <stdlib.h>
#include <string.h>
#ifdef ESP_PLATFORM
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
static StaticSemaphore_t mutex_storage;
static SemaphoreHandle_t mutex;
static portMUX_TYPE init_lock=portMUX_INITIALIZER_UNLOCKED;
static void lock(void) {
    portENTER_CRITICAL(&init_lock);
    if(!mutex) mutex=xSemaphoreCreateMutexStatic(&mutex_storage);
    portEXIT_CRITICAL(&init_lock);
    xSemaphoreTake(mutex,portMAX_DELAY);
}
static void unlock(void) { xSemaphoreGive(mutex); }
#else
#include <pthread.h>
static pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER;
static void lock(void) { pthread_mutex_lock(&mutex); }
static void unlock(void) { pthread_mutex_unlock(&mutex); }
#endif
// 槽数与像素预算来自板级宏。/ Slot count and pixel budget come from the board macros.
#define CACHE_SLOTS KIIKO_BOARD_COVER_RAM_SLOTS
#define CACHE_BUDGET ((size_t)KIIKO_BOARD_COVER_RAM_BUDGET_KB * 1024u)
typedef struct {
    metalio_cover_key_t key;
    metalio_cover_meta_t meta;
    uint8_t *pixels;
    size_t bytes;
    uint64_t used;
} cache_entry_t;
static cache_entry_t *entries;
static metalio_cover_stats_t stats;
static uint64_t stamp;
static void discard(unsigned i) {
    if(!entries[i].pixels) return;
    free(entries[i].pixels); stats.bytes-=entries[i].bytes; --stats.entries;
    memset(&entries[i],0,sizeof(entries[i])); ++stats.evictions;
}
static bool evict(void) {
    if(!entries || !stats.entries) return false;
    unsigned oldest=0; uint64_t age=UINT64_MAX;
    for(unsigned i=0;i<CACHE_SLOTS;i++) if(entries[i].pixels && entries[i].used<age) {
        oldest=i; age=entries[i].used;
    }
    discard(oldest); return true;
}
bool metalio_cover_cache_get(const metalio_cover_key_t *key,void *out,size_t capacity,metalio_cover_meta_t *meta) {
    if(!key || !out) return false;
    lock();
    if(entries) for(unsigned i=0;i<CACHE_SLOTS;i++) {
        cache_entry_t *e=&entries[i];
        if(e->pixels && !memcmp(&e->key,key,sizeof(*key)) && capacity>=e->bytes) {
            memcpy(out,e->pixels,e->bytes); if(meta) *meta=e->meta;
            e->used=++stamp; ++stats.hits; unlock(); return true;
        }
    }
    ++stats.misses; unlock(); return false;
}
bool metalio_cover_cache_put(const metalio_cover_key_t *key,const void *pixels,size_t bytes,metalio_cover_meta_t meta) {
    if(!key || !pixels || !bytes || bytes>CACHE_BUDGET) return false;
    lock();
    if(!entries) entries=heap_caps_calloc(CACHE_SLOTS,sizeof(*entries),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!entries) { unlock(); return false; }
    // 同一文件版本/光栅只存一份；书籍修改时移除旧版本。/ Replace same raster or stale file versions.
    for(unsigned i=0;i<CACHE_SLOTS;i++) if(entries[i].pixels &&
       (!memcmp(&entries[i].key,key,sizeof(*key)) ||
        (entries[i].key.field[0]==key->field[0] &&
         (entries[i].key.field[2]!=key->field[2] || entries[i].key.field[3]!=key->field[3] ||
          (key->field[4]==3 && entries[i].key.field[4]<=2))))) discard(i);
    while(stats.entries && (stats.bytes+bytes>metalio_cache_budget(stats.bytes,CACHE_BUDGET) ||
          stats.entries==CACHE_SLOTS || !metalio_cache_can_grow(bytes))) evict();
    if(!metalio_cache_can_grow(bytes)) { unlock(); return false; }
    uint8_t *copy=heap_caps_malloc(bytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    while(!copy && evict()) copy=heap_caps_malloc(bytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!copy) { unlock(); return false; }
    for(unsigned i=0;i<CACHE_SLOTS;i++) if(!entries[i].pixels) {
        memcpy(copy,pixels,bytes);
        entries[i]=(cache_entry_t){.key=*key,.meta=meta,.pixels=copy,.bytes=bytes,.used=++stamp};
        stats.bytes+=bytes; ++stats.entries; unlock(); return true;
    }
    free(copy); unlock(); return false;
}
void metalio_cover_cache_trim(size_t budget) {
    lock(); while(stats.bytes>budget && evict()) {} unlock();
}
void metalio_cover_cache_clear(void) {
    lock(); while(evict()) {} free(entries); entries=NULL; unlock();
}
metalio_cover_stats_t metalio_cover_cache_stats(void) { lock(); metalio_cover_stats_t s=stats; unlock(); return s; }

#else
// 本板不编封面缓存的实现时给一个空翻译单元，避免依赖空 TU 的扩展。/ Keep a non-empty unit when this board has no cache.
typedef int metalio_cover_cache_unused_t;
#endif