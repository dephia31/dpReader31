#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# Verify the actual local-install helper returns caches/rails before flash, forwarding failures.
from pathlib import Path
import subprocess,tempfile
from home_cover_cache_host_test import function
root=Path(__file__).resolve().parents[1]
unit=r'''
#include <assert.h>
#include <stdbool.h>
#define ESP_ERR_NOT_FINISHED 99
static bool jobs_busy,avatar_busy;
static bool book_cover_jobs_quiesce(unsigned ms){assert(ms==2000);return !jobs_busy;}
static bool ui_avatar_quiesce(unsigned ms){assert(ms==2000);return !avatar_busy;}
#include <stdio.h>
#include <string.h>
typedef int esp_err_t;
#define PICO_OTA_UPDATE_PATH "/sdcard/Metalio-S31-update.bin"
static char s_upgrade_notice[64];
static int step,result;
static void app_home_cover_mode_changed(void){assert(step++==0);}
static void app_book_cover_mode_changed(void){assert(step++==1);}
static void epd_poweroff(void){assert(step++==2);}
static int pico_ota_install(const char *path,char *notice,size_t capacity){
 assert(step++==3 && !strcmp(path,PICO_OTA_UPDATE_PATH));
 assert(notice==s_upgrade_notice && capacity==sizeof(s_upgrade_notice));
 snprintf(notice,capacity,"result=%d",result);return result;
}
'''+function('install_local_update',root/'main/apps/app_device_settings.c')+r'''
int main(void){
 jobs_busy=true;assert(install_local_update()==ESP_ERR_NOT_FINISHED&&!step);jobs_busy=false;
 avatar_busy=true;assert(install_local_update()==ESP_ERR_NOT_FINISHED&&!step);avatar_busy=false;
 const int errors[]={0,-1,0x101,0x103,0x108};
 for(unsigned i=0;i<sizeof(errors)/sizeof(errors[0]);++i){
  step=0;result=errors[i];assert(install_local_update()==result && step==4 && s_upgrade_notice[0]);
 }
 puts("PASS: production TF installer clears both caches, turns rails off before flash, forwards success/failures and preserves unique S31 path");
}
'''
with tempfile.TemporaryDirectory() as tmp:
 d=Path(tmp);(d/'test.c').write_text(unit)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-g','-fsanitize=address,undefined',str(d/'test.c'),'-o',str(d/'test')],check=True)
 subprocess.run([str(d/'test')],check=True)
