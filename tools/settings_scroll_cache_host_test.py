"""Actual settings cache ownership and native viewport/one-bit boundaries, ASan/UBSan.
SPDX-License-Identifier: Apache-2.0
"""
from pathlib import Path
import re, subprocess, tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'main/apps/app_device_settings.c').read_text()
def function(name):
    match=re.search(r'^static [^\n]+\b'+name+r'\([^;{}]*\)\s*\{',s,re.M)
    assert match,name
    start=match.start();at=s.index('{',start)+1;depth=1;quote=None;escape=False
    while depth:
        c=s[at]
        if quote:
            if escape:escape=False
            elif c=='\\':escape=True
            elif c==quote:quote=None
        elif c in "\"'":quote=c
        elif c=='{':depth+=1
        elif c=='}':depth-=1
        at+=1
    return s[start:at]+'\n'
unit=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ui_follow_cache.h"
#define W 1216
#define H 684
#define UI_NAV_TOP 1096
#define SETTINGS_SCROLL_MAX 907
#define EXTENT (936+SETTINGS_SCROLL_MAX)
#define BYTES ((size_t)W*H/2)
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
#define EPD_ROT_INVERTED_PORTRAIT 3
#define SETTINGS_MAIN 0
typedef struct{bool touched;}touch_t;
typedef struct{bool consumed;uint8_t *fb;touch_t *touch;}app_ctx_t;
static struct{bool tracking,swallow,coasting,settle_pending;}s_follow_scroll;
static unsigned s_scroll_cache_step;
static uint8_t *s_scroll_cache;
static int s_page,s_main_scroll,paints,allocations;
static bool s_avatar_pending,busy;
static size_t available=8u*1024*1024;
static bool fail_scratch;
static int epd_width(void){return W;}
static int epd_height(void){return H;}
static int epd_get_rotation(void){return EPD_ROT_INVERTED_PORTRAIT;}
static bool display_follow_busy(void){return busy;}
static size_t heap_caps_get_free_size(unsigned caps){assert(caps==3);return available;}
static void *heap_caps_calloc(size_t size,size_t n,unsigned caps){assert(caps==3);++allocations;return calloc(size,n);}
static void *heap_caps_malloc(size_t size,unsigned caps){assert(caps==3);++allocations;return fail_scratch?NULL:malloc(size);}
static unsigned gray(int row,int doc_y){return (unsigned)(row*7+doc_y*3+(doc_y/43))&15;}
static void render(app_ctx_t *ctx,uint8_t *fb){
 (void)ctx;++paints;memset(fb,0xa5,BYTES);
 for(int row=0;row<H;++row)for(int y=160;y<UI_NAV_TOP;y+=2)
  fb[(size_t)row*(W/2)+y/2]=gray(row,y+s_main_scroll)|(gray(row,y+1+s_main_scroll)<<4);
}
'''+function('settings_scroll_cache_release')+function('settings_scroll_cache_prepare')+r'''
static void check_view(uint8_t *fb,int offset){
 for(int row=0;row<H;++row)for(int y=0;y<W;y+=2){
  uint8_t expected=0xa5;
  if(y>=160&&y<UI_NAV_TOP)expected=(gray(row,y+offset)&8?15:0)|(gray(row,y+1+offset)&8?240:0);
  assert(fb[(size_t)row*(W/2)+y/2]==expected);
 }
}
int main(void){
 uint8_t *frame=malloc(BYTES);assert(frame);touch_t touch={0};app_ctx_t ctx={.fb=frame,.touch=&touch};
 render(&ctx,frame);int before=paints;
 assert(settings_scroll_cache_prepare(&ctx)&&s_scroll_cache_step==1&&paints==before&&allocations==1);
 uint8_t *original=malloc(BYTES);assert(original);memcpy(original,frame,BYTES);
 touch.touched=true;assert(!settings_scroll_cache_prepare(&ctx)&&allocations==1);touch.touched=false;
 s_follow_scroll.settle_pending=true;assert(!settings_scroll_cache_prepare(&ctx));s_follow_scroll.settle_pending=false;
 busy=true;assert(!settings_scroll_cache_prepare(&ctx));busy=false;
 assert(settings_scroll_cache_prepare(&ctx)&&s_scroll_cache_step==2&&paints==before+1&&allocations==2&&s_main_scroll==0);
 assert(!memcmp(frame,original,BYTES));
 before=paints;int prior_allocs=allocations;
 // Every possible subpixel/nibble/byte offset, including non-byte-aligned final extent.
 for(int offset=0;offset<=SETTINGS_SCROLL_MAX;++offset){
  memset(frame,0xa5,BYTES);assert(ui_follow_cache_blit(s_scroll_cache,frame,W,H,160,UI_NAV_TOP,EXTENT,offset));check_view(frame,offset);
 }
 assert(paints==before&&allocations==prior_allocs&&!settings_scroll_cache_prepare(&ctx));
 assert(!ui_follow_cache_blit(s_scroll_cache,frame,W,H,160,UI_NAV_TOP,EXTENT,SETTINGS_SCROLL_MAX+1));
 assert(!ui_follow_cache_blit(s_scroll_cache,frame,W,H,159,UI_NAV_TOP,EXTENT,0));
 settings_scroll_cache_release();assert(!s_scroll_cache&&!s_scroll_cache_step);
 // Low memory leaves a working uncached renderer and does not repeatedly allocate.
 available=1024*1024;assert(!settings_scroll_cache_prepare(&ctx)&&s_scroll_cache_step==3&&!s_scroll_cache);
 available=8u*1024*1024;assert(!settings_scroll_cache_prepare(&ctx));settings_scroll_cache_release();
 render(&ctx,frame);assert(settings_scroll_cache_prepare(&ctx));fail_scratch=true;
 assert(!settings_scroll_cache_prepare(&ctx)&&s_scroll_cache_step==3&&!s_scroll_cache&&s_main_scroll==0);
 // Building after a previous nonzero scroll retains the visible offset and frame.
 settings_scroll_cache_release();fail_scratch=false;s_main_scroll=197;render(&ctx,frame);memcpy(original,frame,BYTES);
 assert(settings_scroll_cache_prepare(&ctx)&&s_main_scroll==197&&!memcmp(frame,original,BYTES));
 assert(settings_scroll_cache_prepare(&ctx)&&s_main_scroll==197&&!memcmp(frame,original,BYTES));
 settings_scroll_cache_release();free(original);free(frame);
 printf("PASS: actual two-slice idle cache; retained %zu bytes, no render/allocation during 908 drag positions; all gray thresholds, fixed chrome, low memory/failure and offset restoration\n",(size_t)H*((EXTENT+7)/8));
}
'''
with tempfile.TemporaryDirectory() as directory:
    c=Path(directory)/'cache.c';binary=c.with_suffix('');c.write_text(unit)
    subprocess.run(['cc','-O1','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
                    '-I'+str(root/'main/ui'),str(c),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
