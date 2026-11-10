"""Exercise the persistent inversion setter and S31 settings row wiring."""
from pathlib import Path
import subprocess,tempfile
from home_cover_cache_host_test import function
root=Path(__file__).resolve().parents[1]
src=root/'firmware/shared/main/settings.c'
unit=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
typedef int nvs_handle_t;typedef int esp_err_t;
#define ESP_OK 0
#define NVS_NS "test"
#define NVS_READWRITE 1
#define NVS_KEY_INVERT "screen_invert"
static bool s_screen_inverted;
static int open_error,write_error,commit_error,writes;
static uint8_t stored;
static int backup_nvs_open(const char *n,int m,int *h){(void)n;(void)m;*h=1;return open_error;}
static int backup_nvs_set_u8(int h,const char *k,uint8_t v){assert(h==1&&!strcmp(k,NVS_KEY_INVERT));++writes;if(!write_error)stored=v;return write_error;}
static int settings_backup_commit(int h){assert(h==1);return commit_error;}
static void nvs_close(int h){assert(h==1);}
'''
for name in ['app_settings_screen_inverted','app_settings_set_screen_inverted']:unit+=function(name,src)+'\n'
unit+=r'''
int main(void){
 assert(!app_settings_screen_inverted());
 assert(app_settings_set_screen_inverted(false)&&!writes);
 open_error=1;assert(!app_settings_set_screen_inverted(true)&&!s_screen_inverted&&!writes);
 open_error=0;write_error=1;assert(!app_settings_set_screen_inverted(true)&&!s_screen_inverted);
 write_error=0;commit_error=1;assert(!app_settings_set_screen_inverted(true)&&!s_screen_inverted);
 commit_error=0;assert(app_settings_set_screen_inverted(true)&&s_screen_inverted&&stored==1);
 assert(app_settings_set_screen_inverted(false)&&!s_screen_inverted&&stored==0);
 puts("PASS inversion preference: default off, NVS errors retain selection, successful toggles persist");
}
'''
ui=(root/'firmware/shared/main/apps/app_device_settings.c').read_text()
assert '#define SETTINGS_DISPLAY_ROWS 9' in ui
assert '"Đèn nền & Rung","Đảo màu"' in ui
assert 'else if(row==7)s_page=SETTINGS_FRONTLIGHT;' in ui
assert 'app_settings_set_screen_inverted(!app_settings_screen_inverted())' in ui
assert 's_screen_inverted=nvs_get_u8(h,NVS_KEY_INVERT,&inverted)==ESP_OK && inverted==1;' in src.read_text()
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(unit)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
