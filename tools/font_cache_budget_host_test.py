"""Run actual cross-slot glyph eviction against constrained/fragmented PSRAM."""
from pathlib import Path
import subprocess,tempfile
from home_cover_cache_host_test import function
r=Path(__file__).resolve().parents[1];src=r/'main/font/ttf_font.c'
unit=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include "metalio_memory.h"
#define TTF_FONT_SLOTS 4
#define TTF_CACHE_BUCKETS 256
#define TTF_CACHE_LIMIT (1024u*1024u)
#define TTF_CACHE_PRESSURE_LIMIT (320u*1024u)
typedef struct glyph_entry {uint32_t codepoint;uint8_t size;void *bitmap;size_t bitmap_bytes;
 uint64_t cache_tick;struct glyph_entry *hash_next,*lru_prev,*lru_next;}glyph_entry_t;
typedef struct {bool f_ready;glyph_entry_t *f_cache_buckets[256],*f_lru_head,*f_lru_tail;size_t f_cache_bytes;}slot_t;
static slot_t s_slots[4],*s_cur=&s_slots[0];static int s_cur_index;static uint64_t s_cache_tick;static size_t s_cache_total;
static size_t available=8u*1024u*1024u,largest=8u*1024u*1024u;
void *heap_caps_malloc(size_t n,unsigned c){assert(c==3);return malloc(n);}
void *heap_caps_calloc(size_t n,size_t m,unsigned c){assert(c==3);return calloc(n,m);}
size_t heap_caps_get_free_size(unsigned c){assert(c==3);return available;}
size_t heap_caps_get_largest_free_block(unsigned c){assert(c==3);return largest;}
static void heap_caps_free(void *p){free(p);}
#define lru_tail (s_cur->f_lru_tail)
#define lru_head (s_cur->f_lru_head)
#define cache_buckets (s_cur->f_cache_buckets)
#define cache_bytes (s_cur->f_cache_bytes)
'''
for name in ['cache_bucket','lru_detach','lru_touch','cache_evict_one','cache_reserve']:unit+=function(name,src)+'\n'
unit+=r'''
static void select(int n){s_cur_index=n;s_cur=&s_slots[n];}
static void seed(int n){select(n);s_cur->f_ready=true;glyph_entry_t *g=calloc(1,sizeof(*g));assert(g);
 g->codepoint=n;g->size=48;g->bitmap_bytes=200u*1024u;g->bitmap=malloc(g->bitmap_bytes);assert(g->bitmap);
 cache_buckets[cache_bucket(n,48)]=g;cache_bytes=g->bitmap_bytes+sizeof(*g);s_cache_total+=cache_bytes;lru_touch(g);}
int main(void){
 for(int i=0;i<4;++i)seed(i);select(0);lru_touch(lru_head);
 assert(cache_reserve(400u*1024u));assert(!s_slots[1].f_cache_bytes&&s_slots[0].f_cache_bytes&&s_slots[2].f_cache_bytes&&s_slots[3].f_cache_bytes);
 assert(s_cur_index==0&&s_cur==&s_slots[0]);
 available=largest=3u*1024u*1024u;assert(cache_reserve(100u*1024u));
 largest=512u*1024u;assert(cache_reserve(100u*1024u));
 assert(s_cache_total<=TTF_CACHE_PRESSURE_LIMIT-100u*1024u);
 for(int i=0;i<4;++i)assert(s_slots[i].f_ready);
 available=128u*1024u;assert(cache_reserve(20u*1024u));
 assert(!cache_reserve(TTF_CACHE_LIMIT+1));assert(!s_cache_total);
 puts("PASS: actual global font LRU keeps hot faces, bounded 1MiB/768KiB/320KiB growth, fragmented-memory downgrade, essential low-memory glyph admission and face identity preservation");
}
'''
with tempfile.TemporaryDirectory() as d:
 d=Path(d);(d/'test.c').write_text(unit)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(r/'tools/metalio_cache_host_stubs'),'-I'+str(r/'main/book'),str(d/'test.c'),'-o',str(d/'test')],check=True)
 subprocess.run([str(d/'test')],check=True)
