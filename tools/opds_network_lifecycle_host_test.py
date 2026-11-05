#!/usr/bin/env python3
"""Actual OPDS cold/reused network cleanup, timeout and server ownership."""
from pathlib import Path
import subprocess,tempfile
from home_cover_cache_host_test import function
ROOT=Path(__file__).resolve().parents[1]
source=ROOT/'main/book/opds_client.c'
unit=r'''
#include <assert.h>
#define read_pico_transfer_prepare_network() ((void)0)
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
enum{ESP_OK,ESP_ERR_NOT_FOUND,ESP_ERR_INVALID_STATE,ESP_ERR_TIMEOUT,ESP_ERR_NO_MEM};
enum{READ_PICO_TRANSFER_STOPPED,READ_PICO_TRANSFER_UPLOADING,READ_PICO_TRANSFER_ERROR};
enum{READ_PICO_TRANSFER_MODE_STA,READ_PICO_TRANSFER_MODE_AP};
#define pdMS_TO_TICKS(n) (n)
typedef int esp_err_t;
typedef struct{int state,mode;bool network_ready;}read_pico_transfer_status_t;
static read_pico_transfer_status_t status;
static bool saved=true,shareable=true,s_session_active,s_session_owns_wifi;
static int error,starts,stops,observed;
static int64_t now,ready_at;
static void *s_network_context;
static void observer(void *p){(void)p;++observed;}
static void(*s_network_observer)(void*)=observer;
static bool read_pico_transfer_network_shareable(void){return shareable;}
static void read_pico_transfer_get_status(read_pico_transfer_status_t *s){*s=status;}
static int read_pico_transfer_get_saved_wifi(char *ssid,bool *configured){strcpy(ssid,"saved");*configured=saved;return ESP_OK;}
static int read_pico_transfer_preempt_for_network(void){++starts;return error;}
static const char *esp_err_to_name(int e){(void)e;return "error";}
static int64_t esp_timer_get_time(void){return now;}
static void vTaskDelay(int ms){now+=(int64_t)ms*1000;}
static void read_pico_transfer_service_poll(void){if(ready_at>=0&&now>=ready_at)status.network_ready=true;}
static int read_pico_transfer_sync_time_online(uint32_t *t){(void)t;return ESP_OK;}
static void read_pico_transfer_stop(void){++stops;status.network_ready=false;status.state=READ_PICO_TRANSFER_STOPPED;}
static void read_pico_transfer_network_request_stop(void){read_pico_transfer_stop();}
'''+function('opds_session_begin',source)+'\n'+function('opds_session_end',source)+'\n'+function('opds_session_active',source)+'\n'+function('opds_acquire_network',source)+'\n'+function('opds_release_network',source)+r'''
static void reset(void){status=(read_pico_transfer_status_t){.mode=READ_PICO_TRANSFER_MODE_STA};saved=shareable=true;s_session_active=s_session_owns_wifi=false;starts=stops=error=observed=0;now=0;ready_at=0;}
int main(void){bool owned;char message[128];
 reset();status.network_ready=true;assert(opds_acquire_network(&owned,message,sizeof(message))==ESP_OK&&!owned);opds_release_network(owned);assert(!starts&&!stops);
 reset();assert(opds_acquire_network(&owned,message,sizeof(message))==ESP_OK&&owned&&observed==1);opds_release_network(owned);assert(starts==1&&stops==1);
 reset();ready_at=-1;assert(opds_acquire_network(&owned,message,sizeof(message))==ESP_ERR_TIMEOUT&&!owned);opds_release_network(owned);assert(starts==1&&stops==1);
 reset();saved=false;assert(opds_acquire_network(&owned,message,sizeof(message))==ESP_ERR_NOT_FOUND&&!owned&&!starts&&!stops);
 reset();error=ESP_ERR_NO_MEM;assert(opds_acquire_network(&owned,message,sizeof(message))==ESP_ERR_NO_MEM&&!owned&&!stops);
 reset();shareable=false;status.network_ready=true;assert(opds_acquire_network(&owned,message,sizeof(message))==ESP_ERR_INVALID_STATE&&!owned&&!starts&&!stops);
 reset();opds_session_begin();assert(opds_session_active());
 assert(opds_acquire_network(&owned,message,sizeof(message))==ESP_OK&&owned);opds_release_network(owned);assert(!stops&&s_session_owns_wifi);
 assert(opds_acquire_network(&owned,message,sizeof(message))==ESP_OK&&!owned);opds_release_network(owned);assert(!stops&&starts==1);
 opds_session_end();assert(stops==1&&!opds_session_active());opds_session_end();assert(stops==1);
 reset();status.network_ready=true;opds_session_begin();assert(opds_acquire_network(&owned,message,sizeof(message))==ESP_OK&&!owned);opds_release_network(owned);opds_session_end();assert(!stops);
 puts("PASS actual OPDS lifecycle: browse session retains self-started radio across requests until close; one-shot/timeout cleanup and borrowed manual connection retained, upload-server reuse rejected, missing credentials/OOM unwind, connecting observer fired");
}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp);(p/'test.c').write_text(unit)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)

ui=ROOT/'main/apps/app_book.c'
assert 'opds_session_begin();' in function('opds_open_catalog',ui)
assert 'opds_session_end();' in function('book_on_exit',ui)
assert 'if(s_view!=OPDS && opds_session_active())opds_session_end();' in function('on_tick',ui)
print('PASS OPDS UI session begin, page-exit cleanup and same-page view transition cleanup')
