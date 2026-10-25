#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
"""Exercise the actual idle preview scheduler, scratch ownership and touch deferral."""
from pathlib import Path
import subprocess
import tempfile
from home_cover_cache_host_test import function

root = Path(__file__).resolve().parents[1]
home = root / 'main/apps/app_dashboard.c'
unit = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "app.h"
typedef struct {int x,y,width,height;} EpdRect;
enum {HOME_ROOT,HOME_COMPONENT,HOME_GOAL,HOME_TIME};
typedef enum {APP_HOME_FOLDER,APP_HOME_CLOCK,APP_HOME_ASTERISK=2,APP_HOME_MOON=4,APP_HOME_KEHAI=5,APP_HOME_DALI=6,APP_HOME_CAT=7,APP_HOME_HOUSE=8} app_home_component_t;
typedef enum {UI_HOME_CLOCK_PS1995,UI_HOME_CLOCK_ASTERISK,UI_HOME_CLOCK_MOON,UI_HOME_CLOCK_KEHAI,UI_HOME_CLOCK_DALI,UI_HOME_CLOCK_CAT,UI_HOME_CLOCK_HOUSE,UI_HOME_CLOCK_COUNT} ui_home_clock_style_t;
static const app_home_component_t s_components[]={APP_HOME_MOON,APP_HOME_CLOCK,APP_HOME_KEHAI,APP_HOME_CAT,APP_HOME_DALI,APP_HOME_ASTERISK,APP_HOME_HOUSE,APP_HOME_FOLDER};
#define HOME_COMPONENT_OPTIONS (sizeof(s_components)/sizeof(s_components[0]))
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
static app_home_component_t s_component_browse=APP_HOME_KEHAI;
static int s_view=HOME_COMPONENT;
static bool s_component_pending,fast,oom,ready[UI_HOME_CLOCK_COUNT];
static uint8_t cache,front[1216*684/2],back[1216*684/2],*s_clock_base_cache=&cache,*allocated;
static int64_t s_component_warm_after_ms;
static unsigned allocations,frees,draws,clears;
static ui_home_clock_style_t prepared;
static size_t free_bytes=4u*1024u*1024u;
static int epd_width(void){return 1216;}
static int epd_height(void){return 684;}
static bool app_settings_main_fast_refresh(void){return fast;}
static size_t heap_caps_get_free_size(int flags){assert(flags==3);return free_bytes;}
static void *heap_caps_malloc(size_t bytes,int flags){assert(flags==3&&bytes==sizeof(front)&&!allocated);++allocations;return oom?NULL:(allocated=malloc(bytes));}
static void checked_free(void *p){assert(p&&p==allocated);++frees;allocated=NULL;free(p);}
#define free checked_free
static bool ui_home_clock_preview_cached(EpdRect b,bool binary,ui_home_clock_style_t style){assert(b.x==141&&b.y==294&&b.width==420&&b.height==460&&binary==fast);return ready[style];}
static void ui_fill_rect_fast(EpdRect b,int gray,uint8_t *fb){assert(fb==allocated&&gray==255&&b.width==420);++clears;memset(fb,255,sizeof(front));}
static void ui_home_clock_preview_draw(uint8_t *fb,EpdRect b,bool binary,ui_home_clock_style_t style){assert(fb==allocated&&fb!=front&&fb!=back&&b.width==420&&binary==fast&&!ready[style]);ready[style]=true;prepared=style;++draws;}
'''
unit += '\n'.join(function(name, home) for name in
                  ['component_index', 'clock_style', 'component_clock_bounds', 'component_prepare_neighbor'])
unit += r'''
int main(void){
 app_ctx_t ctx={.fb=front,.now_ms=1000};memset(front,0x37,sizeof(front));memset(back,0x5a,sizeof(back));
 s_component_warm_after_ms=1200;component_prepare_neighbor(&ctx);assert(!allocations);
 ctx.now_ms=1200;ctx.consumed=true;component_prepare_neighbor(&ctx);assert(!allocations);ctx.consumed=false;
 cst836u_touch_t touch={.touched=true};ctx.touch=&touch;component_prepare_neighbor(&ctx);assert(!allocations);touch.touched=false;
 s_component_pending=true;component_prepare_neighbor(&ctx);assert(!allocations);s_component_pending=false;
 s_view=HOME_ROOT;component_prepare_neighbor(&ctx);assert(!allocations);s_view=HOME_COMPONENT;
 s_clock_base_cache=NULL;component_prepare_neighbor(&ctx);assert(!allocations);s_clock_base_cache=&cache;
 free_bytes=2u*1024u*1024u+sizeof(front)-1;component_prepare_neighbor(&ctx);assert(!allocations);
 free_bytes=4u*1024u*1024u;ctx.now_ms+=600;oom=true;component_prepare_neighbor(&ctx);assert(allocations==1&&!draws&&!allocated);oom=false;
 ctx.now_ms+=600;component_prepare_neighbor(&ctx);assert(draws==1&&frees==1&&clears==1&&prepared==UI_HOME_CLOCK_CAT&&!allocated);
 component_prepare_neighbor(&ctx);assert(draws==1);ctx.now_ms+=600;component_prepare_neighbor(&ctx);
 assert(draws==2&&frees==2&&prepared==UI_HOME_CLOCK_PS1995);
 for(int i=0;i<10;++i){ctx.now_ms+=600;component_prepare_neighbor(&ctx);}
 assert(draws==6&&frees==6&&!ready[UI_HOME_CLOCK_KEHAI]); // visible style is already prepared by normal presentation
 unsigned old=allocations;ctx.now_ms+=600;component_prepare_neighbor(&ctx);assert(allocations==old);
 for(size_t i=0;i<sizeof(front);++i)assert(front[i]==0x37&&back[i]==0x5a);
 memset(ready,0,sizeof(ready));fast=true;ctx.now_ms+=600;component_prepare_neighbor(&ctx);assert(draws==7&&frees==7);
 puts("PASS: actual S31 neighboring-style preparation stays off front/back, prioritizes adjacent styles, runs one per600ms and defers during touch/pending output/low memory/OOM");
}
'''
with tempfile.TemporaryDirectory(prefix='s31-clock-picker-prefetch-') as folder:
    path = Path(folder) / 'test.c'
    binary = path.with_suffix('')
    path.write_text(unit)
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined',
                    '-I'+str(root/'tools/ui_gesture_stubs'), str(path), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
