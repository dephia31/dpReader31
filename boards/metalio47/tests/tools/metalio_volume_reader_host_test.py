#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
"""中文：真实S31音量派发独立于底部自定义键，并保留模态/切页保护。
English: Exercise actual S31 volume routing independently of bottom bindings and with modal/navigation guards.
"""
from pathlib import Path
import re, subprocess, tempfile
from home_cover_cache_host_test import function
root = Path(__file__).resolve().parents[1]
book = root / 'main/apps/app_book.c'
settings = (root / 'main/settings.h').read_text()
a = settings.index('typedef enum {\n    APP_READER_KEY_PREV')
b = settings.index('} app_reader_key_action_t;', a) + len('} app_reader_key_action_t;')
unit = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
typedef enum {APP_REDRAW_NONE,APP_REDRAW_AREA,APP_REDRAW_PAGE,APP_REDRAW_FULL}app_redraw_t;
struct app_desc;
typedef struct {int64_t now_ms;const struct app_desc *request_app;bool request_menu,request_return;}app_ctx_t;
typedef struct app_desc {bool owns_keys;app_redraw_t(*on_key)(app_ctx_t*,int);}app_desc_t;
enum {SHELF,READING,TOC,READER_PANEL_NONE,READER_PANEL_TOOLS,READER_PANEL_FONT_SETTINGS,READER_PANEL_FONT_RECOVERY,READER_PANEL_ONLINE_FONTS};
#define UI_KEY_1 0
#define UI_KEY_3 2
static int s_view=READING,s_reader_panel=READER_PANEL_NONE,s_reader_slider,s_pressed_control;
static bool s_scan_pending,s_clear_confirm,root_visible,usb_busy,online_busy;
static const char *s_text="text";
static struct {bool pending;}s_reader_tap;
static int64_t s_stats_activity_ms;
static int turned,turns,home,fullscreen,refresh,bindings_read,held_cancel,prep,root_key,child_key=-1;
static void reader_indent_hold_cancel(void){++held_cancel;}
static void pause_reader_prep(void){++prep;}
static app_redraw_t turn_page(app_ctx_t *ctx,int direction){(void)ctx;turned=direction;++turns;s_reader_panel=READER_PANEL_NONE;return APP_REDRAW_AREA;}
static app_redraw_t reader_return(app_ctx_t *ctx){(void)ctx;++home;return APP_REDRAW_NONE;}
static app_redraw_t toggle_reader_fullscreen(app_ctx_t *ctx){(void)ctx;++fullscreen;return APP_REDRAW_PAGE;}
static app_redraw_t reader_manual_refresh(app_ctx_t *ctx){(void)ctx;++refresh;return APP_REDRAW_FULL;}
static bool main_page_ready(app_ctx_t *ctx,const app_desc_t *app){(void)ctx;(void)app;return root_visible;}
static bool usb_storage_active(void){return usb_busy;}
static bool pico_online_busy(void){return online_busy;}
static app_redraw_t main_key_action(app_ctx_t *ctx,const app_desc_t *app,int key,bool held){(void)ctx;(void)app;assert(!held);root_key=key;return APP_REDRAW_PAGE;}
'''
unit += settings[a:b] + '\n'
unit += 'static app_reader_key_action_t bindings[3];\n'
unit += function('reader_key_action',book)+'\n'
unit += function('app_book_volume_key',book)+'\n'
unit += r'''
static app_redraw_t bottom_key(app_ctx_t *ctx,int key){assert(key==0||key==2);++bindings_read;return reader_key_action(ctx,bindings[key]);}
static app_redraw_t child_on_key(app_ctx_t *ctx,int key){(void)ctx;child_key=key;return APP_REDRAW_PAGE;}
static const app_desc_t app_book={true,bottom_key},child={true,child_on_key},no_keys={false,NULL};
'''
unit += function('volume_key_action',root/'main/app/app_loop.c')+'\n'
unit += r'''
static void reset(void){
 s_view=READING;s_reader_panel=READER_PANEL_NONE;s_reader_slider=2;s_pressed_control=9;
 s_text="text";s_reader_tap.pending=true;s_scan_pending=s_clear_confirm=root_visible=usb_busy=online_busy=false;
 turns=turned=home=fullscreen=refresh=bindings_read=held_cancel=prep=0;root_key=child_key=-1;
}
int main(void){
 app_ctx_t ctx={.now_ms=1234};
 for(int left=APP_READER_KEY_PREV;left<=APP_READER_KEY_NONE;++left)
 for(int right=APP_READER_KEY_PREV;right<=APP_READER_KEY_NONE;++right)
 for(int dir=-1;dir<=1;dir+=2){
  reset();bindings[0]=left;bindings[2]=right;
  assert(volume_key_action(&ctx,&app_book,dir)==APP_REDRAW_AREA);
  assert(turns==1&&turned==dir&&!home&&!fullscreen&&!refresh&&!bindings_read);
  assert(!s_reader_tap.pending&&s_pressed_control==-1&&held_cancel==1&&s_stats_activity_ms==ctx.now_ms);
 }
 reset();bindings[0]=APP_READER_KEY_HOME;bindings[2]=APP_READER_KEY_TOOLS;
 assert(bottom_key(&ctx,0)==APP_REDRAW_NONE&&home==1&&!turns);
 assert(bottom_key(&ctx,2)==APP_REDRAW_PAGE&&s_reader_panel==READER_PANEL_TOOLS&&!turns);
 for(int dir=-1;dir<=1;dir+=2){reset();s_reader_panel=READER_PANEL_TOOLS;assert(volume_key_action(&ctx,&app_book,dir)==APP_REDRAW_AREA&&turned==dir&&s_reader_panel==READER_PANEL_NONE);}
 for(int panel=READER_PANEL_FONT_SETTINGS;panel<=READER_PANEL_ONLINE_FONTS;++panel){
  reset();s_reader_panel=panel;assert(volume_key_action(&ctx,&app_book,1)==APP_REDRAW_NONE);
  assert(!turns&&!bindings_read&&s_reader_panel==panel&&s_pressed_control==9);
 }
 for(int guard=0;guard<10;++guard){
  reset();ctx.request_app=NULL;ctx.request_menu=ctx.request_return=false;
  if(guard==0)s_text=NULL;if(guard==1)s_scan_pending=true;if(guard==2)s_clear_confirm=true;
  if(guard==3)usb_busy=true;if(guard==4)online_busy=true;if(guard==5)ctx.request_app=&child;
  if(guard==6)ctx.request_menu=true;if(guard==7)ctx.request_return=true;
  int direction=guard==8?0:guard==9?2:1;
  app_redraw_t r=direction==2?APP_REDRAW_FULL:volume_key_action(&ctx,&app_book,direction);
  if(direction==2)assert(app_book_volume_key(&ctx,direction,&r));
  assert(r==APP_REDRAW_NONE&&!turns&&!bindings_read);
 }
 ctx=(app_ctx_t){.now_ms=2345};
 for(int dir=-1;dir<=1;dir+=2){
  reset();root_visible=true;assert(volume_key_action(&ctx,&app_book,dir)==APP_REDRAW_PAGE&&root_key==(dir<0?0:2)&&!turns&&!bindings_read);
  reset();s_view=TOC;assert(volume_key_action(&ctx,&child,dir)==APP_REDRAW_PAGE&&child_key==(dir<0?0:2)&&!turns);
  reset();assert(volume_key_action(&ctx,&no_keys,dir)==APP_REDRAW_NONE);
 }
 puts("PASS: actual S31 volume routing and reader actions: all49 bottom binding pairs, fixed previous/next, unchanged custom bottom actions, tools closing, modal/missing-book/scan/navigation/USB/OTA guards and root/child delegation");
}
'''
with tempfile.TemporaryDirectory() as tmp:
    path=Path(tmp)/'test.c';path.write_text(unit)
    exe=path.with_suffix('')
    subprocess.run(['cc','-std=gnu11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(path),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
