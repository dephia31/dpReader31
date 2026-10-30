#!/usr/bin/env python3
"""Execute additive hold bindings with NVS write/commit fault injection."""
from pathlib import Path
import subprocess,tempfile,re
from home_cover_cache_host_test import function
root=Path(__file__).resolve().parents[1];source=root/'main/settings.c'
header=(root/'main/settings.h').read_text()
enum=re.search(r'typedef enum \{\s*APP_READER_KEY_PREV[\s\S]*?app_reader_key_action_t;',header)[0]
unit=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#define KIIKO_BOARD_METALIO47 1
#define APP_CONTROL_HOLD_DEFAULT 255
#define NVS_NS "metalio_s31"
#define NVS_READWRITE 1
#define ESP_OK 0
typedef int nvs_handle_t;
typedef int esp_err_t;
static int failure,writes,commits;
static const char *last_key;
static uint8_t middle=4;
static int nvs_open(const char *ns,int mode,int *h){assert(!strcmp(ns,NVS_NS)&&mode==1);*h=1;return failure==1?-1:0;}
static int nvs_set_u8(int h,const char *key,uint8_t value){(void)h;(void)value;++writes;last_key=key;return failure==2?-1:0;}
static int nvs_commit(int h){(void)h;++commits;return failure==3?-1:0;}
static void nvs_close(int h){(void)h;}
'''+enum+r'''
static uint8_t s_control_holds[7]={255,APP_READER_KEY_NONE,APP_READER_KEY_NONE,255,255,APP_READER_KEY_NONE,APP_READER_KEY_NONE};
static const char *const s_control_hold_names[]={"hold_ai","hold_vup","hold_vdown","hold_power","hold_mid","hold_left","hold_right"};
static app_reader_key_action_t app_settings_reader_hold_action(void){return middle;}
static bool app_settings_set_reader_hold_action(app_reader_key_action_t a){if(failure)return false;middle=a;return true;}
'''
for name in ('app_settings_reader_key_action_allowed','app_settings_control_hold','app_settings_set_control_hold'):
 unit+=function(name,source)+'\n'
unit+=r'''
int main(void){
 for(unsigned k=0;k<7;++k){
  uint8_t old=app_settings_control_hold(k);
  assert(!app_settings_set_control_hold(k,APP_READER_KEY_FULLSCREEN));
  for(failure=1;failure<=3;++failure){assert(!app_settings_set_control_hold(k,APP_READER_KEY_NEXT_CHAPTER));assert(app_settings_control_hold(k)==old);}
  failure=0;assert(app_settings_set_control_hold(k,APP_READER_KEY_NEXT_CHAPTER));assert(app_settings_control_hold(k)==APP_READER_KEY_NEXT_CHAPTER);
  if(k!=4)assert(!strcmp(last_key,s_control_hold_names[k]));
  assert(app_settings_set_control_hold(k,APP_CONTROL_HOLD_DEFAULT));
  assert(app_settings_control_hold(k)==(k==4?APP_READER_KEY_HOME:APP_CONTROL_HOLD_DEFAULT));
 }
 assert(!app_settings_set_control_hold(7,0));assert(!app_settings_set_control_hold(0,200));
 assert(writes>commits);puts("PASS seven hold bindings: independent NVS keys,middle legacy API,invalid input,open/write/commit rollback/defaults");
}
'''
with tempfile.TemporaryDirectory() as directory:
 p=Path(directory);(p/'test.c').write_text(unit)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
