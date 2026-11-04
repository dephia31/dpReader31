#!/usr/bin/env python3
"""The Home status clock must never repaint the removed calendar/widget area."""
from pathlib import Path
import subprocess,tempfile
s=(Path(__file__).resolve().parents[1]/'main/apps/app_dashboard.c').read_text()
a=s.index('    if(redraw==APP_REDRAW_AREA&&s_clock_pending) {');b=s.index('    s_clock_pending=s_widget_pending=s_component_pending=false;',a)
unit=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
enum {APP_REDRAW_AREA=1,EPD_DRAW_SUCCESS=0};
enum EpdDrawError {ERR=1};
typedef struct{int x,y,width,height;} EpdRect;
typedef struct{void *hl;uint8_t *fb;}app_ctx_t;
static bool s_clock_pending,fail,full;
enum {HOME_ROOT};static int s_view,s_ticket_battery;
static bool app_settings_simple_full_ui(void){return full;}
static EpdRect simple_ticket_status_rect(void){return (EpdRect){60,80,564,58};}
static void draw_simple_ticket_status(uint8_t *fb){fb[0]=80;}static int64_t s_clock_minute;static unsigned calls,paints,guards;
static void ui_fill_rect_fast(EpdRect r,int gray,uint8_t *fb){(void)gray;(void)fb;assert(r.x==24&&r.y==18&&r.width==156&&r.height==44);++paints;}
static void ui_nav_clock_draw(uint8_t *fb){fb[0]=71;}
static enum EpdDrawError present_clock_area(app_ctx_t *ctx,EpdRect r){(void)ctx;assert(full?(r.x==60&&r.y==80&&r.width==564&&r.height==58):(r.x==24&&r.y==18&&r.width==156&&r.height==44));++calls;return fail?ERR:EPD_DRAW_SUCCESS;}
static void guard_draw_result(void *hl,enum EpdDrawError e){(void)hl;(void)e;++guards;}
static bool run(app_ctx_t *ctx){int redraw=APP_REDRAW_AREA;int64_t minute=1234;
'''+s[a:b]+r'''
return false;}
int main(void){uint8_t fb[1]={0};app_ctx_t ctx={NULL,fb};
 s_clock_pending=true;s_clock_minute=1233;fail=true;assert(run(&ctx)&&s_clock_minute==1233&&calls==1&&guards==1&&!s_clock_pending);
 s_clock_pending=true;fail=false;assert(run(&ctx)&&s_clock_minute==1234&&calls==2&&guards==2&&paints==2&&fb[0]==71);
 full=true;s_clock_pending=true;assert(run(&ctx)&&calls==3&&paints==2&&fb[0]==80);
 puts("PASS Home minute refresh: only status clock area, single output, failed output retries without consuming minute");}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(unit)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
