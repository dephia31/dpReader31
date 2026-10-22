#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
"""中文：实际底栏三模式灰底，Pico不继承S31二值作用域。/ English: Actual gray navigation in all modes, without S31 binary scope on Pico."""
from pathlib import Path
import subprocess,tempfile
from home_cover_cache_host_test import function
root=Path(__file__).resolve().parents[1]
unit=r'''
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#include "ui_nav_layout.h"
#define UI_LOCK_WIDTH 684
#define UI_LOCK_HEIGHT 1216
#define UI_GRAY_LIGHT 0xc0
#define UI_GRAY_WHITE 0xff
#define UI_GRAY_BLACK 0
#define EPD_DRAW_ALIGN_CENTER 1
enum {APP_MAIN_REFRESH_NORMAL,APP_MAIN_REFRESH_FAST,APP_MAIN_REFRESH_WATER};
typedef struct {int x,y,width,height;} EpdRect;
static uint8_t frame[684*1216];static int mode;
static const char *labels[]={"home","book","files","settings"};
static int app_settings_main_refresh_mode(void){return mode;}
static void ui_fill_rect_fast(EpdRect r,int gray,uint8_t *fb){assert(r.x>=0&&r.y>=0&&r.x+r.width<=684&&r.y+r.height<=1216);for(int y=r.y;y<r.y+r.height;++y)for(int x=r.x;x<r.x+r.width;++x)fb[y*684+x]=gray;}
static void ui_hairline(uint8_t *fb,int y,int x,int width,int gray){ui_fill_rect_fast((EpdRect){x,y,width,1},gray,fb);}
static void icon(uint8_t *fb,int tab,int x,int y){(void)fb;(void)tab;(void)x;(void)y;}
static void ui_text(uint8_t *fb,int x,int y,int px,const char *label,int align,bool inverse){(void)fb;(void)x;(void)y;(void)px;(void)label;(void)align;(void)inverse;}
'''+function('ui_nav_draw',root/'main/ui/ui_nav.c')+r'''
int main(void){
 for(mode=0;mode<3;++mode)for(int tab=0;tab<4;++tab){
  ui_nav_draw(frame,tab);
  for(int y=UI_NAV_TOP+8;y<1216;++y)for(int x=0;x<684;++x)assert(frame[y*684+x]==0xe0);
  assert(frame[(UI_NAV_TOP+2)*684+(tab*2+1)*684/8]==0);
 }
 puts("PASS: actual root navigation retains0xe0 gray background for all4tabs in normal/fast/water; marker-only black region");
}
'''
with tempfile.TemporaryDirectory() as directory:
 p=Path(directory);(p/'test.c').write_text(unit)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(root/'main/ui'),str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
loop=(root/'main/app/app_loop.c').read_text()
assert 'display_system_binary_set' not in loop, 'Run in the board-selected Pico host view'
