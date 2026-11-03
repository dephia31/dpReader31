#!/usr/bin/env python3
"""Execute the actual loop timeout section with all network/BLE activity gates."""
from pathlib import Path
import subprocess,tempfile
ROOT=Path(__file__).resolve().parents[1]
source=(ROOT/'main/app/app_loop.c').read_text()
a=source.index('        read_pico_transfer_status_t transfer;\n        read_pico_transfer_get_status(&transfer);',source.index('WiFi 在 C6'))
b=source.index('#if KIIKO_BOARD_METALIO47\n        ble_pt_sync',a)
code=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
enum {READ_PICO_TRANSFER_STOPPED,READ_PICO_TRANSFER_UPLOADING,READY};
typedef struct{int state;}read_pico_transfer_status_t;
static struct{int64_t now_ms;}ctx;
static int app_device_settings,other;
static int64_t last_wifi_active_ms,last_ble_active_ms;
static bool opds_open;
static bool opds_session_active(void){return opds_open;}
static bool job,sync_job,ota,usb,bt,connected,connecting,scanning,admit;
static uint32_t generation,last_network_generation;
static uint32_t read_pico_transfer_network_generation(void){return generation;}
static unsigned wifi_timeout,ble_timeout,stop_requests,stops;
static read_pico_transfer_status_t status;
#define TAG "test"
#define ESP_LOGI(...) ((void)0)
static void read_pico_transfer_get_status(read_pico_transfer_status_t *s){*s=status;}
static bool read_pico_transfer_network_busy(void){return job;}
static bool kosync_busy(void){return sync_job;}
static bool pico_online_busy(void){return ota;}
static bool app_download_active(void){return usb;}
static unsigned app_settings_wifi_timeout_minutes(void){return wifi_timeout;}
static unsigned app_settings_ble_timeout_minutes(void){return ble_timeout;}
static bool read_pico_transfer_try_stop_if_idle(void){++stop_requests;if(admit){++stops;status.state=READ_PICO_TRANSFER_STOPPED;}return admit;}
static bool app_settings_ble_turner(void){return bt;}
static void app_settings_set_ble_turner(bool on){bt=on;}
static bool ble_pt_connected(void){return connected;}
static bool ble_pt_connecting(void){return connecting;}
static bool ble_pt_scanning(void){return scanning;}
static void tick(int *current){
'''+source[a:b]+r'''
}
static void reset(void){ctx.now_ms=120001;last_wifi_active_ms=last_ble_active_ms=1;status.state=READY;
 opds_open=false;generation=last_network_generation=0;wifi_timeout=ble_timeout=1;stop_requests=stops=0;job=sync_job=ota=usb=connected=connecting=scanning=false;admit=bt=true;
}
int main(void){
 for(int kind=0;kind<5;++kind){reset();job=kind==0;sync_job=kind==1;ota=kind==2;usb=kind==3;if(kind==4)status.state=READ_PICO_TRANSFER_UPLOADING;
  tick(&other);assert(!stop_requests&&bt&&last_wifi_active_ms==ctx.now_ms);
 }
 reset();tick(&other);assert(stops==1&&status.state==READ_PICO_TRANSFER_STOPPED&&bt);
 reset();opds_open=true;tick(&other);assert(!stop_requests);opds_open=false;ctx.now_ms+=60001;tick(&other);assert(stops==1);
 reset();generation=2;tick(&other);assert(!stop_requests&&last_wifi_active_ms==ctx.now_ms);
 ctx.now_ms+=60001;tick(&other);assert(stops==1);
 reset();admit=false;tick(&other);assert(stop_requests==1&&!stops&&status.state==READY);
 reset();wifi_timeout=0;tick(&other);assert(!stop_requests&&bt);
 for(int kind=0;kind<4;++kind){reset();status.state=READ_PICO_TRANSFER_STOPPED;job=kind==0;connected=kind==1;connecting=kind==2;scanning=kind==3;
  tick(&other);assert(bt&&last_ble_active_ms==ctx.now_ms);
 }
 reset();status.state=READ_PICO_TRANSFER_STOPPED;tick(&app_device_settings);assert(bt);
 reset();status.state=READ_PICO_TRANSFER_STOPPED;tick(&other);assert(!bt);
 reset();status.state=READ_PICO_TRANSFER_STOPPED;ble_timeout=0;tick(&other);assert(bt);
 puts("PASS actual idle policy: active OPDS/font/NTP/KoSync/OTA/upload/USB protected, busy admission respected; BLE link/connect/scan/network pause retained; idle timers and disabled timers preserved");
}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp);(p/'test.c').write_text(code)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)

# Reader input must not reset radio idle time anywhere in the scheduler.
assert 'last_input_ms = ctx.now_ms; last_wifi_active_ms' not in source
input_block=source[source.index('if (err == ESP_OK && (touch.touched || released))'):source.index('if (ctx.now_ms - last_network_poll_ms')]
assert 'last_wifi_active_ms =' not in input_block
