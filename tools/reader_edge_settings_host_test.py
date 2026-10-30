#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Actual independent gesture preferences: persistent writes and fault rollback."""
from pathlib import Path
import subprocess,tempfile
from home_cover_cache_host_test import function
R=Path(__file__).resolve().parents[1];src=R/'main/settings.c'
u=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef int nvs_handle_t;typedef int esp_err_t;
#define ESP_OK 0
#define NVS_NS "metalio_s31"
#define NVS_READWRITE 1
static bool s_reader_edge_brightness=true,s_reader_edge_warmth=true;
static int failure,slot;static uint8_t stored[2]={1,1},staged;
static int nvs_open(const char*ns,int mode,int*h){assert(!strcmp(ns,NVS_NS)&&mode==1);*h=1;return failure==1?-1:0;}
static int nvs_set_u8(int h,const char*k,uint8_t v){assert(h==1&&v<=1);slot=!strcmp(k,"r_edge_light")?0:!strcmp(k,"r_edge_warm")?1:-1;assert(slot>=0);staged=v;return failure==2?-1:0;}
static int nvs_commit(int h){assert(h==1);if(failure==3)return -1;stored[slot]=staged;return 0;}
static void nvs_close(int h){assert(h==1);}
'''
for name in ('reader_edge_setting','app_settings_reader_edge_brightness','app_settings_set_reader_edge_brightness','app_settings_reader_edge_warmth','app_settings_set_reader_edge_warmth'):u+=function(name,src)+'\n'
u+=r'''
int main(void){assert(app_settings_reader_edge_brightness()&&app_settings_reader_edge_warmth());
 assert(app_settings_set_reader_edge_brightness(false));assert(!s_reader_edge_brightness&&s_reader_edge_warmth&&!stored[0]&&stored[1]);
 for(failure=1;failure<=3;++failure){assert(!app_settings_set_reader_edge_warmth(false));assert(s_reader_edge_warmth&&stored[1]);assert(!app_settings_set_reader_edge_brightness(true));assert(!s_reader_edge_brightness&&!stored[0]);}
 failure=0;assert(app_settings_set_reader_edge_warmth(false));assert(!stored[1]);assert(app_settings_set_reader_edge_brightness(true));assert(stored[0]&&!stored[1]);
 s_reader_edge_brightness=stored[0]!=0;s_reader_edge_warmth=stored[1]!=0;assert(s_reader_edge_brightness&&!s_reader_edge_warmth);
 puts("PASS actual edge settings: independent NVS keys, defaults, persisted selection, open/write/commit failure rollback");}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(u);subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(p/'test.c'),'-o',str(p/'test')],check=True);subprocess.run([str(p/'test')],check=True)
