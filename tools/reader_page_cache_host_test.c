/* SPDX-License-Identifier: Apache-2.0 */
#include "reader_page_cache.h"
#include "esp_heap_caps.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static size_t available=16u*1024u*1024u,largest=16u*1024u*1024u;
static bool oom;
void *heap_caps_malloc(size_t n,unsigned caps){assert(caps==3);return oom?NULL:malloc(n);}
void *heap_caps_calloc(size_t n,size_t m,unsigned caps){assert(caps==3);return oom?NULL:calloc(n,m);}
size_t heap_caps_get_free_size(unsigned caps){assert(caps==3);return available;}
size_t heap_caps_get_largest_free_block(unsigned caps){assert(caps==3);return largest;}
int main(void){
 enum{BYTES=684*1216/2};uint8_t *pixels=malloc(BYTES);assert(pixels);memset(pixels,0x35,BYTES);
 available=2u*1024u*1024u;assert(!reader_page_cache_begin(1,1,BYTES,0));
 available=16u*1024u*1024u;largest=512u*1024u;assert(!reader_page_cache_begin(1,1,BYTES,0));
 largest=16u*1024u*1024u;oom=true;assert(!reader_page_cache_begin(1,1,BYTES,0));oom=false;
 uint8_t *partial=reader_page_cache_begin(1,1,BYTES,0);assert(partial);partial[0]=0x42;
 assert(!reader_page_cache_find(1,1,BYTES));
 assert(reader_page_cache_begin(1,1,BYTES,0)==partial&&partial[0]==0x42);
 reader_page_cache_finish(1,1);assert(reader_page_cache_find(1,1,BYTES)==partial);
 assert(reader_page_cache_store(0,1,pixels,BYTES,1));
 assert(reader_page_cache_store(2,1,pixels,BYTES,1));
 assert(reader_page_cache_store(3,1,pixels,BYTES,1));
 assert(reader_page_cache_bytes()==4u*BYTES);
 assert(reader_page_cache_store(4,1,pixels,BYTES,1));
 assert(reader_page_cache_find(0,1,BYTES)&&reader_page_cache_find(1,1,BYTES)&&reader_page_cache_find(2,1,BYTES));
 assert(!reader_page_cache_find(3,1,BYTES)&&reader_page_cache_find(4,1,BYTES));
 assert(!reader_page_cache_find(4,2,BYTES)&&!reader_page_cache_find(4,1,BYTES-1));
 reader_page_cache_invalidate();assert(!reader_page_cache_find(4,1,BYTES));
 partial=reader_page_cache_begin(4,1,BYTES,4);assert(partial);reader_page_cache_finish(4,1);
 assert(reader_page_cache_find(4,1,BYTES)==partial);
 partial=reader_page_cache_begin(5,1,BYTES,4);assert(partial);reader_page_cache_cancel();
 assert(!reader_page_cache_find(5,1,BYTES)&&reader_page_cache_find(4,1,BYTES));
 available=1;assert(reader_page_cache_store(6,2,pixels,BYTES,6));
 assert(reader_page_cache_bytes()==4u*BYTES);
 reader_page_cache_clear();assert(!reader_page_cache_bytes());
 assert(!reader_page_cache_begin(0,1,BYTES,0));
 available=16u*1024u*1024u;
 assert(!reader_page_cache_begin(0,1,0,0)&&!reader_page_cache_begin(0,1,1024u*1024u+1,0));
 assert(reader_page_cache_store(0,9,pixels,BYTES,0));
 pixels[0]=0x77;assert(reader_page_cache_find(0,9,BYTES)[0]==0x35);
 reader_page_cache_clear();free(pixels);
 puts("PASS: four-frame bound, previous/future retention, unfinished-page isolation, generation/geometry invalidation, copy ownership, reserve/fragmentation/OOM fallback and teardown");
}
