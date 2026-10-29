#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
"""中文：执行S31网络/BLE租约，验证互斥、失败回滚和最近设备恢复。
English: Execute S31 network/BLE leases with exclusion, failure rollback and peer restoration.
"""
from pathlib import Path
import os
import subprocess,tempfile
from home_cover_cache_host_test import function
root=Path(__file__).resolve().parents[1];source=root/'components/ble_page_turner/src/ble_page_turner.c'
unit=r'''
#include <assert.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_TIMEOUT -2
typedef int esp_err_t;
static atomic_uint s_network_users;
static atomic_bool s_sleep_suspended;
static char s_network_peer[18],s_reconnect_addr[18],s_conn_addr[18];
static bool s_network_peer_pending,s_link_up,running,locked,lock_fails,stop_fails;
static unsigned stops,attempts;
static bool lifecycle_take(unsigned ms){assert(ms==2000&&!locked);++attempts;if(lock_fails)return false;locked=true;return true;}
static void lifecycle_give(void){assert(locked);locked=false;}
static esp_err_t stop_locked(void){assert(locked&&atomic_load(&s_network_users));++stops;if(stop_fails)return ESP_FAIL;running=false;s_link_up=false;s_reconnect_addr[0]=0;return ESP_OK;}
'''
for name in ['metalio_radio_reserved','network_resume_peer','ble_pt_network_acquire','ble_pt_network_release']:unit+=function(name,source)+'\n'
unit+=r'''
static void reset(void){atomic_store(&s_network_users,0);atomic_store(&s_sleep_suspended,false);s_network_peer_pending=s_link_up=locked=lock_fails=stop_fails=false;running=true;stops=attempts=0;s_reconnect_addr[0]=s_network_peer[0]=s_conn_addr[0]=0;}
int main(void){
 reset();lock_fails=true;assert(ble_pt_network_acquire()==ESP_ERR_TIMEOUT&&!atomic_load(&s_network_users)&&running&&!stops);lock_fails=false;
 stop_fails=true;strcpy(s_reconnect_addr,"11:22:33:44:55:66");assert(ble_pt_network_acquire()==ESP_FAIL&&!atomic_load(&s_network_users)&&running&&!locked);stop_fails=false;
 assert(ble_pt_network_acquire()==ESP_OK&&!running&&atomic_load(&s_network_users)==1&&metalio_radio_reserved());
 assert(ble_pt_network_acquire()==ESP_OK&&atomic_load(&s_network_users)==2);
 network_resume_peer();assert(!s_reconnect_addr[0]);
 ble_pt_network_release();assert(metalio_radio_reserved());ble_pt_network_release();ble_pt_network_release();assert(!metalio_radio_reserved());
 network_resume_peer();assert(!strcmp(s_reconnect_addr,"11:22:33:44:55:66")&&!s_network_peer_pending);
 for(unsigned peer=0;peer<2;++peer){reset();s_link_up=peer;strcpy(s_conn_addr,"AA:BB:CC:DD:EE:FF");strcpy(s_reconnect_addr,"11:22:33:44:55:66");
  assert(ble_pt_network_acquire()==ESP_OK);assert(!running&&metalio_radio_reserved());
  atomic_store(&s_sleep_suspended,true);ble_pt_network_release();assert(metalio_radio_reserved());
  atomic_store(&s_sleep_suspended,false);assert(!metalio_radio_reserved());network_resume_peer();
  assert(!strcmp(s_reconnect_addr,peer?"AA:BB:CC:DD:EE:FF":"11:22:33:44:55:66"));
 }
 reset();assert(ble_pt_network_acquire()==ESP_OK);ble_pt_network_release();strcpy(s_reconnect_addr,"22:33:44:55:66:77");network_resume_peer();assert(!strcmp(s_reconnect_addr,"22:33:44:55:66:77"));
 puts("PASS: actual S31 network lease stops BLE under lifecycle lock,nested ownership blocks restart,lock/stop failures roll back,balanced/double release,sleep overlap and saved/connected peer restoration");
}
'''
sanitizers=[] if os.environ.get('HOST_TEST_SANITIZERS')=='0' else ['-fsanitize=address,undefined']
with tempfile.TemporaryDirectory(prefix='s31-radio-lease-') as folder:
 p=Path(folder);(p/'test.c').write_text(unit)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror',*sanitizers,str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
