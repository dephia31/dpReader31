#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Exercise the actual Home minute branch: identical paint, one output, retry on failure."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1]
s=(r/'main/apps/app_dashboard.c').read_text();start=s.index('    if(redraw==APP_REDRAW_AREA&&s_clock_pending) {');at=s.index('{',start)+1;depth=1
while depth:
 depth+=(s[at]=='{')-(s[at]=='}');at+=1
branch=s[start:at]
prefix=r'''
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <time.h>
#define APP_REDRAW_AREA 1
#define HOME_ROOT 0
#define APP_HOME_FOLDER 3
#define UI_HOME_CLOCK_PS1995 0
#define MODE_GL16 5
#define EPD_DRAW_SUCCESS 0
enum EpdDrawError {ERR=1};
typedef struct{int x,y,width,height;}EpdRect;
typedef struct{void*hl;uint8_t*fb;}app_ctx_t;
typedef int ui_home_clock_style_t;
static bool s_clock_pending,pipeline,fail;
static int s_view,component,s_widget_phase,calls,guards;
static int64_t s_clock_minute;
static struct tm s_date;
static int E0470_WAVEFORM;
static bool display_pipeline_enabled(void){return pipeline;}
static int app_settings_home_component(void){return component;}
static bool app_settings_main_fast_refresh(void){return false;}
static void ui_fill_rect_fast(EpdRect r,unsigned x,uint8_t*fb){(void)r;(void)x;fb[0]=0;}
static void ui_nav_clock_draw(uint8_t*fb){fb[1]=71;}
static void date_now(struct tm*d){d->tm_min=37;}
static int clock_style(int c){return c;}
static void clock_draw_region(uint8_t*fb,EpdRect r,struct tm*d,bool b,int s,EpdRect clip,unsigned p){(void)r;(void)d;(void)b;(void)s;(void)clip;(void)p;fb[2]=83;}
static EpdRect component_clock_graphics(void){return(EpdRect){24,640,320,326};}
static EpdRect ui_rect_union(EpdRect a,EpdRect b){int x=a.x<b.x?a.x:b.x,y=a.y<b.y?a.y:b.y;int right=a.x+a.width>b.x+b.width?a.x+a.width:b.x+b.width,bottom=a.y+a.height>b.y+b.height?a.y+a.height:b.y+b.height;return(EpdRect){x,y,right-x,bottom-y};}
static enum EpdDrawError present_clock_area(app_ctx_t*c,EpdRect r){(void)c;assert(r.x>=0&&r.y>=0&&r.x+r.width<=684&&r.y+r.height<=1216);++calls;return fail?ERR:0;}
static enum EpdDrawError update_display_area_diff_with(void*h,void*w,int m,EpdRect r){(void)h;(void)w;(void)m;return present_clock_area(NULL,r);}
static void guard_draw_result(void*h,enum EpdDrawError e){(void)h;(void)e;++guards;}
static bool run(app_ctx_t*ctx){int redraw=APP_REDRAW_AREA;int64_t minute=1234;
'''
unit=prefix+branch+r'''
return false;}
int main(void){
 for(s_view=0;s_view<2;++s_view)for(component=0;component<7;++component){
  uint8_t reference[8],actual[8];memset(reference,0xa5,8);memcpy(actual,reference,8);app_ctx_t c={NULL,reference};
  pipeline=false;fail=false;calls=guards=0;s_clock_pending=true;s_clock_minute=1233;assert(run(&c));unsigned old=calls;assert(guards==1&&s_clock_minute==1234);
  pipeline=true;calls=guards=0;s_clock_pending=true;s_clock_minute=1233;c.fb=actual;assert(run(&c));assert(guards==1&&calls==1&&!s_clock_pending&&s_clock_minute==1234&&!memcmp(actual,reference,8));
  assert(old==(unsigned)(s_view==HOME_ROOT&&component!=APP_HOME_FOLDER?2:1));
  fail=true;calls=guards=0;s_clock_pending=true;s_clock_minute=1233;assert(run(&c));assert(calls==1&&guards==1&&s_clock_minute==1233);
 }
 puts("PASS: actual Home minute branch retains painted target, merges header/static-clock outputs, preserves folder/subview scope and retries failed display");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(unit)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-DKIIKO_BOARD_METALIO47=1','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
