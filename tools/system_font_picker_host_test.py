#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""中文：真实S31系统字体绘制/命中覆盖所有字号和分页，不依赖设备。/ English: Exercise real S31 font-picker paint/hits at every scale and page."""
from pathlib import Path
import subprocess,tempfile
from board_source_view import select_source
from home_cover_cache_host_test import function
root=Path(__file__).resolve().parents[1]
s=select_source((root/'main/apps/app_device_settings.c').read_text(),'metalio47')
def block(at):
 start=s.index('{',at);depth=1;at=start+1
 while depth:
  depth+=(s[at]=='{')-(s[at]=='}');at+=1
 return s[start:at]
render_start=s.index('static void render_content(')
paint=block(s.index('if (s_page == SETTINGS_SYSTEM_FONT)',render_start))
gesture=s.index('static app_redraw_t on_gesture(')
choose=block(s.index('if (s_page == SETTINGS_SYSTEM_FONT) {',gesture))
modal=block(s.index('if (s_page==SETTINGS_SYSTEM_FONT && s_font_sync_confirm)',gesture))
# Extract helpers from a selected source file so nested board guards do not leak into the probe.
with tempfile.TemporaryDirectory() as folder:
 p=Path(folder);src=p/'selected.c';src.write_text(s)
 unit=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef struct {int x,y,width,height;} EpdRect;
typedef int app_redraw_t;
enum {APP_REDRAW_NONE,APP_REDRAW_PAGE,APP_REDRAW_AREA,UI_GESTURE_TAP,SETTINGS_SYSTEM_FONT};
enum {EPD_DRAW_ALIGN_LEFT,EPD_DRAW_ALIGN_CENTER,EPD_DRAW_ALIGN_RIGHT,UI_ICON_ROTATE_CW,UI_ICON_CHEVRON_LEFT,UI_ICON_CHEVRON_RIGHT};
#define UI_GRAY_WHITE 255
#define UI_GRAY_BLACK 0
#define UI_NAV_TOP 1096
#define TTF_FONT_NAME_MAX 160
#define SETTINGS_SCROLL_MAX 4000
static int s_page=SETTINGS_SYSTEM_FONT,scale=100,s_font_page,s_main_scroll,count;
static bool s_scroll_present_pending,s_font_sync_confirm,write_fail;
static char s_notice[96],s_font_sync_path[192],saved[192],reading[192]="reader.ttf";
static int cards,font_rows,activations,total_labels;
static EpdRect font_boxes[8];
static bool ui_rect_hit(EpdRect r,int x,int y){return x>=r.x&&x<r.x+r.width&&y>=r.y&&y<r.y+r.height;}
typedef struct{char path[192];}ttf_font_item_t;
static ttf_font_item_t font;
static int ttf_font_count(void){return count;}
static const ttf_font_item_t*ttf_font_item(int i){if(i<0||i>=count)return NULL;snprintf(font.path,sizeof(font.path),"/fonts/Font-%d.ttf",i);return &font;}
static unsigned app_settings_system_font_size(void){return scale;}
static void app_settings_set_system_font_size(unsigned n){if(!write_fail)scale=n;}
static int ui_text_effective_px(int px){return ((px+2)*scale+50)/100;}
static const char*app_settings_system_font_path(void){return saved;}
static const char*app_settings_font_path(void){return reading;}
static void app_settings_set_system_font_path(const char*p){if(!write_fail)snprintf(saved,sizeof(saved),"%s",p);}
static void app_settings_set_font_path(const char*p){if(!write_fail)snprintf(reading,sizeof(reading),"%s",p);}
static void app_font_activate_system(void){++activations;}
static void ui_text_set_system_scale(bool enabled){assert(enabled);}
static const char*system_font_label(const char *p){return p;}
static int ui_text_fixed_width_px(int px,const char*t){int n=0;for(;*t;++t)if((*t&192)!=128)++n;return n*px;}
static void settings_card(uint8_t*f,EpdRect r,int radius,int fill,int edge){(void)f;(void)radius;(void)fill;(void)edge;++cards;if(r.y>=640&&r.y<1020){assert(font_rows<8);font_boxes[font_rows++]=r;}}
static void back_header(uint8_t*f,const char*t){(void)f;(void)t;}
static void ui_online_fonts_draw_entry(uint8_t*f,int y){(void)f;(void)y;}
static void ui_draw_icon(uint8_t*f,int x,int y,int px,int icon,int gray){(void)f;(void)x;(void)y;(void)px;(void)icon;(void)gray;}
static void section(uint8_t*f,int y,const char*t){(void)f;(void)y;(void)t;}
static void epd_fill_circle(int x,int y,int r,int g,uint8_t*f){(void)x;(void)y;(void)r;(void)g;(void)f;}
static void ui_text_vc(uint8_t*f,int x,int cy,int px,const char*t,int align,bool inverted){(void)f;(void)align;(void)inverted;
 if(!strncmp(t,"共 ",4))++total_labels;
 if(cy==441||cy==510){assert(cy-ui_text_effective_px(px)/2>=398&&cy+ui_text_effective_px(px)/2<=552);}
 if(cy>=640&&cy<1020){assert(x==60&&ui_text_fixed_width_px(ui_text_effective_px(px),t)<=512);}
}
static void ui_text_box(uint8_t*f,EpdRect r,int px,const char*t,int a,bool b){(void)f;(void)px;(void)t;(void)a;(void)b;
 if(r.y>=640&&r.y<1020){assert(r.x==60&&r.width==512&&r.height>0&&r.y+r.height<1030);}
}
static void settings_navigation(uint8_t*f){(void)f;}
static void draw_system_font_sync(uint8_t*f){(void)f;}
'''
 layout=s[s.index('typedef struct {\n    EpdRect size, preview;'):s.index('static system_font_layout_t system_font_layout(')]
 unit+=layout
 for n in ('settings_viewport_end','system_font_layout','system_font_item_rect','system_font_page_button','system_font_sync_button','settings_fit_text'):
  unit+=function(n,src)+'\n'
 unit+='static void paint(void){uint8_t pixel=0;uint8_t*fb=&pixel;'+paint+'}\n'
 unit+='static app_redraw_t tap(int x,int y){struct{int type,x0,y0;}e={UI_GESTURE_TAP,x,y};const typeof(e)*ev=&e;'+'if(s_font_sync_confirm)'+modal+choose+'return APP_REDRAW_NONE;}\n'
 unit+=r'''
int main(void){
 const int amounts[]={0,1,3,4,5,17,128};
 for(scale=100;scale<=200;scale+=10)for(unsigned a=0;a<sizeof(amounts)/sizeof(amounts[0]);++a){
  count=amounts[a];s_font_page=0;s_font_sync_confirm=false;
  system_font_layout_t l=system_font_layout();assert(l.slots>=3);
  EpdRect prev=system_font_page_button(false),next=system_font_page_button(true);
  assert(prev.y>=system_font_item_rect(l.slots-1).y+system_font_item_rect(l.slots-1).height+8);
  assert(next.y+next.height<UI_NAV_TOP);
  int visited=0,pages=(count+l.slots)/l.slots;
  for(int page=0;page<pages;++page){
   cards=font_rows=total_labels=0;paint();assert(total_labels==1);
   int expected=count+1-page*l.slots;if(expected>l.slots)expected=l.slots;
   assert(font_rows==expected);
   for(int i=0;i<font_rows;++i){
    EpdRect r=font_boxes[i];assert(r.y==system_font_item_rect(i).y&&r.y+r.height<=prev.y);
    assert(tap(300,r.y+r.height/2)==APP_REDRAW_PAGE);++visited;
    assert(!strcmp(reading,"reader.ttf"));s_font_sync_confirm=false;
    if(i+1<font_rows)assert(tap(300,r.y+r.height+3)==APP_REDRAW_NONE);
   }
   assert(tap(next.x+20,next.y+20)==(page+1<pages?APP_REDRAW_AREA:APP_REDRAW_NONE));
   assert(s_font_page==(page+1<pages?page+1:page));
  }
  assert(visited==count+1);
  while(s_font_page)assert(tap(prev.x+20,prev.y+20)==APP_REDRAW_AREA);
  assert(tap(prev.x+20,prev.y+20)==APP_REDRAW_NONE);
 }
 scale=150;count=17;s_font_page=3;s_font_sync_confirm=false;
 EpdRect size=system_font_layout().size;assert(tap(300,size.y+20)==APP_REDRAW_PAGE&&scale==160&&s_font_page==0);
 write_fail=true;assert(tap(300,size.y+20)==APP_REDRAW_PAGE&&scale==160&&s_notice[0]);
 puts("PASS: actual S31 font-picker render/hits at11 scales,0–128 imports,4 rows,all fonts reachable,disabled page bounds,preview fit,opt-in reading sync and save-failure behavior");
}
'''
 (p/'test.c').write_text(unit)
 subprocess.run(['cc','-std=gnu11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
