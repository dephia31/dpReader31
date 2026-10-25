#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""S31 real hint helpers: double click priority, bounded local cache, exact restore and error paths."""
from pathlib import Path
import subprocess,tempfile
from home_cover_cache_host_test import function
root=Path(__file__).resolve().parents[1]
s=root/'main/sleep.c'
unit=r'''

#include <assert.h>
#include <stdbool.h>
static bool s_boot_lock_active,s_boot_lock_known;
static unsigned app_settings_wake_presses(void){return 2;}
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
typedef struct {int x,y,width,height;} EpdRect;
typedef struct {uint8_t *front_fb;} EpdiyHighlevelState;
enum {EPD_ROT_LANDSCAPE,EPD_ROT_PORTRAIT,EPD_ROT_INVERTED_LANDSCAPE,EPD_ROT_INVERTED_PORTRAIT};
enum EpdDrawError {EPD_DRAW_SUCCESS,EPD_DRAW_FAULT};
static bool display_fail;
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
#define UI_GRAY_WHITE 255
#define UI_GRAY_BLACK 0
#define EPD_DRAW_ALIGN_CENTER 1
#define MODE_GL16 2
#define W 1216
#define H 684
static int rotation,draws,allocated;
static uint32_t serial;static int64_t now;static bool double_key,oom;
static uint8_t fb[W*H],original[W*H];static EpdiyHighlevelState hl={fb};static int E0470_WAVEFORM;
static uint8_t *epd_hl_get_framebuffer(EpdiyHighlevelState*h){return h->front_fb;}
static int epd_width(void){return W;}static int epd_height(void){return H;}
static int epd_get_rotation(void){return rotation;}
static int epd_rotated_display_width(void){return rotation%2?H:W;}
static int epd_rotated_display_height(void){return rotation%2?W:H;}
static uint8_t epd_get_pixel(int x,int y,int w,int h,uint8_t*f){assert(w==W&&h==H&&x>=0&&x<w&&y>=0&&y<h);return f[y*W+x];}
static void epd_draw_pixel(int x,int y,uint8_t v,uint8_t*f){
 int px=x,py=y;
 if(rotation==EPD_ROT_PORTRAIT){px=W-y-1;py=x;}
 else if(rotation==EPD_ROT_INVERTED_LANDSCAPE){px=W-x-1;py=H-y-1;}
 else if(rotation==EPD_ROT_INVERTED_PORTRAIT){px=y;py=H-x-1;}
 assert(px>=0&&px<W&&py>=0&&py<H);f[py*W+px]=v;
}
static void epd_poweron(void){}static void epd_poweroff(void){}
static enum EpdDrawError update_display_area_diff_with(EpdiyHighlevelState*h,const void*w,int m,EpdRect r){assert(h==&hl&&w==&E0470_WAVEFORM&&m==MODE_GL16&&r.width==448&&r.height==72);++draws;return display_fail?EPD_DRAW_FAULT:EPD_DRAW_SUCCESS;}
static void guard_draw_result(EpdiyHighlevelState*h,enum EpdDrawError e){assert(h==&hl&&e==(display_fail?EPD_DRAW_FAULT:EPD_DRAW_SUCCESS));}
static void *heap_caps_calloc(size_t a,size_t b,unsigned cap){assert(cap==3&&a*b==16128);if(oom)return NULL;allocated=a*b;return calloc(a,b);}
static void heap_caps_free(void*p){assert(allocated==16128);allocated=0;free(p);}
static int64_t esp_timer_get_time(void){return now*1000;}
static uint32_t read_pico_pmu_lock_key_serial(void){return serial;}
static bool read_pico_pmu_take_key_wakeup(void){return double_key;}
static void ui_fill_round_rect(uint8_t*f,EpdRect r,int radius,uint8_t v){assert(radius==28);for(int y=0;y<r.height;++y)for(int x=0;x<r.width;++x)epd_draw_pixel(r.x+x,r.y+y,v,f);}
static void ui_draw_round_rect(uint8_t*f,EpdRect r,int rad,uint8_t v){(void)f;(void)r;(void)rad;(void)v;}
static void ui_text_fixed_vc(uint8_t*f,int x,int y,int px,const char*t,int align,bool bold){assert(px==28&&align==1&&!bold&&!strcmp(t,"双击电源键解锁"));epd_draw_pixel(x,y,0,f);}
'''
source=s.read_text();a=source.index('static EpdiyHighlevelState *s_lock_hint_hl;');b=source.index('static uint8_t lock_hint_pixel',a)
unit+='static EpdiyHighlevelState *s_lock_hint_hl;\n'+source[source.index('static struct {',a):b]
for name in ('lock_hint_pixel','lock_hint_present','lock_hint_finish','lock_hint_service','lock_hint_begin','lock_hint_end'):
    unit+=function(name,s)+'\n'

unit+=r'''
static bool locked_tick(void){if(read_pico_pmu_take_key_wakeup())return true;lock_hint_service();return false;}
static void reset(void){
 memset(&s_lock_hint,0,sizeof(s_lock_hint));
 for(unsigned i=0;i<sizeof(fb);++i)fb[i]=(uint8_t)((i%16)*17);
 memcpy(original,fb,sizeof(fb));serial=7;now=1000;draws=allocated=0;double_key=oom=display_fail=false;
 lock_hint_begin(&hl);
}
int main(void){
 for(rotation=0;rotation<4;++rotation){
  // The initiating/stale click is captured at entry, not presented as another new press.
  reset();now=9000;assert(!locked_tick()&&!draws);
  ++serial;assert(!locked_tick());now=9500;double_key=true;
  assert(locked_tick()&&!draws);lock_hint_end();assert(!s_lock_hint_hl&&!allocated);
  reset();++serial;assert(!locked_tick()&&!draws);now=1649;assert(!locked_tick()&&!draws);
  now=1650;assert(!locked_tick()&&draws==1&&allocated==16128);
  for(now=1700;now<4600;now+=100)assert(!locked_tick()&&draws==1);
  now=4650;assert(!locked_tick()&&draws==2&&!allocated&&!memcmp(fb,original,sizeof(fb)));
  // Real wake restores memory with no extra screen scan before the protected PIN/page is rendered.
  reset();++serial;assert(!locked_tick());now=1650;assert(!locked_tick());
  double_key=true;assert(locked_tick());lock_hint_end();
  assert(draws==1&&!allocated&&!memcmp(fb,original,sizeof(fb))&&!s_lock_hint_hl);
  reset();lock_hint_end();++serial;now=9000;assert(!locked_tick()&&!draws);
  reset();oom=true;++serial;assert(!locked_tick());now=1650;assert(!locked_tick());
  now=4650;assert(!locked_tick()&&!allocated&&!draws&&!memcmp(fb,original,sizeof(fb)));lock_hint_end();
  reset();display_fail=true;++serial;assert(!locked_tick());now=1650;assert(!locked_tick()&&draws==1);
  now=4650;assert(!locked_tick()&&draws==2&&!allocated&&!memcmp(fb,original,sizeof(fb)));lock_hint_end();
 }
 puts("PASS: S31 production hint650ms/3s, double-click priority,16KiB local cache, four rotations, exact restore, stale/context/OOM/output-failure cleanup; no touch/lamp/PIN bypass");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(unit)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
