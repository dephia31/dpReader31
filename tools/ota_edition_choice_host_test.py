#!/usr/bin/env python3
"""Exercise first OTA choice persistence and actual cross-edition feed admission."""
from pathlib import Path
import subprocess
import tempfile
from home_cover_cache_host_test import function

ROOT = Path(__file__).resolve().parents[1]
ONLINE = ROOT/'boards/metalio47/src/main/ota_online.c'
unit = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#define DPREADER_BOARD_METALIO47 1
#include "firmware_edition.h"
#include "ota_online.h"
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
#define NVS_NS "test"
#define portMAX_DELAY 0
typedef int nvs_handle_t;
#define NVS_READWRITE 1
static uint8_t s_ota_edition=255, stored_choice=255;
static int failed;
static int backup_nvs_open(const char *n,int mode,nvs_handle_t *out){(void)n;(void)mode;*out=1;return failed==1?ESP_FAIL:ESP_OK;}
static int backup_nvs_set_u8(nvs_handle_t h,const char *key,uint8_t value){(void)h;assert(!strcmp(key,"ota_edition"));stored_choice=value;return failed==2?ESP_FAIL:ESP_OK;}
static int settings_backup_commit(nvs_handle_t h){(void)h;return failed==3?ESP_FAIL:ESP_OK;}
static void nvs_close(nvs_handle_t h){(void)h;}
static bool s_target_love,s_cancel;
static int s_mutex;
static pico_update_status_t storage,*s_status=&storage;
static pico_release_t candidate;
static pico_update_state_t outcome;
typedef struct {char version[32],project_name[32];} esp_app_desc_t;
static esp_app_desc_t running={.version="1.3.3-vn",.project_name="Kiikoread_Metalio_S31"};
static const esp_app_desc_t *esp_app_get_description(void){return &running;}
static bool layout_valid(void){return true;}
static void *heap_caps_malloc(size_t bytes,int caps){(void)caps;return malloc(bytes);}
static void *heap_caps_calloc(size_t n,size_t bytes,int caps){(void)caps;return calloc(n,bytes);}
#define heap_caps_free free
#define atomic_load(p) (*(p))
static void state(pico_update_state_t result,const char *message){(void)message;outcome=result;}
static void xSemaphoreTake(int mutex,int wait){(void)mutex;(void)wait;}
static void xSemaphoreGive(int mutex){(void)mutex;}
static bool wait_network(void){return true;}
typedef void *esp_http_client_handle_t;
static bool read_once;
static char feed_url[256];
static void *open_http(const char *url,int *code,int64_t *length){strcpy(feed_url,url);*code=200;*length=2;read_once=false;return (void*)1;}
static int read_chunk(void *client,uint8_t *out,int capacity,int64_t *idle){(void)client;(void)capacity;(void)idle;if(read_once)return 0;read_once=true;memcpy(out,"{}",2);return 2;}
static bool esp_http_client_is_complete_data_received(void *client){(void)client;return true;}
static void esp_http_client_cleanup(void *client){(void)client;}
static int64_t esp_timer_get_time(void){return 0;}
#define pdMS_TO_TICKS(n) (n)
static void vTaskDelay(int ticks){(void)ticks;}
static int pico_release_parse(const char *json,size_t length,pico_release_t *out);
'''
# Match the public signature, while controlling valid manifest contents for admission tests.
unit=unit.replace('static int pico_release_parse(const char *json,size_t length,pico_release_t *out);',
'''esp_err_t pico_release_parse(const char *json,size_t length,pico_release_t *out){
(void)json;(void)length;*out=candidate;return ESP_OK;}''')
for name,path in [('app_settings_ota_edition',ROOT/'main/settings.c'),
                  ('app_settings_set_ota_edition',ROOT/'main/settings.c'),
                  ('version_parts',ROOT/'main/ota_release.c'),
                  ('pico_version_compare',ROOT/'main/ota_release.c'),
                  ('pico_release_url_valid',ROOT/'main/ota_release.c'),
                  ('check_feed',ONLINE)]:
    unit+=function(name,path)+chr(10)
unit+='''
int main(void) {
 assert(app_settings_ota_edition()==255);
 for(failed=1;failed<=3;++failed)assert(app_settings_set_ota_edition(1)!=ESP_OK&&app_settings_ota_edition()==255);
 failed=0;assert(app_settings_set_ota_edition(1)==ESP_OK&&app_settings_ota_edition()==1&&stored_choice==1);
 assert(app_settings_set_ota_edition(2)==ESP_ERR_INVALID_ARG&&app_settings_ota_edition()==1);
 strcpy(candidate.version,"1.3.3-love-vn");strcpy(candidate.project,running.project_name);
 strcpy(candidate.minimum_base_version,"1.0.0");
 s_target_love=true;
 assert(check_feed()&&outcome==PICO_UPDATE_AVAILABLE&&strstr(feed_url,"/love-book/"));
 s_target_love=false;assert(!check_feed()&&outcome==PICO_UPDATE_FAILED);
 strcpy(candidate.version,"1.3.3-vn");assert(check_feed()&&outcome==PICO_UPDATE_LATEST);
 strcpy(candidate.version,"1.3.4-vn");assert(check_feed()&&outcome==PICO_UPDATE_AVAILABLE);
 strcpy(running.version,"1.3.3-love-vn");strcpy(candidate.version,"1.3.3-vn");
 assert(check_feed()&&outcome==PICO_UPDATE_AVAILABLE);
 strcpy(candidate.version,"1.3.2-vn");assert(check_feed()&&outcome==PICO_UPDATE_LATEST);
 strcpy(candidate.project,"OtherBoard");assert(!check_feed()&&outcome==PICO_UPDATE_FAILED);
 assert(pico_release_url_valid("https://dephia31.github.io/dpReader31/releases/love-book/dpReader_metalio47_firmware.bin"));
 assert(pico_release_url_valid("https://raw.githubusercontent.com/dephia31/dpReader31/main/releases/love-book/dpReader_metalio47_firmware.bin"));
 assert(!pico_release_url_valid("https://dephia31.github.io/dpReader31/releases/love-book/../bad.bin"));
 assert(!pico_release_url_valid("https://dephia31.github.io/dpReader31/releases/other/bad.bin"));
 puts("PASS first OTA choice, failed-save rollback, Full/Love feeds, confirmed same-version switching, wrong-branch/project rejection and downgrade refusal");
}
'''
unit=unit.replace('#include <time.h>','#include <time.h>'+chr(10)+'#include <ctype.h>')
with tempfile.TemporaryDirectory() as folder:
    path=Path(folder)
    (path/'esp_err.h').write_text('#pragma once\ntypedef int esp_err_t;\n#define ESP_OK 0\n#define ESP_FAIL -1\n#define ESP_ERR_INVALID_ARG 1\n')
    (path/'test.c').write_text(unit)
    subprocess.run(['cc','-std=gnu11','-Wall','-Wextra','-Werror','-fsanitize=undefined,bounds',
                    '-I'+str(path),'-I'+str(ROOT/'main'),str(path/'test.c'),'-o',str(path/'test')],check=True)
    subprocess.run([str(path/'test')],check=True)
