#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
"""中文：实绘图示、点击/保存与真实盖板键逐列对应，保持原存储编号。
English: Match actual drawing, picker save and physical touch keys column by column while retaining persisted IDs.
"""
from pathlib import Path
import subprocess,tempfile
from home_cover_cache_host_test import function
root=Path(__file__).resolve().parents[1]
book=root/'main/apps/app_book.c'
settings=(root/'main/settings.h').read_text()
a=settings.index('typedef enum {\n    APP_READER_KEY_PREV')
b=settings.index('} app_reader_key_action_t;',a)+len('} app_reader_key_action_t;')
u=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef struct {int x,y,width,height;}EpdRect;
typedef struct {int unused;}app_ctx_t;
typedef enum {APP_REDRAW_NONE,APP_REDRAW_PAGE}app_redraw_t;
enum {READER_PANEL_NONE,READER_PANEL_KEY_SETTINGS,READER_PANEL_KEY_PICKER,READER_PANEL_REFRESH_SETTINGS};
enum {EPD_DRAW_ALIGN_CENTER,EPD_DRAW_ALIGN_LEFT,EPD_DRAW_ALIGN_RIGHT,UI_ICON_CHEVRON_DOWN};
#define UI_GRAY_BLACK 0
#define UI_GRAY_WHITE 255
#define UI_KEY_1 0
#define UI_KEY_2 1
#define UI_KEY_3 2
static unsigned s_reader_key_pick;
static int s_reader_panel=READER_PANEL_KEY_SETTINGS;
static char s_reader_key_error[64],labels[3][32],actions[3][32],title[48],hold_label[48],preset_labels[2][96];
static unsigned rings,bars,invalidations;
static bool save_fail;
static int saved_key=-1,saved_value=-1,hold_saved=-1;
static void draw_sheet(uint8_t *fb,int y,const char *t){(void)fb;assert(y==270);snprintf(title,sizeof(title),"%s",t);}
static void draw_sheet_back(uint8_t *fb,int y){(void)fb;assert(y==270);}
static void epd_draw_circle(int x,int y,int radius,int color,uint8_t *fb){(void)fb;(void)radius;(void)color;if(y==418){assert(x==134);++rings;}}
static void epd_fill_circle(int x,int y,int radius,int color,uint8_t *fb){(void)x;(void)y;(void)radius;(void)color;(void)fb;}
static void ui_fill_round_rect(uint8_t *fb,EpdRect r,int radius,int color){(void)fb;(void)radius;(void)color;if(r.y==415){assert(r.x==342-26||r.x==550-26);++bars;}}
static void ui_draw_control_frame(uint8_t *fb,EpdRect r,int radius,int color){(void)fb;(void)r;(void)radius;(void)color;}
static void ui_draw_icon(uint8_t *fb,int x,int y,int size,int icon,int color){(void)fb;(void)x;(void)y;(void)size;(void)icon;(void)color;}
static void reader_panel_text_vc(uint8_t *fb,int x,int y,int size,const char *text,int align,bool inv){(void)fb;(void)size;(void)align;(void)inv;if(y==473){int column=(x-134)/208;assert(column>=0&&column<3);snprintf(labels[column],32,"%s",text);}}
static void reader_panel_text(uint8_t *fb,int x,int y,int size,const char *text,int align,bool inv){(void)fb;(void)x;(void)y;(void)size;(void)text;(void)align;(void)inv;}
static void ui_text_box(uint8_t *fb,EpdRect r,int size,const char *text,int align,bool inv){(void)fb;(void)size;(void)align;(void)inv;
 if(r.y==558){int column=(r.x-48)/208;assert(column>=0&&column<3);snprintf(actions[column],32,"%s",text);}
 if(r.x==58&&r.y==652)snprintf(hold_label,sizeof(hold_label),"%s",text);
 if(r.x==58&&(r.y==869||r.y==1017))snprintf(preset_labels[(r.y-869)/148],96,"%s",text);
}
static bool ui_rect_hit(EpdRect r,int x,int y){return x>=r.x&&x<r.x+r.width&&y>=r.y&&y<r.y+r.height;}
static void copy_text(char *to,size_t cap,const char *text){snprintf(to,cap,"%s",text);}
static void invalidate_prep(void){++invalidations;}
'''
u+=settings[a:b]+r'''
static app_reader_key_action_t values[3]={APP_READER_KEY_PREV,APP_READER_KEY_TOOLS,APP_READER_KEY_NEXT};
static app_reader_key_action_t held=APP_READER_KEY_HOME;
static app_reader_key_action_t app_settings_reader_key_action(unsigned i){assert(i<3);return values[i];}
static app_reader_key_action_t app_settings_reader_hold_action(void){return held;}
static bool app_settings_set_reader_key_action(unsigned i,app_reader_key_action_t v){assert(i<3);if(save_fail)return false;saved_key=i;saved_value=v;values[i]=v;return true;}
static bool app_settings_set_reader_hold_action(app_reader_key_action_t v){if(save_fail)return false;hold_saved=v;held=v;return true;}
static void app_settings_set_reader_key_preset(unsigned p){assert(p==1||p==2);}
'''
for name in ('reader_key_label','reader_key_card','reader_key_preset_card','reader_key_preset_selected','draw_key_settings','draw_key_picker'):
 u+=function(name,book)+'\n'
u+=function('ui_key_hit_test',root/'main/ui/ui_menu.c')+'\n'
action=function('reader_panel_action',book)
a=action.index('    if (s_reader_panel == READER_PANEL_KEY_SETTINGS || s_reader_panel == READER_PANEL_KEY_PICKER) {')
b=action.index('    if (s_reader_panel == READER_PANEL_TURN_SETTINGS)',a)
u+='static app_redraw_t key_action(app_ctx_t *ctx,int x,int y){(void)ctx;\n'+action[a:b]+'return APP_REDRAW_NONE;}\n'
u+=r'''
int main(void){
 uint8_t fb[1];app_ctx_t ctx={0};
 const unsigned ids[3]={UI_KEY_2,UI_KEY_1,UI_KEY_3};const uint16_t physical_x[3]={100,200,300};
 const char *const expected_names[3]={"圆圈键","左键","右键"};
 const char *const expected_titles[3]={"圆圈键功能","左键功能","右键功能"};
 draw_key_settings(fb);assert(rings==2&&bars==2);
 assert(!strcmp(hold_label,"圆圈键长按"));
 assert(!strcmp(preset_labels[0],"圆圈工具栏 · 左上一页 · 右下一页"));
 assert(!strcmp(preset_labels[1],"圆圈全屏 · 左回首页 · 右工具栏"));
 for(unsigned col=0;col<3;++col){
  assert(!strcmp(labels[col],expected_names[col]));
  assert(!strcmp(actions[col],reader_key_label(values[ids[col]])));
  assert(ui_key_hit_test(physical_x[col],1600)==(int)ids[col]);
  EpdRect card=reader_key_card(ids[col]);assert(card.x==36+(int)col*208);
  s_reader_panel=READER_PANEL_KEY_SETTINGS;
  for(int x=card.x;x<card.x+card.width;++x)for(int y=card.y;y<card.y+card.height;++y){
   s_reader_panel=READER_PANEL_KEY_SETTINGS;
   assert(key_action(&ctx,x,y)==APP_REDRAW_PAGE&&s_reader_key_pick==ids[col]&&s_reader_panel==READER_PANEL_KEY_PICKER);
  }
  draw_key_picker(fb);assert(!strcmp(title,expected_titles[col]));
  for(int action=0;action<APP_READER_KEY_COUNT;++action){
   s_reader_panel=READER_PANEL_KEY_SETTINGS;key_action(&ctx,card.x+20,card.y+20);
   assert(key_action(&ctx,100,390+action*94+39)==APP_REDRAW_PAGE);
   assert(saved_key==(int)ids[col]&&saved_value==action&&values[ids[col]]==(app_reader_key_action_t)action);
  }
 }
 // 间隙不误选；保存失败不改配置或退出，长按仍写原槽。
 // Gaps do not select; failed saves retain both bindings and picker, and holds use their existing slot.
 for(int x=0;x<684;++x){bool hit=false;for(unsigned i=0;i<3;++i)hit|=ui_rect_hit(reader_key_card(i),x,550);if(!hit){s_reader_panel=READER_PANEL_KEY_SETTINGS;assert(key_action(&ctx,x,550)==APP_REDRAW_NONE&&s_reader_panel==READER_PANEL_KEY_SETTINGS);}}
 s_reader_panel=READER_PANEL_KEY_SETTINGS;key_action(&ctx,100,550);save_fail=true;int old=values[1];
 assert(key_action(&ctx,100,429)==APP_REDRAW_PAGE&&s_reader_panel==READER_PANEL_KEY_PICKER&&values[1]==old&&s_reader_key_error[0]);save_fail=false;
 s_reader_panel=READER_PANEL_KEY_SETTINGS;key_action(&ctx,100,690);assert(s_reader_key_pick==3);
 draw_key_picker(fb);assert(!strcmp(title,"圆圈键长按功能"));
 key_action(&ctx,100,390+APP_READER_KEY_REFRESH*94+39);assert(hold_saved==APP_READER_KEY_REFRESH);
 assert(!invalidations);
 puts("PASS: actual S31 circle/left/right diagram+labels+action display, all67032 card pixels, all21 action saves, physical touch IDs, gaps, long hold and save failure; persisted numbering unchanged");
}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp)/'test.c';p.write_text(u);exe=p.with_suffix('')
 subprocess.run(['cc','-std=gnu11','-Wall','-Wextra','-Werror','-Wno-sign-compare','-fsanitize=address,undefined',str(p),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
