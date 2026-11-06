/* 中文：Love Book的无书架兼容出口；不分配内存、不读卡、不创建任务。
 * English: Love Book compatibility exits without shelf allocations, card I/O or tasks. */
#include "book_lock_collage.h"
bool book_lock_collage_draw(uint8_t *fb){(void)fb;return false;}
bool book_lock_collage_prepare(uint8_t *fb){(void)fb;return false;}
void book_lock_collage_cache_commit(const uint8_t *fb){(void)fb;}
bool book_lock_collage_warm_cover(const char *path){(void)path;return false;}
void book_lock_collage_invalidate(void){}
