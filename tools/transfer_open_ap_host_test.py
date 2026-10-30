#!/usr/bin/env python3
"""Execute both maintained AP setup blocks and actual WiFi QR payload builder."""
from pathlib import Path
import subprocess,tempfile,re
from home_cover_cache_host_test import function
root=Path(__file__).resolve().parents[1]
source=(root/'components/read_pico_transfer/read_pico_transfer.c').read_text()
header=(root/'components/read_pico_transfer/include/read_pico_transfer.h').read_text()
unit=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define ESP_OK 0
#define ESP_MAC_WIFI_SOFTAP 1
#define WIFI_AUTH_OPEN 0
#define WIFI_AUTH_WPA2_PSK 3
#define portENTER_CRITICAL(x) ((void)(x))
#define portEXIT_CRITICAL(x) ((void)(x))
static int s_lock;
static struct{char ssid[33];}s_status;
static int esp_read_mac(uint8_t *p,int type){(void)type;memset(p,42,6);return 0;}
static char payload_copy[224];
static void ui_wifi_qr_clear(void){payload_copy[0]=0;}
static bool prepare_payload(const char *p){strcpy(payload_copy,p);return true;}
'''
for name in ('READ_PICO_TRANSFER_SSID','READ_PICO_TRANSFER_PASSWORD'):
 unit+=re.search(r'^#define '+name+r' .*',header,re.M)[0]+'\n'
blocks=re.findall(r'        uint8_t mac\[6\];[\s\S]*?portEXIT_CRITICAL\(&s_lock\);',source)
assert len(blocks)==2
for i,body in enumerate(blocks):
 unit+='static void setup'+str(i)+r'''(void){int err=0;
 struct{struct{uint8_t ssid[32],password[64];int ssid_len,channel,max_connection,authmode;}ap;}wifi;
 memset(&wifi,0,sizeof(wifi));
'''+body+r'''
 assert(wifi.ap.authmode==WIFI_AUTH_OPEN&&wifi.ap.password[0]==0);
 assert(wifi.ap.ssid_len==10&&!strcmp((char*)wifi.ap.ssid,"dpReader31"));
 assert(!strcmp(s_status.ssid,"dpReader31"));assert(wifi.ap.max_connection==1);
 return;fail:assert(!err);
}
'''
for name in ('escape_field','ui_wifi_qr_prepare'):
 unit+=function(name,root/'main/ui/ui_wifi_qr.c')+'\n'
unit+=r'''
int main(void){setup0();setup1();
 assert(ui_wifi_qr_prepare(READ_PICO_TRANSFER_SSID,READ_PICO_TRANSFER_PASSWORD));
 assert(!strcmp(payload_copy,"WIFI:T:nopass;S:dpReader31;P:;;"));
 assert(ui_wifi_qr_prepare("A;B","password"));
 assert(!strcmp(payload_copy,"WIFI:T:WPA;S:A\\;B;P:password;;"));
 assert(!ui_wifi_qr_prepare("", ""));
 puts("PASS both AP backends: dpReader31/open/empty password/single client; actual QR nopass and escaped WPA payloads");}
'''
with tempfile.TemporaryDirectory() as directory:
 p=Path(directory);(p/'test.c').write_text(unit)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
