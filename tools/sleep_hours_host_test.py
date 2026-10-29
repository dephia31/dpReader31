#!/usr/bin/env python3
"""中文：实际小时设置的边界、NVS失败和提交语义。/ English: Actual sleep-hour bounds, NVS failures and commit semantics."""
from pathlib import Path
import re,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'main/settings.c').read_text()
a=s.index('uint8_t app_settings_sleep_off_hours(');b=s.index('bool app_settings_staged_shutdown(',a)
init=s[s.index('    raw=APP_SLEEP_OFF_HOURS_DEFAULT;'):s.index('    raw = 1;\n    if (nvs_get_u8(h, NVS_KEY_WAKE_PRESSES') ]
unit=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#define APP_SLEEP_OFF_HOURS_MIN 1
#define APP_SLEEP_OFF_HOURS_MAX 24
#define APP_SLEEP_OFF_HOURS_DEFAULT 1
#define NVS_NS "settings"
#define NVS_KEY_SLEEP_OFF_HOURS "slp_off_h"
#define NVS_READWRITE 1
#define ESP_OK 0
#define ESP_ERR_INVALID_ARG 1
typedef int esp_err_t;
typedef int nvs_handle_t;
static uint8_t s_sleep_off_hours=APP_SLEEP_OFF_HOURS_DEFAULT;
static int opens,writes,commits,closes,open_error,write_error,commit_error,saved;
static uint8_t stored;static int read_error;
static int nvs_get_u8(int h,const char*k,uint8_t *v){assert(h==7&&!strcmp(k,"slp_off_h"));*v=stored;return read_error;}
static int nvs_open(const char*n,int mode,int *h){(void)n;assert(mode==1);*h=7;++opens;return open_error;}
static int nvs_set_u8(int h,const char*k,uint8_t v){assert(h==7&&!strcmp(k,"slp_off_h"));++writes;saved=v;return write_error;}
static int nvs_commit(int h){assert(h==7);++commits;return commit_error;}
static void nvs_close(int h){assert(h==7);++closes;}
'''+s[a:b]+'static void load_hours(void){uint8_t raw;nvs_handle_t h=7;'+init+'}\n'+r'''
int main(void){
 assert(app_settings_sleep_off_hours()==1);
 for(unsigned h=0;h<256;++h){
  int old=app_settings_sleep_off_hours(),before=opens;
  int e=app_settings_set_sleep_off_hours(h);
  if(h<1||h>24){assert(e==ESP_ERR_INVALID_ARG&&opens==before&&app_settings_sleep_off_hours()==old);}
  else assert(e==ESP_OK&&app_settings_sleep_off_hours()==h);
 }
 int old=app_settings_sleep_off_hours();open_error=2;assert(app_settings_set_sleep_off_hours(1)==2&&app_settings_sleep_off_hours()==old);open_error=0;
 int before=commits;write_error=3;assert(app_settings_set_sleep_off_hours(1)==3&&commits==before&&app_settings_sleep_off_hours()==old);write_error=0;
 commit_error=4;assert(app_settings_set_sleep_off_hours(1)==4&&app_settings_sleep_off_hours()==old);commit_error=0;
 assert(app_settings_set_sleep_off_hours(1)==ESP_OK&&saved==1&&app_settings_sleep_off_hours()==1);
 assert(closes==opens-1&&writes>0);
 for(unsigned h=0;h<256;++h){s_sleep_off_hours=APP_SLEEP_OFF_HOURS_DEFAULT;stored=h;load_hours();assert(app_settings_sleep_off_hours()==(h>=1&&h<=24?h:1));}
 s_sleep_off_hours=APP_SLEEP_OFF_HOURS_DEFAULT;read_error=2;stored=24;load_hours();assert(app_settings_sleep_off_hours()==1);
 puts("PASS: actual sleep-hour setting accepts1..24, defaults1, commits before RAM update and preserves previous value on NVS failures");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(unit);subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror',str(p/'test.c'),'-o',str(p/'test')],check=True);subprocess.run([str(p/'test')],check=True)
