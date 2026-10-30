#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Exercise actual boot VCOM resolution with saved values and reversible trial."""
from pathlib import Path
import subprocess,tempfile,re
from home_cover_cache_host_test import function
R=Path(__file__).resolve().parents[1]
config=(R/'boards/metalio47/kiiko_board_config.h').read_text()
trial=int(re.search(r'#define KIIKO_BOARD_VCOM_TRIAL_MV (\d+)',config)[1]);assert 900<=trial<=2200 and trial%10==0
initial=(R/'boards/metalio47/src/components/read_pico/read_pico_board.c').read_text()
assert int(re.search(r'static int vcom=(\d+);',initial)[1])==trial
unit='''#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#define TAG "test"
#define KIIKO_BOARD_VCOM_DEFAULT_MV 1200
#define ESP_LOGI(tag,fmt,...) ((void)printf(fmt,__VA_ARGS__))
static int stored,applied,reads;
static int vcom_setup_load_mv(void){++reads;return stored;}
static void epd_set_vcom(uint16_t mv){applied=mv;}
'''+function('resolve_vcom_at_boot',R/'boards/metalio47/src/main/app_main.c')+'''
int main(void){int values[]={900,1200,1700,2000,2200};for(unsigned i=0;i<5;++i){stored=values[i];resolve_vcom_at_boot();assert(stored==values[i]);
#ifdef KIIKO_BOARD_VCOM_TRIAL_MV
 assert(applied==KIIKO_BOARD_VCOM_TRIAL_MV);
#else
 assert(applied==stored);
#endif
 }assert(reads==5);puts("PASS actual VCOM boot resolution: configured trial, saved calibration unchanged, normal resolution preserved without trial");}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(unit)
 for enabled in (False,True):
  subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined',*([f'-DKIIKO_BOARD_VCOM_TRIAL_MV={trial}'] if enabled else []),str(p/'test.c'),'-o',str(p/'test')],check=True)
  subprocess.run([str(p/'test')],check=True)
