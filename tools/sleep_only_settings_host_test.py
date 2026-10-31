#!/usr/bin/env python3
"""Real per-firmware Sleep migration/persistence and key-restore retries.
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
typedef int esp_err_t;typedef int nvs_handle_t;
#define ESP_OK 0
#define ESP_ERR_INVALID_ARG 1
#define ESP_ERR_INVALID_STATE 2
#define APP_SLEEP_OFF_HOURS_MAX 24
#define NVS_NS "test"
#define NVS_KEY_SLEEP_OFF_HOURS "sleep_off_h"
#define NVS_READWRITE 1
#define ESP_SLEEP_WAKEUP_ALL 99
#define pdMS_TO_TICKS(ms) (ms)
#define ESP_LOGE(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
static uint8_t s_sleep_off_hours,stored=12,pending;
static char marker[32]="old",pending_marker[32];
static struct{char version[32];}firmware={"new"};
static const void *desc_pointer(void){return &firmware;}
#define esp_app_get_description() ((const __typeof__(firmware)*)desc_pointer())
static int fail_at,calls,commits;
static int op(void){return ++calls==fail_at?9:0;}
static int nvs_open(const char *ns,int mode,int *h){assert(!strcmp(ns,NVS_NS)&&mode==NVS_READWRITE);*h=7;pending=stored;strcpy(pending_marker,marker);return op();}
static int nvs_get_str(int h,const char *key,char *out,size_t *size){assert(h==7&&!strcmp(key,"sleep_fw")&&*size>=strlen(marker)+1);int e=op();if(!e)strcpy(out,marker);return e;}
static int nvs_get_u8(int h,const char *key,uint8_t *out){assert(h==7&&!strcmp(key,NVS_KEY_SLEEP_OFF_HOURS));int e=op();if(!e)*out=stored;return e;}
static int nvs_set_u8(int h,const char *key,uint8_t value){assert(h==7&&!strcmp(key,NVS_KEY_SLEEP_OFF_HOURS));int e=op();if(!e)pending=value;return e;}
static int nvs_set_str(int h,const char *key,const char *value){assert(h==7&&!strcmp(key,"sleep_fw"));int e=op();if(!e)snprintf(pending_marker,sizeof(pending_marker),"%s",value);return e;}
static int nvs_commit(int h){assert(h==7);int e=op();if(!e){stored=pending;strcpy(marker,pending_marker);++commits;}return e;}
static void nvs_close(int h){assert(h==7);}
static bool s_lock_keys_restore_pending;
static unsigned restores,remaining_failures,resumes,delays;
static int metalio_restore_key_io(void){++restores;if(remaining_failures){--remaining_failures;return 9;}return 0;}
static void read_pico_pmu_keys_resume(void){++resumes;}
static void vTaskDelay(unsigned ms){assert(ms==20);++delays;}
static int esp_sleep_disable_wakeup_source(int source){assert(source==99);return 0;}
'''
unit+=next(line for line in (ROOT/'main/settings.h').read_text().splitlines() if line.startswith('#define APP_SLEEP_OFF_HOURS_DEFAULT '))+'\n'
for name,path in [('sleep_policy_load','main/settings.c'),('app_settings_set_sleep_off_hours','main/settings.c'),('lock_restore_key_wake','boards/metalio47/src/main/sleep.c')]:unit+=function(name,ROOT/path)+'\n'
assert 'sleep_policy_load();' in function('app_settings_init',ROOT/'main/settings.c')
unit+=r'''
int main(void){
 sleep_policy_load();assert(s_sleep_off_hours==0&&stored==0&&!strcmp(marker,"new"));
 assert(app_settings_set_sleep_off_hours(7)==0&&stored==7&&s_sleep_off_hours==7);
 int count=commits;s_sleep_off_hours=0;sleep_policy_load();assert(s_sleep_off_hours==7&&commits==count);
 strcpy(firmware.version,"next");sleep_policy_load();assert(s_sleep_off_hours==0&&stored==0&&!strcmp(marker,"next"));
 assert(app_settings_set_sleep_off_hours(24)==0);sleep_policy_load();assert(s_sleep_off_hours==24&&stored==24);
 assert(app_settings_set_sleep_off_hours(25)==ESP_ERR_INVALID_ARG&&s_sleep_off_hours==24);
 for(int fail=1;fail<=4;++fail){calls=0;fail_at=fail;assert(app_settings_set_sleep_off_hours(2)!=0&&s_sleep_off_hours==24&&stored==24);}
 fail_at=0;
 for(int fail=3;fail<=5;++fail){strcpy(marker,"old");stored=12;calls=0;fail_at=fail;sleep_policy_load();assert(s_sleep_off_hours==0&&stored==12&&!strcmp(marker,"old"));
  calls=0;fail_at=0;sleep_policy_load();assert(stored==0&&!strcmp(marker,"next"));}
 assert(app_settings_set_sleep_off_hours(5)==0);sleep_policy_load();assert(s_sleep_off_hours==5);
 remaining_failures=2;lock_restore_key_wake();assert(restores==3&&resumes==1&&!s_lock_keys_restore_pending&&delays==2);
 remaining_failures=9;restores=resumes=delays=0;lock_restore_key_wake();assert(restores==3&&!resumes&&s_lock_keys_restore_pending);
 remaining_failures=0;lock_restore_key_wake();assert(restores==4&&resumes==1&&!s_lock_keys_restore_pending);
 puts("PASS: actual firmware upgrade resets Sleep once, user choice survives same-version boots, next upgrade resets, NVS faults retry/retain choices, key-restore recovery");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);c=p/'test.c';exe=p/'test';c.write_text(unit)
 subprocess.run(['cc','-std=gnu11','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(c),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
