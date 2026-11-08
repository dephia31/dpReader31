#!/usr/bin/env python3
"""Exercise the streaming card store and the S31 failover poll with simulated Wi-Fi."""
from pathlib import Path
import subprocess
import tempfile
from home_cover_cache_host_test import function

root = Path(__file__).resolve().parents[1]
component = root / 'components/read_pico_transfer'
policy = component / 'transfer_policy.h'
source = component / 'read_pico_transfer.c'
policy_text = policy.read_text()
credentials = policy_text[policy_text.rindex('typedef struct {', 0, policy_text.index('} transfer_credentials_t;')):
                          policy_text.index('} transfer_credentials_t;') + len('} transfer_credentials_t;')]
connection = policy_text[policy_text.rindex('typedef struct {', 0, policy_text.index('} transfer_connection_t;')):
                         policy_text.index('} transfer_connection_t;') + len('} transfer_connection_t;')]
unit = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#define DPREADER_BOARD_METALIO47 1
#define TRANSFER_WIFI_DIRECTORY "DPRWIFI"
#define WIFI_PROFILE_ATTEMPT_MS 4500
#define READ_PICO_TRANSFER_WIFI_PROFILES_MAX 6
enum {ESP_OK, ESP_FAIL, ESP_ERR_INVALID_ARG, ESP_ERR_INVALID_STATE, ESP_ERR_NOT_FOUND,
      ESP_ERR_INVALID_SIZE, ESP_ERR_TIMEOUT};
typedef int esp_err_t;
'''
unit += credentials + '\n'
for name in ('clear_secret', 'credentials_valid', 'credentials_from_text'):
    unit += function(name, policy) + '\n'
unit += r'''
static transfer_credentials_t legacy;
static esp_err_t load_legacy_credentials(transfer_credentials_t *out) {*out=legacy;return ESP_OK;}
static esp_err_t store_legacy_credentials(const transfer_credentials_t *next) {
    assert(!next);
    memset(&legacy, 0, sizeof(legacy));
    return ESP_OK;
}
#include "transfer_wifi_profiles.h"
'''
unit += connection + '\n'
for name in ('connection_begin', 'connection_lost', 'connection_poll'):
    unit += function(name, policy) + '\n'
unit += r'''
enum {WIFI_AUTH_WPA_PSK, WIFI_AUTH_OPEN, WIFI_ALL_CHANNEL_SCAN, WIFI_CONNECT_AP_BY_SIGNAL,
      WPA3_SAE_PWE_BOTH, WIFI_IF_STA};
enum {READ_PICO_TRANSFER_MODE_STA, READ_PICO_TRANSFER_STARTING, READ_PICO_TRANSFER_ERROR};
typedef struct {
    struct {
        char ssid[32], password[64];
        int scan_method, sort_method, sae_pwe_h2e;
        struct {int authmode;} threshold;
        struct {bool capable;} pmf_cfg;
    } sta;
} wifi_config_t;
static bool s_started=true, s_stopping, s_shutdown_only, s_upload_active, network_owner=true;
static unsigned s_profile_count, s_profile_index, s_profiles_tried;
static transfer_connection_t s_connection;
static struct {int mode;} s_cfg;
static struct {bool network_ready; char url[64]; int state, last_error;} s_status;
static int64_t now;
static unsigned connects, disconnects, changes;
static char configured_ssid[33], published_ssid[33];
#define portENTER_CRITICAL(lock) ((void)0)
#define portEXIT_CRITICAL(lock) ((void)0)
#define TRANSFER_NETWORK_GUARD_OR_RETURN(...) do { if(!network_owner) return __VA_ARGS__; } while(0)
#define ESP_LOGW(tag,...) do { if(0) printf(__VA_ARGS__); } while(0)
static int64_t esp_timer_get_time(void){return now*1000;}
static int esp_wifi_connect(void){++connects;return ESP_OK;}
static int esp_wifi_disconnect(void){++disconnects;return ESP_OK;}
static int esp_wifi_set_config(int iface, wifi_config_t *config){
    assert(iface==WIFI_IF_STA);
    memcpy(configured_ssid,config->sta.ssid,sizeof(config->sta.ssid));
    ++changes;
    return ESP_OK;
}
static void publish_credentials(const transfer_credentials_t *record){
    strcpy(published_ssid,record?record->ssid:"");
}
'''
backup_header = (component / 'include/read_pico_transfer.h').read_text()
start = backup_header.rindex('typedef struct {', 0, backup_header.index('} read_pico_transfer_wifi_backup_t;'))
end = backup_header.index('} read_pico_transfer_wifi_backup_t;') + len('} read_pico_transfer_wifi_backup_t;')
unit += backup_header[start:end] + '\n'
unit += function('read_pico_transfer_wifi_backup_valid', source) + '\n'
unit += r'''
static bool s_wifi;
static bool claim_config(void) { return true; }
static void release_config(void) {}
#include "transfer_wifi_profiles_backup.h"
'''
for name in ('wifi_sta_config', 'begin_saved_profile', 'saved_profile_lost',
             'advance_saved_profile', 'read_pico_transfer_service_poll', 'sd_relative_valid'):
    unit += function(name, source) + '\n'
unit += r'''
static void save(const char *name, const char *password) {
    transfer_credentials_t next;
    assert(credentials_from_text(name,password,&next));
    assert(store_credentials(&next)==ESP_OK);
}
int main(void) {
    transfer_credentials_t out;
    unsigned count;
    assert(credentials_from_text("legacy","abcdefgh",&legacy));
    assert(load_credentials(&out)==ESP_OK && !strcmp(out.ssid,"legacy"));
    save("home","homepass");
    assert(wifi_profile_read(1,&out,&count)==ESP_OK && count==2 && !strcmp(out.ssid,"legacy"));
    save("work","workpass");
    save("home","newpassword");
    assert(wifi_profile_read(0,&out,&count)==ESP_OK && count==3 && !strcmp(out.password,"newpassword"));
    assert(wifi_profile_read(1,&out,&count)==ESP_OK && !strcmp(out.ssid,"work"));
    assert(rename(WIFI_PROFILES_FILE,WIFI_PROFILES_FILE ".old")==0);
    assert(load_credentials(&out)==ESP_OK && !strcmp(out.ssid,"home"));
    save("home","newpassword");
    assert(mkdir(WIFI_PROFILES_FILE ".tmp",0700)==0);
    assert(store_credentials(&out)!=ESP_OK);
    assert(rmdir(WIFI_PROFILES_FILE ".tmp")==0);
    assert(load_credentials(&out)==ESP_OK && !strcmp(out.ssid,"home"));
    assert(!sd_relative_valid("dprwifi/WIFI.BIN.old",false));
    assert(!sd_relative_valid("DPRWIFI",false));
    assert(sd_relative_valid("books/read.epub",false));

    s_profile_count=count;
    begin_saved_profile(now);
    read_pico_transfer_service_poll();
    assert(connects==1);
    now=4500;
    s_upload_active=true;
    read_pico_transfer_service_poll();
    assert(!changes&&!disconnects);
    s_upload_active=false;
    network_owner=false;
    read_pico_transfer_service_poll();
    assert(!changes);
    network_owner=true;
    read_pico_transfer_service_poll();
    assert(changes==1&&disconnects==1&&!strcmp(configured_ssid,"work"));
    assert(!strcmp(published_ssid,"work")&&s_status.state==READ_PICO_TRANSFER_STARTING);
    now+=250;read_pico_transfer_service_poll();assert(connects==2);
    now=9000;read_pico_transfer_service_poll();
    assert(changes==2&&!strcmp(configured_ssid,"legacy"));
    now=13500;read_pico_transfer_service_poll();
    assert(changes==2&&s_status.state==READ_PICO_TRANSFER_ERROR);

    // Sau khi mạng thứ hai đã online, mất mạng sẽ thử lại mạng đó trước khi chuyển tiếp.
    s_profile_index=1;
    s_profiles_tried=0;
    begin_saved_profile(now);
    s_connection.online=true;
    saved_profile_lost(now);
    unsigned previous_connects=connects;
    read_pico_transfer_service_poll();
    assert(connects==previous_connects);
    now+=1000;read_pico_transfer_service_poll();
    assert(connects==previous_connects+1 && changes==2);
    now+=3500;read_pico_transfer_service_poll();
    assert(changes==3 && !strcmp(configured_ssid,"legacy"));

    save("c","");save("d","");save("e","");
    assert(credentials_from_text("too-many","",&out));
    assert(store_credentials(&out)==ESP_ERR_INVALID_SIZE);
    assert(wifi_profile_read(0,&out,&count)==ESP_OK && count==6 && !strcmp(out.ssid,"e"));
    read_pico_transfer_wifi_backup_t archive[READ_PICO_TRANSFER_WIFI_PROFILES_MAX];
    read_pico_transfer_wifi_backup_t check[READ_PICO_TRANSFER_WIFI_PROFILES_MAX];
    assert(read_pico_transfer_export_wifi_profiles_backup(archive)==ESP_OK);
    assert(read_pico_transfer_wifi_profiles_backup_valid(archive));
    assert(!strcmp(archive[0].ssid,"e")&&!strcmp(archive[5].ssid,"legacy"));
    memcpy(check,archive,sizeof(check));
    check[1]=check[0];
    assert(read_pico_transfer_import_wifi_profiles_backup(check)==ESP_ERR_INVALID_ARG);
    memcpy(check,archive,sizeof(check));
    memset(&check[0],0,sizeof(check[0]));
    assert(!read_pico_transfer_wifi_profiles_backup_valid(check));
    assert(store_credentials(NULL)==ESP_OK);
    save("replacement","");
    assert(mkdir(WIFI_PROFILES_FILE ".tmp",0700)==0);
    assert(read_pico_transfer_import_wifi_profiles_backup(archive)!=ESP_OK);
    assert(rmdir(WIFI_PROFILES_FILE ".tmp")==0);
    assert(load_credentials(&out)==ESP_OK&&!strcmp(out.ssid,"replacement"));
    assert(read_pico_transfer_import_wifi_profiles_backup(archive)==ESP_OK);
    assert(read_pico_transfer_export_wifi_profiles_backup(check)==ESP_OK);
    assert(!memcmp(check,archive,sizeof(check))&&!strcmp(published_ssid,"e"));
    memset(check,0,sizeof(check));
    assert(read_pico_transfer_import_wifi_profiles_backup(check)==ESP_OK);
    assert(wifi_profile_read(0,&out,&count)==ESP_OK&&count==0);
    assert(store_credentials(NULL)==ESP_OK);
    assert(wifi_profile_read(0,&out,&count)==ESP_OK && count==0 && out.version==0 && legacy.version==0);
    assert(access(WIFI_PROFILES_FILE ".old", F_OK)!=0);
    puts("PASS legacy import, streaming add/update, 6-network limit, interrupted publish, backup/restore in priority order, path protection, failover, reconnect, upload protection, and forget");
}
'''
with tempfile.TemporaryDirectory() as temp:
    work = Path(temp)
    (work / 'test.c').write_text(unit)
    subprocess.run(['cc', '-std=gnu11', '-Wall', '-Wextra', '-Werror', '-fsanitize=undefined',
                    '-I' + str(component), str(work / 'test.c'), '-o', str(work / 'test')], check=True)
    subprocess.run([str(work / 'test')], cwd=work, check=True)
