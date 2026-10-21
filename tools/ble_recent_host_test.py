"""实际BLE最近设备排序、持久化与快捷连接队列回归。
Real BLE recent-peer persistence and quick-connect queue regression.
SPDX-License-Identifier: Apache-2.0
"""
from pathlib import Path
import re,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
source=(root/'components/ble_page_turner/src/ble_page_turner.c').read_text()
def function(name):
 start=re.search(r'^(?:static )?[^\n]+\b'+name+r'\([^;{}]*\)\s*\{',source,re.M);assert start,name
 at=start.end();depth=1
 while depth:
  depth+=(source[at]=='{')-(source[at]=='}');at+=1
 return source[start.start():at]+'\n'
unit=r'''
#include <assert.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "ble_page_turner.h"
#define NVS_NS_BONDS "bonds"
#define NVS_NS_BINDINGS "bindings"
#define NVS_READONLY 0
#define NVS_READWRITE 1
typedef int nvs_handle_t;
static ble_pt_bond_t s_bonds[BLE_PT_MAX_BONDS],disk[BLE_PT_MAX_BONDS];
static bool on_disk[BLE_PT_MAX_BONDS];
static uint8_t s_bond_count;
static atomic_bool s_recent_requested;
static bool s_link_up,s_connecting;
static char s_reconnect_addr[18],s_pending_addr[18];
static uint32_t s_reconnect_at,s_retry_after_ms;
static char s_start_failure[160],s_failure[128];
static int commits,handles,connects,connect_error,boot_tries;
static char connected_addr[18];
static int64_t now_us=123000;
static int64_t esp_timer_get_time(void){return now_us;}
static void boot_tries_store(unsigned value){boot_tries=(int)value;}
static void why_store(const char *why){(void)why;}
static esp_err_t nvs_open(const char *ns,int mode,nvs_handle_t *h){assert(mode==NVS_READONLY||mode==NVS_READWRITE);*h=!strcmp(ns,NVS_NS_BONDS)?1:2;++handles;return ESP_OK;}
static int key_index(const char *key){int i=-1;assert(sscanf(key,"d_%d",&i)==1&&i>=0&&i<BLE_PT_MAX_BONDS);return i;}
static esp_err_t nvs_get_blob(nvs_handle_t h,const char *key,void *value,size_t *len){assert(h==1&&*len==sizeof(ble_pt_bond_t));int i=key_index(key);if(!on_disk[i])return ESP_FAIL;memcpy(value,&disk[i],*len);return ESP_OK;}
static esp_err_t nvs_set_blob(nvs_handle_t h,const char *key,const void *value,size_t len){assert(h==1&&len==sizeof(ble_pt_bond_t));int i=key_index(key);disk[i]=*(const ble_pt_bond_t*)value;on_disk[i]=true;return ESP_OK;}
static esp_err_t nvs_erase_key(nvs_handle_t h,const char *key){if(h==1)on_disk[key_index(key)]=false;else assert(!strcmp(key,"binding"));return ESP_OK;}
static esp_err_t nvs_commit(nvs_handle_t h){assert(h==1||h==2);++commits;return ESP_OK;}
static void nvs_close(nvs_handle_t h){assert(h==1||h==2);--handles;}
static const char *bond_key(char *out,size_t cap,const char *addr){assert(addr[0]);snprintf(out,cap,"binding");return out;}
esp_err_t ble_pt_connect(const char *addr){++connects;snprintf(connected_addr,sizeof(connected_addr),"%s",addr);return connect_error;}
'''
for name in ['bonds_load','bonds_save','bond_find','bond_add','ble_pt_reconnect_recent','ble_pt_cancel_recent','reconnect_recent_requested','ble_pt_reset_failure','ble_pt_forget']:
 unit+=function(name)
unit+=r'''
int main(void){
 const char *a="01:02:03:04:05:06",*b="11:12:13:14:15:16",*c="21:22:23:24:25:26";
 ble_pt_reconnect_recent();assert(s_recent_requested&&!connects);
 reconnect_recent_requested();assert(!connects&&!s_recent_requested);
 bond_add(a,"A",0);bond_add(b,"B",1);bond_add(c,"C",0);assert(s_bond_count==3&&commits==3&&!handles);
 // 已存在的设备成功重连应成为最近设备，重复通知不能反复写NVS。
 // An existing peer's successful reconnection makes it most recent; duplicate reports do not rewrite NVS.
 bond_add(a,"A",0);assert(commits==4&&!strcmp(s_bonds[2].addr,a));
 for(int n=0;n<100;++n)bond_add(a,"A",0);assert(commits==4);
 memset(s_bonds,0,sizeof(s_bonds));s_bond_count=0;bonds_load();assert(s_bond_count==3&&!strcmp(s_bonds[2].addr,a)&&!handles);
 ble_pt_reconnect_recent();ble_pt_reconnect_recent();assert(!connects);
 reconnect_recent_requested();assert(connects==1&&!strcmp(connected_addr,a)&&!strcmp(s_reconnect_addr,a)&&s_reconnect_at==123);
 reconnect_recent_requested();assert(connects==1);
 s_link_up=true;ble_pt_reconnect_recent();reconnect_recent_requested();assert(connects==1);s_link_up=false;
 s_connecting=true;ble_pt_reconnect_recent();reconnect_recent_requested();assert(connects==1);s_connecting=false;
 // 名字和地址类型更新后也仍能持久化，不清除其他设备。
 // Persist name/type changes without dropping other peers.
 bond_add(b,"B new",0);assert(s_bond_count==3&&!strcmp(s_bonds[2].addr,b)&&!strcmp(s_bonds[2].name,"B new")&&!s_bonds[2].addr_type);
 snprintf(s_reconnect_addr,sizeof(s_reconnect_addr),"%s",b);snprintf(s_pending_addr,sizeof(s_pending_addr),"%s",b);
 ble_pt_forget(b);assert(s_bond_count==2&&!s_reconnect_addr[0]&&!s_pending_addr[0]&&!handles);
 ble_pt_reconnect_recent();reconnect_recent_requested();assert(connects==2&&!strcmp(connected_addr,a));
 // 主动关闭取消排队；失败只尝试一次，后续重试仍走原4秒退避。
 // Explicit off cancels requests; failures start once and retain the original four-second retry policy.
 ble_pt_reconnect_recent();boot_tries=2;ble_pt_cancel_recent();reconnect_recent_requested();assert(connects==2&&boot_tries==2);ble_pt_reset_failure();assert(!boot_tries);
 connect_error=ESP_FAIL;ble_pt_reconnect_recent();reconnect_recent_requested();assert(connects==3&&!strcmp(s_reconnect_addr,a));
 reconnect_recent_requested();assert(connects==3);
 puts("PASS: recent successful peer ordering/reload, no repeated NVS writes, coalesced asynchronous quick-connect, existing-link protection, explicit-off cancellation and forgotten-target removal");
}
'''
poll=function('poll_locked');assert poll.index('if (!s_running) return;')<poll.index('reconnect_recent_requested();')
assert 'BLE_PT_RECONNECT_MS' in poll and '!s_scanning' in poll
with tempfile.TemporaryDirectory(prefix='kiiko-ble-recent-') as tmp:
 p=Path(tmp);(p/'esp_err.h').write_text('#pragma once\ntypedef int esp_err_t;\nenum {ESP_OK,ESP_FAIL};\n');(p/'test.c').write_text(unit)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-fsanitize=address,undefined','-I'+tmp,'-I'+str(root/'components/ble_page_turner/include'),str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
