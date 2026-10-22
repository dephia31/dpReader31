#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
"""Exercise production reset confirmation and deferred job without resetting a real device."""
from pathlib import Path
import subprocess,tempfile
from home_cover_cache_host_test import function
ROOT=Path(__file__).resolve().parents[1]
unit=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
typedef int app_ctx_t;typedef int esp_err_t;typedef int app_redraw_t;
typedef struct {int type,x0,y0;} ui_gesture_event_t;
typedef struct {int x,y,width,height;} EpdRect;
typedef struct {int state;} read_pico_transfer_status_t;
enum {APP_REDRAW_NONE,APP_REDRAW_PAGE,UI_GESTURE_TAP,SETTINGS_MAIN,SETTINGS_FACTORY_RESET,READ_PICO_TRANSFER_UPLOADING};
#define ESP_OK 0
#define ESP_ERR_INVALID_STATE -1
static bool s_reset_confirm,s_reset_pending,ota,covers=true,avatar=true;
static char s_reset_notice[96];static int s_page=SETTINGS_FACTORY_RESET,nav=-1,requested=-1,ble_error,reset_error,resets,restarts,stops,upload;
static bool ui_rect_hit(EpdRect r,int x,int y){return x>=r.x&&y>=r.y&&x<r.x+r.width&&y<r.y+r.height;}
static int settings_nav_hit(int x,int y){(void)x;(void)y;return nav;}
static void ui_nav_request(app_ctx_t *ctx,int tab){(void)ctx;requested=tab;}
static void read_pico_transfer_get_status(read_pico_transfer_status_t *s){s->state=upload;}
static bool pico_online_busy(void){return ota;}
static bool book_cover_jobs_quiesce(unsigned ms){assert(ms==2000);return covers;}
static bool ui_avatar_quiesce(unsigned ms){assert(ms==2000);return avatar;}
static void ble_pt_cancel_recent(void){}
static int ble_pt_stop(unsigned ms){assert(ms==2000);return ble_error;}
static void read_pico_transfer_stop(void){++stops;}
static int app_factory_reset(void){++resets;return reset_error;}
static void esp_restart(void){++restarts;}
static const char *esp_err_to_name(int error){(void)error;return "failure";}
'''
for name in ('reset_gesture','reset_tick'):
    unit+=function(name,ROOT/'main/apps/app_device_settings.c')+'\n'
unit+=r'''
int main(void){
 app_ctx_t ctx=0;ui_gesture_event_t tap={UI_GESTURE_TAP,340,890};
 assert(reset_gesture(&ctx,&tap)==APP_REDRAW_PAGE&&s_reset_confirm&&!s_reset_pending&&!resets);
 ui_gesture_event_t cancel={UI_GESTURE_TAP,340,980};assert(reset_gesture(&ctx,&cancel)==APP_REDRAW_PAGE&&!s_reset_confirm&&s_page==SETTINGS_MAIN);
 s_page=SETTINGS_FACTORY_RESET;tap.type=99;assert(reset_gesture(&ctx,&tap)==APP_REDRAW_NONE&&!s_reset_confirm);tap.type=UI_GESTURE_TAP;
 nav=2;assert(reset_gesture(&ctx,&tap)==APP_REDRAW_NONE&&requested==2&&!s_reset_pending);nav=-1;
 assert(reset_gesture(&ctx,&tap)==APP_REDRAW_PAGE&&s_reset_confirm&&!s_reset_pending);
 assert(reset_gesture(&ctx,&tap)==APP_REDRAW_PAGE&&s_reset_pending&&!resets);
 assert(reset_gesture(&ctx,&cancel)==APP_REDRAW_NONE&&s_reset_pending);
 for(int fault=0;fault<6;++fault){
  ota=fault==0;upload=fault==1?READ_PICO_TRANSFER_UPLOADING:0;covers=fault!=2;avatar=fault!=3;ble_error=fault==4?-2:0;reset_error=fault==5?-3:0;
  s_reset_pending=s_reset_confirm=true;int before=resets;
  assert(reset_tick()==APP_REDRAW_PAGE&&!s_reset_pending&&!s_reset_confirm&&!restarts&&s_reset_notice[0]);
  assert(resets==before+(fault==5));
 }
 ota=false;upload=ble_error=reset_error=0;covers=avatar=true;s_reset_pending=true;
 assert(reset_tick()==APP_REDRAW_NONE&&restarts==1&&!s_reset_pending);
 puts("PASS: two separate taps required, cancel/navigation/drag do not erase, transfer/job/BLE/error guards, failed reset retry and restart only after success");
}
'''
with tempfile.TemporaryDirectory() as folder:
    p=Path(folder);(p/'test.c').write_text(unit)
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
