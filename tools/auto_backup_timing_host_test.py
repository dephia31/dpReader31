#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Keep automatic snapshots on sleep/update paths, outside interactive navigation."""
from pathlib import Path
import re
import subprocess
import tempfile
from home_cover_cache_host_test import function
ROOT=Path(__file__).resolve().parents[1]
SLEEP=ROOT/'boards/metalio47/src/main/sleep.c'
BOOK=ROOT/'main/apps/app_book.c'
helper=function('sleep_backup_checkpoint',SLEEP)
light=function('lock_light_sleep_once',SLEEP)
deep=function('app_enter_host_sleep',SLEEP)
assert light.index('metalio_usb_blocks_sleep()') < light.index('sleep_backup_checkpoint();')
assert light.index('media_state==ESP_ERR_NOT_FINISHED') < light.index('sleep_backup_checkpoint();')
assert light.index('sleep_backup_checkpoint();') < light.index('s_lock_io_pending=true') < light.index('book_suspend_io()')
assert deep.index('if(mode==APP_SLEEP_OFF)') < deep.index('return err;',deep.index('if(mode==APP_SLEEP_OFF)')) < deep.index('sleep_backup_checkpoint();')
assert 'if (mode == APP_SLEEP_DEEP) sleep_backup_checkpoint();' in deep
assert deep.index('sleep_backup_checkpoint();') < deep.index('ttf_font_suspend_io()')
expected={'boards/metalio47/src/main/sleep.c','main/ota_update.c','main/apps/app_device_settings.c'}
actual=set()
for base in [ROOT/'main',ROOT/'boards/metalio47/src/main']:
    for path in base.rglob('*.c'):
        if path==ROOT/'main/settings.c':continue
        if re.search(r'\bapp_settings_autobackup_checkpoint\s*\(',path.read_text()):actual.add(str(path.relative_to(ROOT)))
assert actual==expected,(actual,expected)
local=function('pico_ota_install',ROOT/'main/ota_update.c')
assert local.index('pico_ota_inspect(')<local.index('app_settings_autobackup_checkpoint()')<local.index('esp_ota_begin(')
online=function('upgrade_start_pending',ROOT/'main/apps/app_device_settings.c')
assert 'if (downloading) (void)app_settings_autobackup_checkpoint();' in online
assert online.index('app_settings_autobackup_checkpoint()')<online.index('pico_online_download()')
assert 'save_progress();' in function('before_lock',BOOK)
unit=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
static bool enabled=true,usb,online,pending,ready=true,cover=true,avatar=true,s_save_failed;
#define s_lock_io_pending pending
static int calls,io_checks,cover_checks,avatar_checks,result;
static bool app_settings_autobackup_enabled(void){return enabled;}
static bool usb_storage_active(void){return usb;}
static bool pico_online_busy(void){return online;}
static bool read_pico_sd_io_ready(void){++io_checks;return ready;}
static bool book_cover_jobs_quiesce(int wait){assert(!wait);++cover_checks;return cover;}
static bool ui_avatar_quiesce(int wait){assert(!wait);++avatar_checks;return avatar;}
static int app_settings_autobackup_checkpoint(void){++calls;return result;}
'''+function('app_book_progress_saved',BOOK)+'\n'+helper+r'''
int main(void){
 enabled=false;sleep_backup_checkpoint();assert(!calls&&!io_checks);
 enabled=true;usb=true;sleep_backup_checkpoint();assert(!calls&&!io_checks);usb=false;
 online=true;sleep_backup_checkpoint();assert(!calls&&!io_checks);online=false;
 pending=true;sleep_backup_checkpoint();assert(!calls&&!io_checks);pending=false;
 ready=false;sleep_backup_checkpoint();assert(!calls);ready=true;
 s_save_failed=true;sleep_backup_checkpoint();assert(!calls&&!cover_checks);s_save_failed=false;
 cover=false;sleep_backup_checkpoint();assert(!calls&&!avatar_checks);cover=true;
 avatar=false;sleep_backup_checkpoint();assert(!calls);avatar=true;
 sleep_backup_checkpoint();assert(calls==1);
 result=-1;sleep_backup_checkpoint();assert(calls==2); // A failed snapshot does not abort the sleep caller.
 puts("PASS automatic backup timing: sleep/update only, before IO suspension, no power-off/navigation hook, readiness/progress guards");
}
'''
with tempfile.TemporaryDirectory() as folder:
    p=Path(folder);(p/'test.c').write_text(unit)
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
