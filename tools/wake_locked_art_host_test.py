#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
# 中文：执行真实开机画面选择，深睡缓存失败不得把等待解锁伪装成加载。
# English: Execute actual startup frame selection; failed resume art must not disguise locked input as loading.
from pathlib import Path
import subprocess,tempfile
from home_cover_cache_host_test import function

root=Path(__file__).resolve().parents[1]
source=root/'boards/pico/src/main/app_main.c'
unit=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define ESP_OK 0
#define ESP_ERR_NOT_FINISHED 1
#define pdMS_TO_TICKS(x) (x)
#define BYTES (684u*1216u/2u)
typedef struct{uint8_t *back_fb;}EpdiyHighlevelState;
typedef struct{bool mounted;}read_pico_sd_info_t;
static const uint8_t loading_4bpp_bin_start[1]={0x56};
static bool pin,cached,valid,mounted;static int pending,probes,waits,reads,power,clears,censuses,presents;
static bool lock_pin_enabled(void){return pin;}
static bool pico_boot_resume_frame_cached(void){return cached;}
static int epd_width(void){return 684;}
static int epd_height(void){return 1216;}
static int read_pico_sd_get_info(read_pico_sd_info_t *info){++probes;info->mounted=mounted&&pending<=0;return pending-->0?ESP_ERR_NOT_FINISHED:ESP_OK;}
static void vTaskDelay(int ms){assert(ms==20);++waits;}
static bool lock_screen_restore(uint8_t *fb){assert(mounted);++reads;memset(fb,0x9a,BYTES);return valid;}
static void epd_poweron(void){++power;}
static void read_pico_i2c_census_take(void){++censuses;}
static void epd_clear(void){++clears;}
static void epd_hl_set_all_white(EpdiyHighlevelState *hl){memset(hl->back_fb,255,BYTES);}
static void ui_draw_full_image(uint8_t *fb,const uint8_t *image){memset(fb,image[0],BYTES);}
static void ui_lock_fallback_draw(uint8_t *fb){memset(fb,255,BYTES);fb[BYTES/2]=0;}
static void update_display_from_white(EpdiyHighlevelState *hl){assert(power==1&&clears==1);for(unsigned i=0;i<BYTES;++i)assert(hl->back_fb[i]==255);++presents;}
'''
unit+=function('restore_retained_lock',source)+'\n'+function('prepare_startup_frame',source)+r'''
int main(void){
 uint8_t front[BYTES],back[BYTES];EpdiyHighlevelState hl={back};
 for(unsigned resume=0;resume<2;++resume)for(unsigned vcom=0;vcom<2;++vcom)
 for(pin=false;;pin=true){
  for(cached=false;;cached=true){
   for(unsigned mount=0;mount<3;++mount)for(valid=false;;valid=true){
    mounted=mount!=0;pending=mount==2?1000:0;
    probes=waits=reads=power=clears=censuses=presents=0;
    memset(front,0x42,BYTES);memset(back,0x55,BYTES);
    bool expected=resume&&vcom&&!pin&&cached&&mount==1&&valid;
    assert(prepare_startup_frame(&hl,front,resume,vcom)==expected);
    assert(waits<=150&&probes<=150);
    if(expected){assert(!power&&!clears&&!presents&&!censuses);assert(!memcmp(front,back,BYTES));}
    else{
     assert(power==1&&clears==1&&presents==1&&censuses==!resume);
     for(unsigned i=0;i<BYTES;++i)assert(front[i]==(resume?(i==BYTES/2?0:255):0x56));
    }
    if(!resume||!vcom||pin||!cached)assert(!reads&&!probes);
    if(mount!=1)assert(!reads);
    if(valid)break;
   }
   if(cached)break;
  }
  if(pin)break;
 }
 puts("PASS: real cold/resume art across PIN/VCOM/cache/card/corruption; lock art on every resume fallback, retained front/back on hit, bounded pending media without cache I/O, original cold splash and one output");
}
'''
with tempfile.TemporaryDirectory(prefix='pico-wake-art-') as directory:
 p=Path(directory);(p/'test.c').write_text(unit)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
