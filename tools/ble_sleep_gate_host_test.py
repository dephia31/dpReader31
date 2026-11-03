from pathlib import Path
import subprocess,tempfile
from home_cover_cache_host_test import function
r=Path(__file__).resolve().parents[1];ble=r/'components/ble_page_turner/src/ble_page_turner.c'
u=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
enum{ESP_OK,ESP_ERR_TIMEOUT};typedef int esp_err_t;
#define ESP_LOGI(...) ((void)0)
static atomic_bool s_sleep_suspended;static bool s_link_up,s_running;
static char s_sleep_peer[18],s_conn_addr[18],s_reconnect_addr[18];
static uint32_t s_stop_budget_ms=2000;static int result,calls,lock_budget,stop_budget;
static bool lifecycle_take(int ms){lock_budget=ms;return true;}static void lifecycle_give(void){}
static int stop_locked(void){++calls;stop_budget=s_stop_budget_ms;if(!result)s_running=false;return result;}
'''+function('ble_pt_suspend_for_sleep',ble)+'\n'+function('ble_pt_resume_after_sleep',ble)+r'''
int main(void){s_link_up=s_running=true;strcpy(s_conn_addr,"AA");result=ESP_ERR_TIMEOUT;
 assert(ble_pt_suspend_for_sleep()==ESP_ERR_TIMEOUT&&s_sleep_suspended&&s_running&&calls==1&&lock_budget==2000&&stop_budget==2000);
 strcpy(s_conn_addr,"BB");assert(ble_pt_suspend_for_sleep()==ESP_ERR_TIMEOUT&&calls==2&&lock_budget==0&&stop_budget==0&&!strcmp(s_sleep_peer,"AA")&&s_stop_budget_ms==2000);
 result=0;assert(ble_pt_suspend_for_sleep()==ESP_OK&&!s_running&&s_sleep_suspended&&calls==3);
 assert(ble_pt_suspend_for_sleep()==ESP_OK&&calls==4&&s_sleep_suspended);
 ble_pt_resume_after_sleep();assert(!s_sleep_suspended&&!strcmp(s_reconnect_addr,"AA"));
 puts("PASS BLE Sleep gate: stop timeout blocks restart, repeated calls retry without waiting, original peer preserved, success confirms host stopped, only unlock resume clears gate");}
'''
sleep=(r/'boards/metalio47/src/main/sleep.c').read_text();wait=function('app_light_sleep_wait_timed',r/'boards/metalio47/src/main/sleep.c');assert 'bool ble_stopped=ble_pt_suspend_for_sleep()==ESP_OK;' in wait and 'if (wifi_stopped && ble_stopped &&' in wait
assert 's_sleep_suspended' in function('metalio_radio_reserved',ble)
for name in ['ble_pt_connect','ble_pt_scan_start']:assert 'metalio_radio_reserved()' in function(name,ble)
assert 'if(atomic_load(&s_sleep_suspended))return;' in function('poll_locked',ble)
with tempfile.TemporaryDirectory() as t:
 p=Path(t);(p/'test.c').write_text(u);subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(p/'test.c'),'-o',str(p/'test')],check=True);subprocess.run([str(p/'test')],check=True)
