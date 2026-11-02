#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Exercise the actual Backup/Restore render and tap branches without a device."""
from pathlib import Path
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[1]
source=(ROOT/'main/apps/app_device_settings.c').read_text()
needle='    if (s_page == SETTINGS_CONFIG) {'
def block(start):
    opening=source.index('{',start);depth=1;end=opening+1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    return source[start:end]
first=source.index(needle);second=source.index(needle,first+1)
unit=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define CONFIG_DPREADER_AUTO_BACKUP 1
#define SETTINGS_CONFIG 1
#define APP_REDRAW_PAGE 1
#define APP_REDRAW_NONE 0
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_STATE 2
#define ESP_ERR_NOT_FOUND 3
#define ESP_ERR_INVALID_RESPONSE 4
#define EPD_DRAW_ALIGN_LEFT 0
#define UI_GRAY_WHITE 255
typedef int esp_err_t;
typedef struct {int x,y,width,height;} EpdRect;
typedef struct {int x0,y0;} Event;
static int s_page=SETTINGS_CONFIG,sets,saves,restores,toggle_y;
static bool enabled=true,fail,blocked,toggle_on,s_config_confirm;
static char s_notice[96],toggle_detail[256];
static bool ui_rect_hit(EpdRect r,int x,int y){return x>=r.x&&x<r.x+r.width&&y>=r.y&&y<r.y+r.height;}
static bool app_settings_autobackup_enabled(void){return enabled;}
static bool app_settings_autobackup_blocked(void){return blocked;}
static esp_err_t app_settings_set_autobackup_enabled(bool value){if(fail)return ESP_FAIL;enabled=value;++sets;return ESP_OK;}
static esp_err_t app_settings_backup_save(void){++saves;return ESP_OK;}
static esp_err_t app_settings_backup_restore(void){++restores;return ESP_OK;}
static void setting_toggle(uint8_t*fb,int y,const char*title,const char*detail,bool on){
 (void)fb;assert(!strcmp(title,"Tự động sao lưu"));toggle_y=y;toggle_on=on;
 snprintf(toggle_detail,sizeof(toggle_detail),"%s",detail);
}
#define back_header(...) ((void)0)
#define section(...) ((void)0)
#define settings_card(...) ((void)0)
#define settings_line(...) ((void)0)
#define ui_draw_button(...) ((void)0)
#define ui_text_caption(...) ((void)0)
#define settings_navigation(...) ((void)0)
#define ttf_font_scan() ((void)0)
#define app_font_activate_system() ((void)0)
#define ui_text_set_system_scale(...) ((void)0)
#define book_store_notify_changed() ((void)0)
#define read_pico_transfer_get_saved_wifi(ssid,saved) ((void)(ssid),*(saved)=false,ESP_OK)
'''
unit+='static void render(uint8_t*fb){'+block(first)+'}\n'
unit+='static int tap(const Event*ev){int y=ev->y0;'+block(second)+'return APP_REDRAW_NONE;}\n'
unit+=r'''
int main(void){
 render(NULL);assert(toggle_y==299&&toggle_on);
 Event ev={20,350};tap(&ev);assert(!sets&&enabled);
 ev=(Event){100,350};assert(tap(&ev)==APP_REDRAW_PAGE&&!enabled&&sets==1&&!saves);
 render(NULL);assert(!toggle_on);
 fail=true;assert(tap(&ev)==APP_REDRAW_PAGE&&!enabled&&strstr(s_notice,"Không lưu"));fail=false;
 s_config_confirm=true;tap(&ev);assert(enabled&&!s_config_confirm);
 blocked=true;render(NULL);assert(strstr(toggle_detail,"Tạm dừng"));
 ev=(Event){100,580};tap(&ev);assert(saves==1);
 ev=(Event){100,700};tap(&ev);assert(s_config_confirm&&!restores);
 tap(&ev);assert(!s_config_confirm&&restores==1);
 puts("PASS backup switch UI: on/off display, full-row hitbox, failed save, confirmation cancellation, quarantine detail and manual actions");
}
'''
with tempfile.TemporaryDirectory() as folder:
    work=Path(folder);(work/'test.c').write_text(unit)
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(work/'test.c'),'-o',str(work/'test')],check=True)
    subprocess.run([str(work/'test')],check=True)
