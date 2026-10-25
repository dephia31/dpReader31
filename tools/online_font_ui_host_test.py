#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
"""Exercise the production online-font hit geometry, cancel ownership and progress throttling."""
from pathlib import Path
import subprocess,tempfile
from home_cover_cache_host_test import function
ROOT=Path(__file__).resolve().parents[1]
source=ROOT/'main/ui/ui_online_fonts.c'
functions=['back_rect','row_rect','download_rect','status_rect','return_rect','cancel_button',
           'ui_online_fonts_entry_rect','ui_online_fonts_open','ui_online_fonts_close','percent',
           'ui_online_fonts_gesture','ui_online_fonts_tick']
code=r'''
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "online_font.h"
typedef struct {int x,y,width,height;} EpdRect;
typedef enum {APP_REDRAW_NONE,APP_REDRAW_AREA,APP_REDRAW_PAGE,APP_REDRAW_FULL,APP_REDRAW_DONE} app_redraw_t;
typedef enum {UI_GESTURE_PRESS,UI_GESTURE_MOVE,UI_GESTURE_LONG_PRESS,UI_GESTURE_TAP,UI_GESTURE_SWIPE_L,UI_GESTURE_SWIPE_R,UI_GESTURE_SWIPE_U,UI_GESTURE_SWIPE_D,UI_GESTURE_CANCEL} ui_gesture_type_t;
typedef struct {ui_gesture_type_t type;uint16_t x0,y0,x,y;int64_t hold_ms;} ui_gesture_event_t;
typedef struct {online_font_status_t status;bool installed[5],reading,closed,confirm_cancel,leave_after_cancel;int64_t updated_ms;unsigned painted_percent;} ui_online_fonts_t;
static online_font_status_t live;static bool installed[5];static unsigned starts,joins;
static bool ui_rect_hit(EpdRect r,int x,int y){return x>=r.x&&x<r.x+r.width&&y>=r.y&&y<r.y+r.height;}
void online_font_status(online_font_status_t *out){*out=live;}
bool online_font_installed(unsigned i){assert(i<5);return installed[i];}
esp_err_t online_font_start(unsigned i){assert(i<5);++starts;live=(online_font_status_t){.phase=ONLINE_FONT_CONNECTING,.item=i,.total=100,.revision=live.revision+1,.busy=true};return ESP_OK;}
void online_font_cancel_join(void){++joins;live.busy=false;live.phase=ONLINE_FONT_CANCELLED;++live.revision;}
'''
code+='\n'.join(function(name,source) for name in functions)
code+=r'''
static ui_gesture_event_t tap(EpdRect r){return (ui_gesture_event_t){.type=UI_GESTURE_TAP,.x0=r.x+r.width/2,.x=r.x+r.width/2,.y0=r.y+r.height/2,.y=r.y+r.height/2};}
int main(void){
 for(int reading=0;reading<2;++reading)for(unsigned i=0;i<5;++i){
  memset(installed,0,sizeof(installed));live=(online_font_status_t){0};ui_online_fonts_t ui;ui_online_fonts_open(&ui,reading);starts=joins=0;
  EpdRect r=row_rect(i),b=download_rect(i);assert(r.y>=242&&r.y+r.height<=842&&b.x>=r.x&&b.x+b.width<=r.x+r.width);
  ui_gesture_event_t event=tap(b);event.type=UI_GESTURE_CANCEL;assert(ui_online_fonts_gesture(&ui,&event)==APP_REDRAW_NONE&&!starts);
  event=tap(b);event.x=b.x-1;assert(ui_online_fonts_gesture(&ui,&event)==APP_REDRAW_NONE&&!starts);
  event=tap(b);assert(ui_online_fonts_gesture(&ui,&event)==APP_REDRAW_PAGE&&starts==1&&ui.status.busy&&ui.status.item==i);
  event=tap(download_rect((i+1)%5));assert(ui_online_fonts_gesture(&ui,&event)==APP_REDRAW_NONE&&starts==1);
  event=tap(b);assert(ui_online_fonts_gesture(&ui,&event)==APP_REDRAW_PAGE&&ui.confirm_cancel&&!ui.leave_after_cancel);
  event=tap(cancel_button(false));assert(ui_online_fonts_gesture(&ui,&event)==APP_REDRAW_PAGE&&!ui.confirm_cancel&&!joins);
  event=tap(return_rect());assert(ui_online_fonts_gesture(&ui,&event)==APP_REDRAW_PAGE&&ui.confirm_cancel&&ui.leave_after_cancel);
  event=tap(cancel_button(true));assert(ui_online_fonts_gesture(&ui,&event)==APP_REDRAW_PAGE&&ui.closed&&joins==1);
  ui_online_fonts_open(&ui,reading);installed[i]=true;
  live=(online_font_status_t){.phase=ONLINE_FONT_DOWNLOADING,.item=i,.total=100,.received=10,.revision=50,.busy=true};
  assert(ui_online_fonts_tick(&ui,1000)==APP_REDRAW_PAGE);
  live.received=11;++live.revision;assert(ui_online_fonts_tick(&ui,1100)==APP_REDRAW_NONE);
  assert(ui_online_fonts_tick(&ui,1250)==APP_REDRAW_AREA);
  ++live.revision;assert(ui_online_fonts_tick(&ui,1600)==APP_REDRAW_NONE);
  live.phase=ONLINE_FONT_DONE;live.busy=false;++live.revision;assert(ui_online_fonts_tick(&ui,1700)==APP_REDRAW_PAGE&&ui.installed[i]);
  starts=0;event=tap(b);assert(ui_online_fonts_gesture(&ui,&event)==APP_REDRAW_NONE&&!starts);
  ui_online_fonts_close(&ui);assert(ui.closed);
 }
 puts("PASS: five shared row/button bounds, both entries, cancelled/outside releases, single-worker admission, return/cancel/join, downloaded guard and250ms/percent progress throttle (ASan/UBSan)");
}
'''
with tempfile.TemporaryDirectory() as d:
    w=Path(d);(w/'test.c').write_text(code)
    (w/'esp_err.h').write_text('#pragma once\ntypedef int esp_err_t;\n#define ESP_OK 0\n')
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-fsanitize=address,undefined',
                    '-I'+str(w),'-I'+str(ROOT/'main/font'),str(w/'test.c'),'-o',str(w/'test')],check=True)
    subprocess.run([str(w/'test')],check=True)
