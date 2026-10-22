#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
"""中文：锁屏提示保持验证规则、准确恢复原像素、触摸窗口有界。/ English: Lock hints retain authentication, restore real pixels and bound touch power."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'boards/pico/src/main/sleep.c').read_text();a=s.index('static EpdiyHighlevelState *s_lock_hint_hl;');b=s.index('static void lock_arm_ioe_wakeup',a)
unit=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
typedef struct {int x,y,width,height;} EpdRect;
typedef struct {uint8_t *front_fb;} EpdiyHighlevelState;
typedef struct {bool touched;} cst836u_touch_t;
enum {EPD_ROT_LANDSCAPE,EPD_ROT_PORTRAIT,EPD_ROT_INVERTED_LANDSCAPE,EPD_ROT_INVERTED_PORTRAIT};
enum EpdDrawError {EPD_DRAW_SUCCESS};
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
#define UI_GRAY_WHITE 255
#define UI_GRAY_BLACK 0
#define EPD_DRAW_ALIGN_CENTER 1
#define MODE_GL16 2
#define ESP_OK 0
#define CST836U_MODE_DEEPSLEEP 2
#define W 1216
#define H 684
static int s_tp=1,rotation,mode,wakes,reads,draws,allocated;
static uint32_t serial;static int64_t now;static bool double_key,oom,screen_touch;
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
static enum EpdDrawError update_display_area_diff_with(EpdiyHighlevelState*h,const void*w,int m,EpdRect r){assert(h==&hl&&w==&E0470_WAVEFORM&&m==MODE_GL16&&r.width==448&&r.height==72);++draws;return 0;}
static void guard_draw_result(EpdiyHighlevelState*h,enum EpdDrawError e){assert(h==&hl&&!e);}
static void *heap_caps_calloc(size_t a,size_t b,unsigned cap){assert(cap==3&&a*b==16128);if(oom)return NULL;allocated=a*b;return calloc(a,b);}
static void heap_caps_free(void*p){assert(allocated==16128);allocated=0;free(p);}
static int cst836u_set_mode(int h,int m){assert(h==s_tp&&m==CST836U_MODE_DEEPSLEEP);mode=m;return 0;}
static int cst836u_wake(int h){assert(h==s_tp);++wakes;mode=0;return 0;}
static int cst836u_read(int h,cst836u_touch_t*t){assert(h==s_tp);++reads;t->touched=screen_touch;return 0;}
static int64_t esp_timer_get_time(void){return now*1000;}
static uint32_t read_pico_pmu_lock_key_serial(void){return serial;}
static unsigned presses=2;static bool single_key;
static uint8_t app_settings_wake_presses(void){return presses;}
static bool read_pico_pmu_take_key_wakeup_single(void){return single_key;}
static bool read_pico_pmu_take_key_wakeup(void){return double_key;}
static void ui_fill_round_rect(uint8_t*f,EpdRect r,int radius,uint8_t v){assert(radius==28);for(int y=0;y<r.height;++y)for(int x=0;x<r.width;++x)epd_draw_pixel(r.x+x,r.y+y,v,f);}
static void ui_draw_round_rect(uint8_t*f,EpdRect r,int rad,uint8_t v){(void)f;(void)r;(void)rad;(void)v;}
static void ui_text_fixed_vc(uint8_t*f,int x,int y,int px,const char*t,int align,bool bold){assert(px==28&&align==1&&!bold&&!strcmp(t,"双击电源键解锁"));epd_draw_pixel(x,y,0,f);}
'''
unit+=s[a:s.index('static EpdiyHighlevelState *s_boot_lock_hl;',a)]+'static void lock_resume_service(void){}\n'+s[s.index('static struct {',a):b]
unit+=r'''
static void reset(void){
 memset(&s_lock_hint,0,sizeof(s_lock_hint));s_lock_hint_hl=&hl;
 for(unsigned i=0;i<sizeof(fb);++i)fb[i]=(uint8_t)((i%16)*17);
 memcpy(original,fb,sizeof(fb));serial=0;now=1000;draws=wakes=reads=allocated=0;mode=2;double_key=screen_touch=oom=false;presses=2;single_key=false;
}
int main(void){
 for(rotation=0;rotation<4;++rotation){
  reset();presses=1;single_key=true;assert(lock_wait_key()&&!draws&&!wakes);lock_hint_finish(false);
  reset();presses=1;serial=1;assert(!lock_wait_key());now=1650;assert(!lock_wait_key()&&draws==1&&wakes==1);lock_hint_finish(false);
  reset();serial=1;assert(!lock_wait_key()&&draws==0&&wakes==0);
  now=1500;double_key=true;assert(lock_wait_key()&&draws==0&&wakes==0);lock_hint_finish(false);
  reset();serial=1;assert(!lock_wait_key());now=1650;assert(!lock_wait_key());assert(draws==1&&wakes==1&&allocated==16128);
  screen_touch=true;for(now=1700;now<4600;now+=100)assert(!lock_wait_key());assert(draws==1&&wakes==1&&reads>10);
  now=4700;assert(!lock_wait_key());assert(draws==2&&mode==2&&allocated==0&&!memcmp(fb,original,sizeof(fb)));
  reset();serial=1;assert(!lock_wait_key());now=1650;assert(!lock_wait_key());lock_hint_finish(false);
  assert(draws==1&&allocated==0&&!memcmp(fb,original,sizeof(fb)));
  reset();s_lock_hint_hl=NULL;serial=1;now=9000;assert(!lock_wait_key()&&draws==0&&wakes==0);
  reset();oom=true;serial=1;assert(!lock_wait_key());now=1650;assert(!lock_wait_key());now=4700;assert(!lock_wait_key()&&allocated==0&&!memcmp(fb,original,sizeof(fb)));
 }
 puts("PASS: physical double-click always wins; screen taps never unlock/retrigger scans; bounded3s touch wake, 16KiB region, four rotations, exact art restore, timeout/context/OOM boundaries");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(unit)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
