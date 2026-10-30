#!/usr/bin/env python3
"""Run actual hold dispatch, checking busy guards and reader-only actions."""
from pathlib import Path
import subprocess,tempfile,re
from home_cover_cache_host_test import function
root=Path(__file__).resolve().parents[1]
header=(root/'main/settings.h').read_text()
enum=re.search(r'typedef enum \{\s*APP_READER_KEY_PREV[\s\S]*?app_reader_key_action_t;',header)[0]
unit=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#define APP_CONTROL_HOLD_DEFAULT 255
'''+enum+r'''
typedef enum{APP_REDRAW_NONE,APP_REDRAW_PAGE,APP_REDRAW_FULL}app_redraw_t;
typedef struct{bool request_app,request_menu,request_return;}app_ctx_t;
typedef struct{int id;bool holds_pmu;bool (*navigation_enabled)(app_ctx_t*);}app_desc_t;
const app_desc_t app_book={.id=1};
static const app_desc_t other={.id=2};
static uint8_t action;
static bool usb,busy,body,download,upload;
typedef struct{int state;}read_pico_transfer_status_t;
#define READ_PICO_TRANSFER_UPLOADING 1
static bool app_download_active(void){return download;}
static void read_pico_transfer_get_status(read_pico_transfer_status_t*s){s->state=upload;}
static int homes,lights,reader_calls;
static uint8_t app_settings_control_hold(unsigned key){assert(key<7);return action;}
static bool usb_storage_active(void){return usb;}
static bool pico_online_busy(void){return busy;}
static void ui_nav_request(app_ctx_t*ctx,int tab){(void)ctx;assert(tab==0);++homes;}
static void metalio_frontlight_toggle(void){++lights;}
static bool app_book_reader_body_visible(void){return body;}
static app_redraw_t app_book_handle_key_action(app_ctx_t*ctx,int a){(void)ctx;assert(a==action);++reader_calls;return APP_REDRAW_PAGE;}
static bool navigation_denied(app_ctx_t*c){(void)c;return false;}
'''+function('control_hold_action',root/'main/app/app_loop.c')+r'''
int main(void){app_ctx_t ctx={0};
 for(unsigned key=0;key<7;++key){
  for(action=0;action<APP_READER_KEY_COUNT;++action){
   if(action==APP_READER_KEY_FULLSCREEN)continue;
   for(int block=0;block<7;++block){
    usb=block==0;busy=block==1;download=block==5;upload=block==6;ctx.request_app=block==2;ctx.request_menu=block==3;ctx.request_return=block==4;
    assert(control_hold_action(&ctx,&app_book,key)==APP_REDRAW_NONE);
   }
   usb=busy=download=upload=false;ctx=(app_ctx_t){0};body=false;
   int before=reader_calls;control_hold_action(&ctx,&other,key);assert(reader_calls==before);
   body=true;app_redraw_t r=control_hold_action(&ctx,&app_book,key);
   if(action==APP_READER_KEY_REFRESH)assert(r==APP_REDRAW_FULL);
   else if(action==APP_READER_KEY_HOME||action==APP_READER_KEY_LIGHT||action==APP_READER_KEY_NONE)assert(r==APP_REDRAW_NONE);
   else assert(r==APP_REDRAW_PAGE&&reader_calls==before+1);
  }
  action=APP_CONTROL_HOLD_DEFAULT;assert(control_hold_action(&ctx,&app_book,key)==APP_REDRAW_NONE);
 }
 action=APP_READER_KEY_REFRESH;app_desc_t blocked={.holds_pmu=true};
 assert(control_hold_action(&ctx,&blocked,0)==APP_REDRAW_NONE);
 blocked.holds_pmu=false;blocked.navigation_enabled=navigation_denied;
 assert(control_hold_action(&ctx,&blocked,0)==APP_REDRAW_NONE);
 assert(homes&&lights&&reader_calls);puts("PASS actual hold dispatch:7 keys,USB/OTA/navigation guards,global Home/light/refresh,reader-only page/chapter/tools/sync");}
'''
with tempfile.TemporaryDirectory() as directory:
 p=Path(directory);(p/'test.c').write_text(unit)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
