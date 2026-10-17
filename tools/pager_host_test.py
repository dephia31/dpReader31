#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
# 中文：真实共用分页栏像素、四键跳转与文件列表按压/取消，防止越入列表或主导航。
# English: Real shared-pager pixels, four-way targets and file press/cancel routing, bounded away from list rows and navigation.
from pathlib import Path
import re, subprocess, tempfile
from home_cover_cache_host_test import function

root=Path(__file__).resolve().parents[1]
kit=root/'main/ui/ui_kit.c'
files=root/'main/apps/app_files.c'
unit=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "assets/ui_icons.h"
#define FILE_ROWS 9
typedef struct {int x,y,width,height;} EpdRect;
enum {EPD_DRAW_ALIGN_CENTER,UI_GRAY_WHITE=255};
enum {APP_REDRAW_NONE,APP_REDRAW_AREA,APP_REDRAW_PAGE};
typedef int app_redraw_t;
typedef struct {uint8_t *fb;void *request_app;} app_ctx_t;
enum {FILE_VIEW_LIST,FILE_VIEW_ACTIONS,FILE_VIEW_RENAME,FILE_VIEW_MOVE};
enum {UI_GESTURE_PRESS,UI_GESTURE_MOVE,UI_GESTURE_TAP,UI_GESTURE_CANCEL,UI_GESTURE_LONG_PRESS,UI_GESTURE_SWIPE_L};
typedef struct {int type;uint16_t x0,y0,x,y;} ui_gesture_event_t;
static uint8_t pixels[684*1216];
static char drawn_label[96];
static int s_count,s_folder,s_page,s_view,s_pager_pressed=-1;
static bool s_scan_pending,s_pager_paint_pending;
static uint8_t ui_read_pixel(const uint8_t *fb,int x,int y){assert(x>=0&&x<684&&y>=0&&y<1216);return fb[y*684+x]>>4;}
static uint8_t ui_contrast_gray(uint8_t gray){return gray&240;}
static void epd_draw_pixel(int x,int y,uint8_t c,uint8_t *fb){assert(x>=36&&x<648&&y>=1006&&y<1056);fb[y*684+x]=c;}
static void ui_text_box(uint8_t *fb,EpdRect r,int px,const char *text,int align,bool inv){
 (void)fb;assert(r.x==212&&r.y==1006&&r.width==260&&r.height==50&&px==21&&align==EPD_DRAW_ALIGN_CENTER&&!inv);
 snprintf(drawn_label,sizeof(drawn_label),"%s",text);
}
static void ui_fill_rect_fast(EpdRect r,int c,uint8_t *fb){assert(r.y>=1006&&r.y+r.height<=1056);for(int y=r.y;y<r.y+r.height;++y)for(int x=r.x;x<r.x+r.width;++x)fb[y*684+x]=c;}
'''
header=(root/'main/ui/ui_kit.h').read_text()
unit+=re.search(r'enum \{ UI_PAGER_NONE=[^}]+\};',header)[0]+'\n'
for name in ('ui_rect_hit','icon_nibble','ui_draw_icon','ui_pager_area','ui_pager_rect','ui_pager_hit','ui_pager_target','ui_pager_draw'):
    unit+=function(name,kit)+'\n'
for name in ('visible_rows','root_rows','file_pager_pages','draw_file_pager','file_pager_paint','file_pager_event'):
    unit+=function(name,files)+'\n'
unit+=r'''
static unsigned ink_in(EpdRect r){unsigned n=0;for(int y=r.y;y<r.y+r.height;++y)for(int x=r.x;x<r.x+r.width;++x)n+=pixels[y*684+x]<128;return n;}
static void check_outside(void){for(int y=0;y<1216;++y)if(y<1006||y>=1056)for(int x=0;x<684;++x)assert(pixels[y*684+x]==240);}
static ui_gesture_event_t event(int type,int action){EpdRect r=ui_pager_rect(action);int x=r.x+r.width/2,y=r.y+r.height/2;return(ui_gesture_event_t){type,x,y,x,y};}
static void tap(app_ctx_t *ctx,int action,int expected){
 app_redraw_t redraw=-1;ui_gesture_event_t e=event(UI_GESTURE_PRESS,action);
 assert(file_pager_event(ctx,&e,&redraw)&&redraw==APP_REDRAW_AREA&&s_pager_pressed==action&&s_pager_paint_pending);
 int prior=s_page;e.type=UI_GESTURE_TAP;
 assert(file_pager_event(ctx,&e,&redraw)&&s_page==expected&&s_pager_pressed<0);
 assert(redraw==(prior==expected?APP_REDRAW_AREA:APP_REDRAW_PAGE));
}
int main(void){
 EpdRect area=ui_pager_area();assert(area.y==1006&&area.y+area.height==1056);
 // 四个触控区与文字各自分开，边界恰止于主导航触控条；大页数也不越界。
 // Four hit targets stay separate from the label and stop exactly before navigation, including large page counts.
 for(int a=0;a<4;++a){EpdRect r=ui_pager_rect(a);assert(r.x>=36&&r.x+r.width<=648&&r.y==1006&&r.height==50);
  for(int y=r.y;y<r.y+r.height;++y)for(int x=r.x;x<r.x+r.width;++x)assert(ui_pager_hit(x,y)==a);
  assert(ui_pager_hit(r.x,1005)<0&&ui_pager_hit(r.x,1056)<0);
 }
 assert(ui_pager_hit(342,1031)<0&&ui_pager_hit(683,1031)<0&&ui_pager_hit(0,1031)<0);
 for(int pages=1;pages<=999;pages+=17)for(int page=0;page<pages;++page){
  assert(ui_pager_target(UI_PAGER_FIRST,page,pages)==0&&ui_pager_target(UI_PAGER_LAST,page,pages)==pages-1);
  assert(ui_pager_target(UI_PAGER_PREVIOUS,page,pages)==(page?page-1:0));
  assert(ui_pager_target(UI_PAGER_NEXT,page,pages)==(page+1<pages?page+1:page));
  assert(ui_pager_target(-1,page,pages)==page);
 }
 assert(ui_pager_target(UI_PAGER_LAST,0,0)==0);
 // 真实Lucide像素：加粗轮廓比原图明显，按压只改箭头，绝不生成圆圈/黑色底框。
 // Real AnimateIcons/Lucide pixels: heavier contours, arrow-only presses and no circle or dark backing box.
 EpdRect previous=ui_pager_rect(UI_PAGER_PREVIOUS);
 memset(pixels,240,sizeof(pixels));ui_pager_draw(pixels,"10000本书 · 088/999",2,9,false,-1);unsigned plain=ink_in(previous);assert(plain>0);check_outside();
 memset(pixels,240,sizeof(pixels));ui_pager_draw(pixels,"10000本书 · 088/999",2,9,true,-1);unsigned outlined=ink_in(previous);assert(outlined>plain);check_outside();
 memset(pixels,240,sizeof(pixels));ui_pager_draw(pixels,"10000本书 · 088/999",2,9,true,UI_PAGER_PREVIOUS);assert(ink_in(previous)>outlined);check_outside();
 assert(pixels[1006*684+116]==240&&pixels[1055*684+203]==240);
 memset(pixels,240,sizeof(pixels));ui_pager_draw(pixels,"0本书 · 01/01",0,1,true,-1);for(int a=0;a<4;++a)assert(!ink_in(ui_pager_rect(a)));check_outside();
 app_ctx_t ctx={pixels,NULL};s_view=FILE_VIEW_LIST;s_folder=-1;s_count=96;s_page=5;
 assert(file_pager_pages()==20);tap(&ctx,UI_PAGER_FIRST,0);tap(&ctx,UI_PAGER_PREVIOUS,0);tap(&ctx,UI_PAGER_NEXT,1);tap(&ctx,UI_PAGER_LAST,19);tap(&ctx,UI_PAGER_NEXT,19);tap(&ctx,UI_PAGER_PREVIOUS,18);
 assert(strstr(drawn_label,"共 20 页"));s_folder=3;assert(visible_rows()==9);s_folder=0;s_page=2;assert(file_pager_pages()==11);tap(&ctx,UI_PAGER_LAST,10);
 s_folder=3;s_view=FILE_VIEW_MOVE;s_page=1;assert(file_pager_pages()==14);tap(&ctx,UI_PAGER_LAST,13);tap(&ctx,UI_PAGER_FIRST,0);
 app_redraw_t redraw;ui_gesture_event_t e=event(UI_GESTURE_PRESS,UI_PAGER_NEXT);file_pager_event(&ctx,&e,&redraw);
 e.type=UI_GESTURE_CANCEL;assert(file_pager_event(&ctx,&e,&redraw)&&redraw==APP_REDRAW_AREA&&!s_page&&s_pager_pressed<0);
 e=event(UI_GESTURE_PRESS,UI_PAGER_NEXT);file_pager_event(&ctx,&e,&redraw);e.type=UI_GESTURE_MOVE;e.x=340;
 assert(file_pager_event(&ctx,&e,&redraw)&&redraw==APP_REDRAW_AREA&&s_pager_pressed<0);e.type=UI_GESTURE_TAP;assert(file_pager_event(&ctx,&e,&redraw)&&redraw==APP_REDRAW_NONE&&!s_page);
 s_scan_pending=true;e=event(UI_GESTURE_TAP,UI_PAGER_LAST);assert(file_pager_event(&ctx,&e,&redraw)&&redraw==APP_REDRAW_NONE&&!s_page);s_scan_pending=false;
 s_view=FILE_VIEW_ACTIONS;assert(!file_pager_event(&ctx,&e,&redraw));s_view=FILE_VIEW_RENAME;assert(!file_pager_event(&ctx,&e,&redraw));
 printf("PASS: shared four-button geometry/targets, real outlined icon ink %u -> %u, navigation/list boundaries; root/subfolder/move paging, empty/single pages, press/release/cancel and scan/modal guards\n",plain,outlined);
}
'''
with tempfile.TemporaryDirectory() as folder:
    path=Path(folder)/'test.c';path.write_text(unit);exe=path.with_suffix('')
    subprocess.run(['cc','-std=gnu11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(root/'main'),str(path),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)

file_source=files.read_text()
assert '设为书籍目录' not in file_source and '设为字体目录' not in file_source
assert 'app_settings_set_books_dir' not in file_source and 'app_settings_set_fonts_dir' not in file_source
assert '214 + row * 88' in file_source and file_source.count('(ev->y0 - 214) / 88')==2
assert 214+8*88+82 < 1006
