#!/usr/bin/env python3
"""中文：实际可选字体表预算与OOM回收，保留字库及活动槽。/ English: Actual optional table budget and OOM reclamation preserve faces and the active slot."""
from pathlib import Path
import subprocess,tempfile
from home_cover_cache_host_test import function
root=Path(__file__).resolve().parents[1]
font=root/'main/font/ttf_font.c'
unit=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define KIIKO_BOARD_METALIO47 1
#define TTF_FONT_SLOTS 4
#define TTF_MAP_MAX (5u*1024u*1024u)
#define TTF_MAP_RESERVE (2560u*1024u)
#define TTF_TABLE_CACHE_MAX (2u*1024u*1024u)
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
#define ESP_LOGI(tag,fmt,...) do{(void)(tag);if(0)printf(fmt,__VA_ARGS__);}while(0)
static const char *TAG="font";
typedef struct {uint8_t *glyf,*gvar;uint32_t f_glyf_ram_len,f_gvar_ram_len,off,voff;bool ready;} slot_t;
static slot_t s_slots[TTF_FONT_SLOTS],*s_cur=&s_slots[0];static int s_cur_index;
#define glyf_ram_len s_cur->f_glyf_ram_len
#define gvar_ram_len s_cur->f_gvar_ram_len
static bool admit=true,read_ok=true,alloc_ok=true;
static unsigned allocations,unmaps,clears;
static size_t largest=10u*1024u*1024u;
static bool metalio_cache_can_grow(size_t bytes){(void)bytes;return admit;}
static size_t heap_caps_get_free_size(int caps){(void)caps;return largest;}
static size_t heap_caps_get_largest_free_block(int caps){(void)caps;return largest;}
static void *heap_caps_malloc(size_t n,int caps){(void)caps;++allocations;return alloc_ok?malloc(n):NULL;}
static void heap_caps_free(void *p){free(p);}
static int64_t esp_timer_get_time(void){return 0;}
static bool io_read_raw(unsigned off,void *p,size_t n){(void)off;(void)p;(void)n;return read_ok;}
static size_t ttf_font_cache_clear_all(void){++clears;return 1234;}
static void io_unmap(void){free(s_cur->glyf);free(s_cur->gvar);s_cur->glyf=s_cur->gvar=NULL;s_cur->f_glyf_ram_len=s_cur->f_gvar_ram_len=0;++unmaps;}
static void slot_restore(int i){s_cur_index=i;s_cur=&s_slots[i];}
'''
for n in ('mapped_table_bytes','try_map_table','ttf_font_reclaim_optional_cache'):unit+=function(n,font)+'\n'
unit+=r'''
#define ESP_OK 0
#define ESP_ERR_NO_MEM 1
#define TTF_FONT_SLOT_SYSTEM 0
#define ESP_LOGW ESP_LOGI
typedef int esp_err_t;
static unsigned load_calls,fail_loads;
static void ttf_font_slots_init(void){}
static int slot_lend(int i){int keep=s_cur_index;slot_restore(i);return keep;}
static int open_path_here(const char *path){(void)path;++load_calls;if(fail_loads){--fail_loads;return ESP_ERR_NO_MEM;}return ESP_OK;}
'''
unit+=function('ttf_font_open',font)+'\n'
unit+=r'''
static bool map(int slot,unsigned bytes){return try_map_table(0,bytes,&s_slots[slot].glyf,&s_slots[slot].off,&s_slots[slot].f_glyf_ram_len,"glyf");}
int main(void){
 for(int i=0;i<4;++i)s_slots[i].ready=true;
 assert(map(0,1536u*1024u));unsigned before=allocations;
 assert(!map(1,800u*1024u)&&allocations==before);
 assert(map(1,256u*1024u)&&map(2,256u*1024u)&&mapped_table_bytes()==TTF_TABLE_CACHE_MAX);
 before=allocations;assert(!map(3,4096)&&allocations==before);
 slot_restore(2);assert(ttf_font_reclaim_optional_cache()==TTF_TABLE_CACHE_MAX+1234);
 assert(clears==1&&unmaps==4&&s_cur_index==2&&s_cur==&s_slots[2]&&mapped_table_bytes()==0);
 for(int i=0;i<4;++i)assert(s_slots[i].ready);
 admit=false;before=allocations;assert(!map(0,4096)&&allocations==before);admit=true;
 largest=TTF_MAP_RESERVE;assert(!map(0,4096)&&allocations==before);largest=10u*1024u*1024u;
 read_ok=false;assert(!map(0,4096)&&!s_slots[0].glyf&&mapped_table_bytes()==0);read_ok=true;
 alloc_ok=false;assert(!map(0,4096)&&mapped_table_bytes()==0);
 unsigned old=clears;slot_restore(2);fail_loads=1;assert(ttf_font_open("chosen.ttf")==ESP_OK&&load_calls==2&&clears==old+1&&s_cur_index==2);
 load_calls=0;fail_loads=3;old=clears;assert(ttf_font_open("chosen.ttf")==ESP_ERR_NO_MEM&&load_calls==2&&clears==old+1&&s_cur_index==2);
 puts("PASS: actual table maps share2MiB, reject pressure/fragmentation, roll back IO/allocation failures; reclamation preserves every face and the active slot");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(unit);subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror',str(p/'test.c'),'-o',str(p/'test')],check=True);subprocess.run([str(p/'test')],check=True)
