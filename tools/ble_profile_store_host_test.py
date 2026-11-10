"""Exercise the production BLE SD journal and corruption/IO recovery."""
from pathlib import Path
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parents[1]
source = (ROOT/'components/ble_page_turner/src/ble_page_turner.c').read_text()
def function(name):
    import re
    match = re.search(r'^(?:static )?[^\n]+\b'+name+r'\([^;{}]*\)\s*\{',source,re.M)
    assert match,name
    pos=match.end(); depth=1
    while depth:
        depth+=(source[pos]=='{')-(source[pos]=='}'); pos+=1
    return source[match.start():pos]+'\n'
with tempfile.TemporaryDirectory() as directory:
    out=Path(directory)
    (out/'esp_err.h').write_text('typedef int esp_err_t;\n#define ESP_OK 0\n#define ESP_FAIL -1\n#define ESP_ERR_NOT_FOUND 1\n#define ESP_ERR_INVALID_STATE 2\n#define ESP_ERR_INVALID_ARG 3\n#define ESP_ERR_NO_MEM 4\n')
    (out/'read_pico_sd.h').write_text('#include <stdbool.h>\nstatic bool card_ready=true;\nstatic bool read_pico_sd_io_ready(void){return card_ready;}\n')
    code=r'''
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include "esp_err.h"
#define BLE_PT_MAX_BONDS 4
#define BLE_PT_FRAME_MAX 16
typedef struct {char addr[18];char name[32];uint8_t addr_type;} ble_pt_bond_t;
typedef struct {uint8_t len,bytes[16];} ble_pt_frame_t;
#include "ble_pt_backup.h"
'''+function('binding_backup_frame_valid')+function('ble_pt_bindings_backup_valid')+function('profiles_merge_bindings')+r'''
static bool fail_sync;
static int test_sync(int fd){return fail_sync?-1:fsync(fd);}
#define fsync test_sync
#define BLE_PROFILE_DIRECTORY "profiles"
#include "ble_profile_store.h"
#undef fsync
int main(void){
 ble_pt_bindings_backup_t merged={0}, incoming={0}, original;
 strcpy(merged.entries[0].key,"b_11111111");
 merged.entries[0].frames[0][0]=1;merged.entries[0].frames[0][1]=7;
 original=merged;
 assert(profiles_merge_bindings(&merged,&incoming)==ESP_OK&&!memcmp(&merged,&original,sizeof(merged)));
 strcpy(incoming.entries[0].key,"b_22222222");
 assert(profiles_merge_bindings(&merged,&incoming)==ESP_OK&&merged.entries[0].frames[0][1]==7);
 strcpy(incoming.entries[0].key,"b_11111111");incoming.entries[0].frames[0][1]=8;
 assert(profiles_merge_bindings(&merged,&incoming)==ESP_OK&&merged.entries[0].frames[0][1]==8);
 strcpy(merged.entries[2].key,"b_33333333");strcpy(merged.entries[3].key,"b_44444444");original=merged;
 strcpy(incoming.entries[0].key,"b_55555555");
 assert(profiles_merge_bindings(&merged,&incoming)==ESP_ERR_NO_MEM&&!memcmp(&merged,&original,sizeof(merged)));
 ble_profile_disk_t value={0},readback={0};
 assert(profile_read(&readback)==ESP_ERR_NOT_FOUND);
 value.count=1;strcpy(value.peers[0].addr,"2a:07:98:4e:65:01");strcpy(value.peers[0].name,"P4");
 strcpy(value.bindings.entries[0].key,"b_12345678");
 value.bindings.entries[0].frames[0][0]=1;value.bindings.entries[0].frames[0][1]=0x51;
 assert(profile_write(&value)==ESP_OK&&value.sequence==1);
 assert(profile_read(&readback)==ESP_OK&&!memcmp(&value,&readback,sizeof(value)));
 value.bindings.entries[0].frames[0][1]=0x52;
 assert(profile_write(&value)==ESP_OK&&value.sequence==2);
 assert(profile_read(&readback)==ESP_OK&&readback.bindings.entries[0].frames[0][1]==0x52);
 FILE *f=fopen("profiles/PROFILE0.BIN","wb");assert(f);fputs("broken",f);fclose(f);
 assert(profile_read(&readback)==ESP_OK&&readback.sequence==1&&readback.bindings.entries[0].frames[0][1]==0x51);
 value=readback;card_ready=false;
 assert(profile_write(&value)==ESP_ERR_INVALID_STATE&&value.sequence==1);
 assert(profile_read(&readback)==ESP_ERR_INVALID_STATE);card_ready=true;
 fail_sync=true;assert(profile_write(&value)==ESP_FAIL&&value.sequence==1);fail_sync=false;
 assert(profile_read(&readback)==ESP_OK);value=readback;
 value.count=0;memset(value.peers,0,sizeof(value.peers));memset(&value.bindings,0,sizeof(value.bindings));
 assert(profile_write(&value)==ESP_OK);
 assert(profile_read(&readback)==ESP_OK&&readback.count==0);
 value.count=5;assert(profile_write(&value)==ESP_ERR_INVALID_ARG);
 value=readback;value.sequence=UINT32_MAX;assert(profile_write(&value)==ESP_ERR_INVALID_STATE);
 f=fopen("profiles/PROFILE0.BIN","wb");assert(f);fclose(f);
 f=fopen("profiles/PROFILE1.BIN","wb");assert(f);fclose(f);
 assert(profile_read(&readback)==ESP_FAIL);
 puts("PASS BLE SD journal: learned frames, reload, two slots, corruption recovery, unavailable/USB card, failed sync, forget, limits, sequence overflow, corrupt-both rejection");
}
'''
    (out/'test.c').write_text(code)
    subprocess.run(['cc','-std=c11','-D_DARWIN_C_SOURCE','-Wall','-Wextra','-Werror','-fsanitize=undefined','-I'+str(out),'-I'+str(ROOT/'components/ble_page_turner/include'),'-I'+str(ROOT/'components/ble_page_turner/src'),str(out/'test.c'),'-o',str(out/'test')],check=True)
    subprocess.run([str(out/'test')],cwd=out,check=True)
