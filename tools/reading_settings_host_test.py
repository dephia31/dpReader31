#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# 中文：验证已确认的阅读设置共用矩形、点按周期、二级页与间隙不穿透。
# English: Validate approved reading-sheet geometry, cycles, secondary destinations and inert gaps.
from pathlib import Path
import subprocess
import tempfile
from home_cover_cache_host_test import function

ROOT = Path(__file__).resolve().parents[1]
unit = r'''
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
typedef struct {int x,y,width,height;} EpdRect;
typedef struct {int unused;} app_ctx_t;
typedef enum {APP_REDRAW_NONE,APP_REDRAW_PAGE} app_redraw_t;
enum {READER_PANEL_NONE,READER_PANEL_REFRESH_SETTINGS,READER_PANEL_TURN_SETTINGS,READER_PANEL_KEY_SETTINGS};
static int s_reader_panel, s_turns, invalidates, toggles[3], full_pages = 10, effect;
static bool ui_rect_hit(EpdRect r,unsigned x,unsigned y){return x>=(unsigned)r.x&&x<(unsigned)(r.x+r.width)&&y>=(unsigned)r.y&&y<(unsigned)(r.y+r.height);}
static unsigned app_settings_reader_full_pages(void){return full_pages;}
static void app_settings_set_reader_full_pages(unsigned n){full_pages=n;}
static unsigned app_settings_reader_turn_effect(void){return effect;}
static void app_settings_set_reader_turn_effect(unsigned n){effect=n;}
static app_redraw_t apply_reader_option(app_ctx_t *ctx,int i){(void)ctx;assert(i>=0&&i<3);++toggles[i];return APP_REDRAW_PAGE;}
static void invalidate_prep(void){++invalidates;}
'''
source = ROOT/'main/apps/app_book.c'
unit+='static unsigned app_settings_system_font_size(void){return 100;}\n'
for name in ('reader_ui_extra', 'reading_settings_top', 'reading_setting_rect', 'reading_toggle_rect', 'reading_settings_action'):
    unit += function(name, source)+'\n'
unit += r'''
int main(void){
    app_ctx_t ctx={0};
    // 7个控件位于半屏内、不重叠；按键控制严格在翻页模式下一行。
    // Seven controls stay inside the half sheet without overlap; key controls sit directly below turn mode.
    for(int i=0;i<7;++i){
        EpdRect a=reading_setting_rect(i);assert(a.x>=36&&a.x+a.width<=648&&a.y>=702&&a.y+a.height<=1122);
        for(int j=i+1;j<7;++j){EpdRect b=reading_setting_rect(j);assert(a.x+a.width<=b.x||b.x+b.width<=a.x||a.y+a.height<=b.y||b.y+b.height<=a.y);}
    }
    EpdRect mode=reading_setting_rect(2),keys=reading_setting_rect(4);
    assert(keys.x==mode.x&&keys.y>mode.y&&keys.y==926);
    const unsigned expected[]={15,30,0,5,10};
    for(unsigned i=0;i<5;++i){s_turns=100;assert(reading_settings_action(&ctx,120,750)==APP_REDRAW_PAGE);assert(full_pages==(int)expected[i]&&!s_turns);}
    full_pages=77;reading_settings_action(&ctx,120,750);assert(full_pages==5);
    for(unsigned i=0;i<6;++i){reading_settings_action(&ctx,450,750);assert(effect==(int)((i+1)%2));}
    s_reader_panel=READER_PANEL_REFRESH_SETTINGS;
    reading_settings_action(&ctx,120,850);assert(s_reader_panel==READER_PANEL_TURN_SETTINGS);
    s_reader_panel=READER_PANEL_REFRESH_SETTINGS;
    reading_settings_action(&ctx,120,970);assert(s_reader_panel==READER_PANEL_KEY_SETTINGS);
    for(int i=0;i<3;++i){EpdRect a=reading_toggle_rect(i);assert(reading_settings_action(&ctx,a.x+a.width-5,a.y+a.height-5)==APP_REDRAW_PAGE&&toggles[i]==1);}
    s_reader_panel=READER_PANEL_REFRESH_SETTINGS;int before=invalidates;
    assert(reading_settings_action(&ctx,342,760)==APP_REDRAW_NONE);
    assert(reading_settings_action(&ctx,120,807)==APP_REDRAW_NONE);
    assert(reading_settings_action(&ctx,120,1130)==APP_REDRAW_NONE);
    assert(s_reader_panel==READER_PANEL_REFRESH_SETTINGS&&invalidates==before);
    reading_settings_action(&ctx,342,1170);assert(s_reader_panel==READER_PANEL_NONE);
    s_reader_panel=READER_PANEL_REFRESH_SETTINGS;
    reading_settings_action(&ctx,342,579);assert(s_reader_panel==READER_PANEL_NONE);
    puts("PASS: approved half-sheet bounds, nonoverlapping shared hit areas, key controls below turn mode, full-refresh/effect cycles, toggles, secondary pages, Done and inert gaps");
}
'''
with tempfile.TemporaryDirectory(dir=ROOT/'build') as folder:
    c,exe=Path(folder)/'test.c',Path(folder)/'test'
    c.write_text(unit)
    subprocess.run(['cc','-std=gnu11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(c),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
