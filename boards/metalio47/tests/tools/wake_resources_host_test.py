#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
# 中文：实测保留屏幕参考恢复，检查挂载等待有界、损坏回退和无额外推屏。
# English: Execute retained-panel restoration; bounded mount wait, corrupt-cache fallback and no extra panel output.
from pathlib import Path
import subprocess,tempfile
from home_cover_cache_host_test import function
r=Path(__file__).resolve().parents[1]
s=(r/'main/app_main.c').read_text()
assert 'const bool resuming = pico_boot_has_resume() && reset == ESP_RST_DEEPSLEEP;' in s
assert 'if(!resuming || lock_pin_enabled())app_font_activate_system();' in s
assert 'if(first==app_at(1) && resume.reader)app_font_activate_reading();' in s
u=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#define ESP_ERR_NOT_FINISHED 1
#define pdMS_TO_TICKS(x) (x)
typedef struct{uint8_t *back_fb;}EpdiyHighlevelState;
typedef struct{bool mounted;}read_pico_sd_info_t;
static int probes,waits,remaining;static bool good;
static int read_pico_sd_get_info(read_pico_sd_info_t *p){(void)p;++probes;return remaining-->0?1:0;}
static void vTaskDelay(int ms){assert(ms==20);++waits;}
static int epd_width(void){return 684;}
static int epd_height(void){return 1216;}
static bool lock_screen_restore(uint8_t *f){memset(f,good?0x56:0x11,684u*1216u/2u);return good;}
'''+function('restore_retained_lock',r/'main/app_main.c')+r'''
int main(void){
 uint8_t front[684u*1216u/2u],back[sizeof(front)];EpdiyHighlevelState hl={back};
 for(int mount=0;mount<4;++mount)for(int valid=0;valid<2;++valid){
  memset(back,0x99,sizeof(back));remaining=mount==3?1000:mount*20;waits=probes=0;good=valid;
  assert(restore_retained_lock(&hl,front)==good);assert(waits<=150&&probes<=150);
  if(good)assert(!memcmp(front,back,sizeof(front)));else for(unsigned i=0;i<sizeof(back);++i)assert(back[i]==0x99);
 }
 puts("PASS: validated retained front/back frame, bounded missing-card wait, corrupt-cache fallback, no font/display work inside restore helper; resume routes avoid redundant system-face load");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.c';p.write_text(u);out=p.with_suffix('')
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p),'-o',str(out)],check=True)
 subprocess.run([str(out)],check=True)
