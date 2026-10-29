"""中文：执行真实主页周期清理/整屏出口，验证黑白不漏算及错误重试。/ English: Execute real root cleanup/full output, including BW counting and failed-clean retry."""
from pathlib import Path
import subprocess,tempfile
from home_cover_cache_host_test import function
root=Path(__file__).resolve().parents[1];source=root/'boards/metalio47/src/main/display.c'
unit=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
typedef struct{int dummy;}EpdiyHighlevelState;
typedef struct{int dummy;}EpdWaveform;
enum EpdDrawError{EPD_DRAW_SUCCESS,EPD_DRAW_ERROR};
enum EpdDrawMode{MODE_GC16=2};
#define APP_UI_GC16_EVERY 6
#define APP_UI_GC16_MIN_INTERVAL_US 3000000LL
static int s_page_refreshes,full_calls,normalized,cancelled;static int64_t s_page_last_full_us,now;
static bool s_system_binary,s_main_fault,s_full_refresh_requested;
static const EpdWaveform E0470_FULL_WAVEFORM={1},E0470_WAVEFORM={0};
static enum EpdDrawError outcome;
static int64_t esp_timer_get_time(void){return now;}
static void system_normalize(EpdiyHighlevelState*h){normalized++;}
static enum EpdDrawError main_recover(EpdiyHighlevelState*h,enum EpdDrawError e){assert(0);return e;}
static void display_main_transition_cancel(void){cancelled++;}
static void epd_hl_waveform(EpdiyHighlevelState*h,const EpdWaveform*w){}
static void epd_poweron(void){}
static void rails_keepalive(void){}
'''
unit+=function('ui_page_cleanup_due',source)+'\n'+function('ui_page_cleaned',source)+'\n'
unit+=r'''
static enum EpdDrawError hl_update(EpdiyHighlevelState*h,const EpdWaveform*w,enum EpdDrawMode mode,bool full,void*area,bool diff){
 assert(w==&E0470_FULL_WAVEFORM&&mode==MODE_GC16&&full&&!area&&!diff);full_calls++;
 ui_page_cleaned(outcome);return outcome;
}
'''
unit+=function('update_display_full_unlocked',source)+'\n'+function('main_periodic_cleanup',source)+'\n'
unit+=r'''
int main(void){EpdiyHighlevelState hl={0};enum EpdDrawError result;
 for(int binary=0;binary<2;binary++){
  s_system_binary=binary;s_page_refreshes=0;s_page_last_full_us=0;now=4000000;int prior=full_calls;
  for(int i=0;i<5;i++)assert(!main_periodic_cleanup(&hl,true,&result));
  for(int i=0;i<50;i++)assert(!main_periodic_cleanup(&hl,false,&result));
  outcome=EPD_DRAW_ERROR;assert(main_periodic_cleanup(&hl,true,&result));assert(result==EPD_DRAW_ERROR&&s_page_refreshes==6);
  outcome=EPD_DRAW_SUCCESS;assert(main_periodic_cleanup(&hl,true,&result));assert(result==EPD_DRAW_SUCCESS&&!s_page_refreshes&&s_page_last_full_us==now);
  assert(full_calls==prior+2);
  now+=1000000;for(int i=0;i<20;i++)assert(!main_periodic_cleanup(&hl,true,&result));
  now+=2000000;assert(main_periodic_cleanup(&hl,true,&result));assert(s_page_refreshes==0);
 }
 assert(normalized==3&&cancelled==6);puts("PASS: normal/BW clean with full vendor GC panel, unchanged frames do not count, min interval is honored, failed cleaning retries without losing debt");
}
'''
text=source.read_text();body=function('main_begin',source);assert body.index('main_periodic_cleanup(hl, changed, result)')<body.index('uint8_t *cache =');assert '!monochrome&&ui_page_cleanup_due' not in body
route=function('hl_update',source);assert '!s_system_binary&&whole_page' not in route;assert 'if(whole_page&&(mode&0xF)!=MODE_GC16&&ui_page_cleanup_due' in route
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(unit)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-parameter',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
