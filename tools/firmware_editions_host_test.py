#!/usr/bin/env python3
"""Exercise firmware edition identity, isolated build plans and bounded OTA checks."""
from pathlib import Path
import json
import subprocess
import sys
import struct
import tempfile
from build_firmware import ROOT, metadata, check_image
from home_cover_cache_host_test import function

full, love = (metadata('metalio47', e) for e in ('full', 'love-book'))
assert love['version'] == full['version'].removesuffix('-vn') + '-love-vn'
assert full['image_chip_id'] == love['image_chip_id'] == 32
with tempfile.TemporaryDirectory() as folder:
    sdk=Path(folder)
    (sdk/'tools').mkdir();(sdk/'tools/idf.py').write_text('')
    (sdk/'components/soc/esp32s31').mkdir(parents=True)
    output=subprocess.check_output([sys.executable,str(ROOT/'tools/build_firmware.py'),'--board','metalio47','--idf-metalio47',str(sdk),'--dry-run'],text=True)
    plans=[json.loads(line) for line in output.splitlines()]
    assert len(plans)==2
    for plan,edition in zip(plans,('full','love-book')):
        command=plan['command'];assert '-DDPREADER_EDITION='+edition in command
        assert '/'+edition+'/metalio47/build' in command[command.index('-B')+1]
for edition, info in (('full',full),('love-book',love)):
    image=bytearray(288);image[0]=0xe9
    struct.pack_into('<H',image,12,32);struct.pack_into('<I',image,32,0xabcd5432)
    version=info['version'].encode();image[48:48+len(version)]=version
    project=b'Kiikoread_Metalio_S31';image[80:80+len(project)]=project
    assert check_image(image,'metalio47',edition)['version']==info['version']
    try:check_image(image,'metalio47','love-book' if edition=='full' else 'full')
    except ValueError:pass
    else:raise AssertionError('Cross-edition package validation succeeded')
with tempfile.TemporaryDirectory() as folder:
    p = Path(folder)
    c = r'''
#include <assert.h>
#include "firmware_edition.h"
int main(void) {
 char bad[32];memset(bad,'a',sizeof(bad));
 assert(firmware_same_edition("1.3.3RC4-vn",32,"1.3.2-vn",32));
 assert(firmware_same_edition("1.3.3RC4-love-vn",32,"1.3.3-love-vn",32));
 assert(!firmware_same_edition("1.3.3RC4-vn",32,"1.3.3RC4-love-vn",32));
 assert(!firmware_same_edition("1.3.3RC4-love-vn",32,"1.3.3-vn",32));
 assert(!firmware_same_edition(bad,32,"1.3.3-vn",32));
}
'''
    (p/'test.c').write_text(c)
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined,bounds','-I'+str(ROOT/'main'),str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
    # Actual settings getters are inline constants in Love Book; saved Full state is ignored.
    stub=p/'esp_err.h';stub.write_text('typedef int esp_err_t;\n')
    settings_unit='#include <assert.h>\n#include "settings.h"\n#define ESP_OK 0\n#define ESP_ERR_NOT_SUPPORTED 1\nstatic uint8_t s_lock_style;static uint8_t s_simple_utilities[2];\n'
    for name in ('app_settings_set_ui_mode','app_settings_set_simple_ui','app_settings_lock_style','app_settings_simple_utility'):
        settings_unit+=function(name,ROOT/'main/settings.c')+'\n'
    settings_unit+='int main(void){assert(app_settings_simple_ui());assert(app_settings_simple_full_ui());assert(app_settings_set_ui_mode(0)==ESP_ERR_NOT_SUPPORTED);assert(app_settings_set_ui_mode(2)==ESP_OK);assert(app_settings_set_simple_ui(false)==ESP_ERR_NOT_SUPPORTED);s_lock_style=2;assert(app_settings_lock_style()==0&&s_lock_style==2);s_simple_utilities[0]=APP_HOME_UTIL_LIBRARY;assert(app_settings_simple_utility(2)==APP_HOME_UTIL_FILES);}'
    (p/'settings.c').write_text(settings_unit)
    subprocess.run(['cc','-std=c11','-DDPREADER_LOVE_BOOK=1','-DDPREADER_BOARD_METALIO47=1','-I'+str(p),'-I'+str(ROOT/'main'),str(p/'settings.c'),'-o',str(p/'settings')],check=True)
    subprocess.run([str(p/'settings')],check=True)
    wake_unit = r'''
#include <assert.h>
#include <stddef.h>
#include "lock_pin.h"
typedef void EpdiyHighlevelState;
typedef void *cst836u_handle_t, *sc7a20h_handle_t;
static bool s_boot_lock_draw, s_boot_power_on, s_boot_power_wake, resume;
static unsigned presses=1, waits, restored;
static void lock_hint_begin(void *frame){(void)frame;}
static void lock_hint_end(void){}
static void lock_resume_service(void){}
static bool pico_boot_has_resume(void){return resume;}
static bool pico_boot_lock_pending(void){return false;}
static void pico_boot_ready(void){}
static void pico_boot_hold_resume(void){}
static unsigned app_settings_wake_presses(void){return presses;}
static bool metalio_boot_power_key(void){return true;}
static void read_pico_pmu_latch_power_wakeup(void){}
static void lock_wait_for_unlock(void *arg){(void)arg;++waits;}
static void app_lock_resume_frame(void *arg){(void)arg;++restored;}
static void ble_pt_resume_after_sleep(void){}
static void read_pico_transfer_resume_after_sleep(void){}
'''
    wake_unit += function('app_lock_boot_gate',ROOT/'boards/metalio47/src/main/sleep.c') + r'''
int main(void){
 assert(!lock_pin_enabled());
 app_lock_boot_gate(NULL,NULL,NULL);
 assert(restored==1 && waits==0);
 resume=true; s_boot_power_on=true;
 app_lock_boot_gate(NULL,NULL,NULL);
 assert(restored==2 && waits==0);
 presses=2;
 app_lock_boot_gate(NULL,NULL,NULL);
 assert(restored==3 && waits==1);
}
'''
    (p/'wake.c').write_text(wake_unit)
    subprocess.run(['cc','-std=c11','-DDPREADER_LOVE_BOOK=1','-I'+str(p),'-I'+str(ROOT/'main'),str(p/'wake.c'),'-o',str(p/'wake')],check=True)
    subprocess.run([str(p/'wake')],check=True)
# Exercise production version parsing and its stable/RC order for both suffixes.
with tempfile.TemporaryDirectory() as folder:
    p=Path(folder)
    unit='#include <assert.h>\n#include <stdbool.h>\n#include <string.h>\n#include <stdlib.h>\n#include <ctype.h>\n'
    unit+=function('version_parts',ROOT/'main/ota_release.c')+'\n'+function('pico_version_compare',ROOT/'main/ota_release.c')
    unit+='\nint main(void){unsigned n[4];assert(version_parts("1.3.3-love-vn",n));assert(version_parts("1.3.3RC4-love-vn",n));assert(!version_parts("1.3.3-other",n));assert(pico_version_compare("1.3.3-love-vn","1.3.3RC4-love-vn")>0);assert(pico_version_compare("1.3.3RC10-love-vn","1.3.3RC4-love-vn")>0);assert(pico_version_compare("1.3.4RC1-love-vn","1.3.3-love-vn")>0);assert(pico_version_compare("1.3.3-vn","1.3.3RC4-vn")>0);}'
    (p/'versions.c').write_text(unit)
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined,bounds',str(p/'versions.c'),'-o',str(p/'versions')],check=True)
    subprocess.run([str(p/'versions')],check=True)
# Verify the actual local OTA validator and online release admission call bounded identity checks.
assert 'firmware_same_edition' in function('pico_ota_inspect',ROOT/'main/ota_update.c')
print('PASS: distinct edition versions; fixed Love Book UI getters; bounded OTA/TF cross-edition rejection')
