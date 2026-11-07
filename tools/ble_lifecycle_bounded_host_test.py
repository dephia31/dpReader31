#!/usr/bin/env python3
"""Execute actual lifecycle code with a controllable SDK stop/host exit scheduler."""
from pathlib import Path
import tempfile,subprocess
from home_cover_cache_host_test import function
root=Path(__file__).resolve().parents[1];source=root/'components/ble_page_turner/src/ble_page_turner.c'
unit=r'''
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <string.h>
#include <stdio.h>
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_TIMEOUT -2
#define ESP_ERR_NO_MEM -3
#define pdPASS 1
#define pdMS_TO_TICKS(x) (x)
#define configMAX_PRIORITIES 25
#define CONFIG_BT_NIMBLE_PINNED_TO_CORE 0
#define BLE_HS_CONN_HANDLE_NONE 65535
#define ESP_LOGE(...) ((void)0)
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGW(tag,...) do{if(0)printf(__VA_ARGS__);}while(0)
#define portENTER_CRITICAL(x) ((void)(x))
#define portEXIT_CRITICAL(x) ((void)(x))
typedef int esp_err_t;
static atomic_bool s_host_ready,s_host_exited,s_stop_active,s_stop_done;
static atomic_flag s_lifecycle=ATOMIC_FLAG_INIT;
static atomic_int s_stop_result;static uint8_t s_own_addr_type;static bool s_stop_timeout_logged;
static void *s_host_task=(void*)1;
static uint32_t s_stop_budget_ms=2000;
static bool s_running=true,s_connected=true,s_connecting=true,s_scanning=true,s_link_up=true;
enum { STEP_IDLE };
static int s_conn_handle,s_stable_since,s_held_usage,s_ring_lock,s_keys,s_raws,s_step,s_scan_cancel_ms;
static char s_reconnect_addr[18],s_pending_addr[18];
static int64_t now_us;static int sdk_stop_calls,deinits,stop_error,deinit_error,create_fail,creates;
static int host_exit_ticks,ensure_error,infer_error;static bool complete_during_wait,worker_running;
static void (*worker)(void *);
static int64_t esp_timer_get_time(void){return now_us;}
static void vTaskDelete(void *p){assert(!p);}
static void vTaskDelay(unsigned ticks){now_us+=ticks*1000;if(worker_running&&host_exit_ticks>0&&!--host_exit_ticks)atomic_store(&s_host_exited,true);if(complete_during_wait&&!worker_running&&worker){void(*fn)(void*)=worker;worker=NULL;worker_running=true;fn(NULL);worker_running=false;}}
static int nimble_port_stop(void){++sdk_stop_calls;return stop_error;}
static int nimble_port_deinit(void){assert(atomic_load(&s_host_exited));++deinits;return deinit_error;}
static int xTaskCreatePinnedToCore(void(*fn)(void*),const char*n,unsigned stack,void*arg,int prio,void*out,int core){assert(!strcmp(n,"ble_stop")&&stack==4096&&!arg&&prio==21&&!out&&!core);++creates;if(create_fail)return 0;worker=fn;return pdPASS;}

static int ble_hs_util_ensure_addr(int x){assert(!x);return ensure_error;}
static int ble_hs_id_infer_auto(int x,uint8_t *out){assert(!x);*out=1;return infer_error;}
'''
# Match production address storage type.

for name in ['lifecycle_take','lifecycle_give','on_sync','on_host_reset','stop_worker','stop_locked','ble_pt_stop','ble_pt_connected','ble_pt_connecting','ble_pt_stop_failed']:
 unit+=function(name,source)+'\n'
sync=source.read_text().split('void ble_pt_sync(bool wanted) {',1)[1].split('bool ble_pt_take_start_failure',1)[0]
assert 'if (!wanted && s_running) (void)ble_pt_stop(0);' in sync and sync.count('ble_pt_stop(')==1
unit+=r'''
static void reset(void){atomic_flag_clear(&s_lifecycle);s_running=s_connected=s_connecting=s_scanning=s_link_up=true;s_host_task=(void*)1;atomic_store(&s_host_ready,true);atomic_store(&s_host_exited,false);atomic_store(&s_stop_active,false);atomic_store(&s_stop_done,false);s_stop_budget_ms=2000;now_us=0;sdk_stop_calls=deinits=stop_error=deinit_error=create_fail=creates=host_exit_ticks=ensure_error=infer_error=0;complete_during_wait=worker_running=false;worker=NULL;}
static void finish_worker(void){assert(worker);worker_running=true;worker(NULL);worker_running=false;worker=NULL;}
int main(void){
 reset();assert(ble_pt_connected());ensure_error=1;on_sync();assert(!atomic_load(&s_host_ready));ensure_error=0;infer_error=1;on_sync();assert(!atomic_load(&s_host_ready));infer_error=0;on_sync();assert(atomic_load(&s_host_ready)&&s_own_addr_type==1);on_host_reset(5);assert(!ble_pt_connected());
 reset();atomic_flag_test_and_set(&s_lifecycle);assert(ble_pt_stop(1)==ESP_ERR_TIMEOUT&&now_us==1000&&creates==0);atomic_flag_clear(&s_lifecycle);
 reset();strcpy(s_pending_addr,"peer");strcpy(s_reconnect_addr,"old");s_keys=s_raws=1;
 assert(ble_pt_stop(0)==ESP_ERR_TIMEOUT&&now_us==0&&creates==1&&s_running&&!ble_pt_connected()&&!ble_pt_stop_failed());
 assert(ble_pt_stop(0)==ESP_ERR_TIMEOUT&&now_us==0&&creates==1);
 host_exit_ticks=2;finish_worker();assert(ble_pt_stop(0)==ESP_OK&&!s_running&&deinits==1);
 assert(!s_pending_addr[0]&&!s_reconnect_addr[0]&&!s_keys&&!s_raws&&!ble_pt_stop_failed());
 reset();assert(ble_pt_stop(25)==ESP_ERR_TIMEOUT&&now_us<=26000&&creates==1&&deinits==0&&s_running&&atomic_load(&s_stop_active)&&!ble_pt_connected());
 assert(ble_pt_stop(0)==ESP_ERR_TIMEOUT&&creates==1&&!deinits);
 on_sync();assert(!atomic_load(&s_host_ready));host_exit_ticks=4;finish_worker();assert(deinits==1&&sdk_stop_calls==1&&atomic_load(&s_stop_done));assert(stop_locked()==ESP_OK&&!s_running&&!atomic_load(&s_stop_active)&&s_host_task==NULL);
 reset();create_fail=1;assert(stop_locked()==ESP_ERR_NO_MEM&&!atomic_load(&s_stop_active)&&!deinits);
 reset();assert(ble_pt_stop(1)==ESP_ERR_TIMEOUT);stop_error=9;finish_worker();assert(stop_locked()==ESP_FAIL&&s_running&&atomic_load(&s_stop_active)&&!deinits&&ble_pt_stop_failed());
 reset();complete_during_wait=true;host_exit_ticks=2;assert(ble_pt_stop(100)==ESP_OK&&!s_running&&deinits==1&&sdk_stop_calls==1&&now_us<=100000);
 reset();s_running=false;assert(stop_locked()==ESP_OK&&creates==0);
 puts("PASS bounded BLE stop: caller deadline, no duplicate workers, host exit before deinit, delayed completion, allocation/SDK failures, quarantine, identity readiness and state confirmation");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(unit);subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(p/'test.c'),'-o',str(p/'test')],check=True);subprocess.run([str(p/'test')],check=True)
