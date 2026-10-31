#!/usr/bin/env python3
"""Exercise real quick-radio feedback, errors, no-op polls and bounded drawing.
SPDX-License-Identifier: Apache-2.0
"""
from pathlib import Path
import subprocess,tempfile
from home_cover_cache_host_test import function
ROOT=Path(__file__).resolve().parents[1]
unit=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef struct{int x,y,width,height;}EpdRect;
typedef int esp_err_t;
#define ESP_OK 0
#define KIIKO_BOARD_METALIO47 1
#define READ_PICO_TRANSFER_MODE_STA 1
#define ESP_ERR_NOT_FOUND 1
#define ESP_ERR_INVALID_STATE 2
#define UI_GRAY_WHITE 255
#define UI_GRAY_BLACK 0
#define EPD_DRAW_ALIGN_CENTER 0
enum{READ_PICO_TRANSFER_STOPPED,READ_PICO_TRANSFER_STARTING,READ_PICO_TRANSFER_READY,READ_PICO_TRANSFER_ERROR};
static int s_quick_notice_kind;
static esp_err_t s_quick_notice_error;
static char s_quick_notice_text[96];
typedef struct{int state,mode;bool network_ready;}read_pico_transfer_status_t;
static read_pico_transfer_status_t network;
static bool enabled,connected,connecting;
typedef struct{int mode;bool network_only;}read_pico_transfer_cfg_t;
static bool configured,stop_allowed;
static int saved_error,start_error,starts,stops;
static int read_pico_transfer_get_saved_wifi(char *ssid,bool *out){ssid[0]=0;*out=configured;return saved_error;}
static bool read_pico_transfer_try_stop_if_idle(void){++stops;return stop_allowed;}
static int read_pico_transfer_start(const read_pico_transfer_cfg_t *cfg){assert(cfg->mode==READ_PICO_TRANSFER_MODE_STA&&cfg->network_only);++starts;return start_error;}
static bool app_settings_ble_turner(void){return enabled;}
static bool ble_pt_connected(void){return connected;}
static bool ble_pt_connecting(void){return connecting;}
static void read_pico_transfer_get_status(read_pico_transfer_status_t *s){*s=network;}
static unsigned fills,frames,texts;
static char rendered[96];
static void ui_fill_round_rect(uint8_t *fb,EpdRect r,int radius,int color){assert(fb&&r.x==24&&r.y==348&&r.width==636&&r.height==34&&radius==10&&color==255);++fills;}
static void ui_draw_control_frame(uint8_t *fb,EpdRect r,int radius,int color){assert(fb&&r.y+r.height<=384&&r.x+r.width<=684&&radius==10&&color==0);++frames;}
static void ui_text_box(uint8_t *fb,EpdRect r,int px,const char *text,int align,bool invert){assert(fb&&r.x>=24&&r.y>=348&&r.x+r.width<=660&&r.y+r.height<=382&&px==22&&align==0&&!invert);snprintf(rendered,sizeof(rendered),"%s",text);++texts;}
'''
for name,path in [('ui_quick_menu_draw_notice','main/ui/ui_quick_menu.c'),('quick_notice_begin','main/app/app_loop.c'),('quick_notice_poll','main/app/app_loop.c'),('quick_toggle_wifi','main/app/app_loop.c')]:unit+=function(name,ROOT/path)+'\n'
unit+=r'''
static uint8_t fb;
static void expect(const char *text){unsigned before=texts;EpdRect r=quick_notice_poll(&fb);assert(r.height==34&&texts==before+1&&!strcmp(rendered,text));assert(!quick_notice_poll(&fb).height&&texts==before+1);}
int main(void){
 assert(quick_toggle_wifi()==ESP_ERR_NOT_FOUND&&!starts);
 saved_error=9;assert(quick_toggle_wifi()==9&&!starts);saved_error=0;
 configured=true;assert(quick_toggle_wifi()==0&&starts==1);
 start_error=9;assert(quick_toggle_wifi()==9&&starts==2);start_error=0;
 network.state=READ_PICO_TRANSFER_READY;assert(quick_toggle_wifi()==ESP_ERR_INVALID_STATE&&stops==1&&starts==2);
 stop_allowed=true;assert(quick_toggle_wifi()==0&&stops==2);network.state=READ_PICO_TRANSFER_STOPPED;
 assert(!quick_notice_poll(&fb).height);
 quick_notice_begin(1,ESP_OK);network.state=READ_PICO_TRANSFER_STARTING;expect("Đã bật Wi-Fi · Đang kết nối…");
 network.state=READ_PICO_TRANSFER_READY;network.network_ready=true;expect("Wi-Fi đã kết nối");
 network.state=READ_PICO_TRANSFER_STOPPED;network.network_ready=false;expect("Đã tắt Wi-Fi");
 quick_notice_begin(1,ESP_ERR_NOT_FOUND);expect("Chưa lưu mạng Wi-Fi");
 quick_notice_begin(1,ESP_ERR_INVALID_STATE);expect("Wi-Fi đang bận truyền tệp");
 quick_notice_begin(1,ESP_OK);network.state=READ_PICO_TRANSFER_ERROR;expect("Kết nối Wi-Fi thất bại");
 quick_notice_begin(2,ESP_OK);enabled=true;expect("Đã bật Bluetooth · Chờ kết nối");
 connecting=true;expect("Bluetooth đang kết nối…");
 connected=true;expect("Bluetooth đã kết nối");
 enabled=false;expect("Đã tắt Bluetooth");
 quick_notice_begin(2,9);expect("Không thể bật/tắt Bluetooth");
 assert(fills==frames&&frames==texts);s_quick_notice_kind=0;assert(!quick_notice_poll(&fb).height);
 puts("PASS actual quick radio notifications: on/off/connecting/connected/errors, no redraw while unchanged, black frame within existing saved384px underlay");
}
'''
with tempfile.TemporaryDirectory() as d:
 c=Path(d)/'test.c';exe=Path(d)/'test';c.write_text(unit)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(c),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
