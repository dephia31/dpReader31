#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
"""中文：真实唤醒选项、设置命中与深睡开机门。/ English: Actual wake preferences, setting hits and the deep-boot gate."""
from pathlib import Path
import subprocess,tempfile
from home_cover_cache_host_test import function
ROOT=Path(__file__).resolve().parents[1]
settings=ROOT/'main/settings.c'
ui=(ROOT/'main/apps/app_device_settings.c').read_text()
boot=(ROOT/'main/app_main.c').read_text()
unit=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#define NVS_KEY_WAKE_PRESSES "wake_press"
#define ESP_OK 0
#define ESP_LOGI(...) ((void)0)
static uint8_t s_wake_presses=2,stored;static unsigned writes,gate,ready,held;
static bool staged,pin,boot_press;
static void nvs_put_u8(const char *key,uint8_t value){assert(!strcmp(key,NVS_KEY_WAKE_PRESSES));stored=value;++writes;}
'''
for name in ('app_settings_wake_presses','app_settings_set_wake_presses'):
    unit+=function(name,settings)+'\n'
unit+=r'''
enum {SETTINGS_POWER_SLEEP,APP_REDRAW_PAGE,APP_REDRAW_NONE};
static int s_page=SETTINGS_POWER_SLEEP;
static void app_settings_set_staged_shutdown(bool value){staged=value;}
static int setting_tap(int y){
'''
start=ui.index('    if (s_page == SETTINGS_POWER_SLEEP) {',ui.index('static app_redraw_t on_gesture'))
end=ui.index('    if (s_page == SETTINGS_CONFIG)',start)
unit+=ui[start:end]+'return -1;}\n'
unit+=r'''
static bool read_pico_pmu_take_boot_short_press(void){bool value=boot_press;boot_press=false;return value;}
static bool lock_pin_enabled(void){return pin;}
static void pico_boot_ready(void){++ready;}
static void pico_boot_hold_resume(void){++held;}
static void app_lock_boot_gate(void *a,int b,int c){(void)a;(void)b;(void)c;++gate;}
static void boot_gate(bool resuming){int hl=0;struct {int touch,sensor;}hw={0};
'''
start=boot.index('    const bool boot_press = read_pico_pmu_take_boot_short_press();')
end=boot.index('    rp_boot_mark("lock");',start)
unit+=boot[start:end]+'}\n'
unit+=r'''
int main(void){
 assert(app_settings_wake_presses()==2);
 app_settings_set_wake_presses(1);assert(stored==1&&writes==1&&app_settings_wake_presses()==1);
 app_settings_set_wake_presses(1);assert(writes==1);
 app_settings_set_wake_presses(0);assert(stored==2&&writes==2&&app_settings_wake_presses()==2);
 app_settings_set_wake_presses(255);assert(writes==2);
 assert(setting_tap(692)==APP_REDRAW_PAGE&&s_wake_presses==1);
 assert(setting_tap(803)==APP_REDRAW_PAGE&&s_wake_presses==1);
 assert(setting_tap(804)==APP_REDRAW_NONE&&s_wake_presses==1);
 assert(setting_tap(815)==APP_REDRAW_NONE&&s_wake_presses==1);
 assert(setting_tap(816)==APP_REDRAW_PAGE&&s_wake_presses==2);
 assert(setting_tap(927)==APP_REDRAW_PAGE&&s_wake_presses==2);
 assert(setting_tap(928)==APP_REDRAW_NONE&&s_wake_presses==2);
 assert(setting_tap(278)==APP_REDRAW_PAGE&&!staged);
 assert(setting_tap(414)==APP_REDRAW_PAGE&&staged);
 for(unsigned resume=0;resume<2;++resume)for(unsigned password=0;password<2;++password)
 for(unsigned key=0;key<2;++key)for(unsigned count=1;count<=2;++count){
  s_wake_presses=count;pin=password;boot_press=key;gate=ready=held=0;
  boot_gate(resume);bool direct=resume&&!password&&key&&count==1;
  assert(gate==!direct&&ready==direct&&held==direct&&!boot_press);
 }
 puts("PASS: wake default/invalid values/idempotent writes, exact setting hit gaps, power evidence consumed once, charger/timer/PIN cannot bypass boot authentication");
}
'''
with tempfile.TemporaryDirectory() as folder:
    p=Path(folder);(p/'test.c').write_text(unit)
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
