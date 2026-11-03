#!/usr/bin/env python3
"""Real network guard: nested calls, cross-task refusal, deferred stop and all returns."""
from pathlib import Path
import subprocess,tempfile
ROOT=Path(__file__).resolve().parents[1]
unit=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdatomic.h>
#include <pthread.h>
#include "transfer_network_activity.h"
static _Thread_local uintptr_t task_id=1;
static atomic_int stops;
static atomic_bool work_finished;
void read_pico_transfer_stop(void){
 transfer_network_guard_t guard __attribute__((cleanup(transfer_network_guard_end)))={read_pico_transfer_network_begin()};
 if(!guard.held){read_pico_transfer_network_request_stop();return;}
 assert(atomic_load(&work_finished));atomic_fetch_add(&stops,1);
}
static void *other_task(void *arg){(void)arg;task_id=2;
 assert(read_pico_transfer_network_busy()&&read_pico_transfer_network_background_busy());assert(!read_pico_transfer_network_begin());
 read_pico_transfer_network_end();assert(read_pico_transfer_network_busy());
 read_pico_transfer_stop();assert(!atomic_load(&stops));return NULL;
}
static int failure(void){TRANSFER_NETWORK_GUARD_OR_RETURN(-1);return -7;}
int main(void){
 assert(!read_pico_transfer_network_busy());
 for(int i=0;i<200;++i){
  atomic_store(&work_finished,false);atomic_store(&stops,0);
  uint32_t before=read_pico_transfer_network_generation();
  assert(read_pico_transfer_network_begin());assert(read_pico_transfer_network_generation()==before+1);
  assert(read_pico_transfer_network_begin());assert(read_pico_transfer_network_generation()==before+1);assert(!read_pico_transfer_network_background_busy());
  pthread_t thread;assert(!pthread_create(&thread,NULL,other_task,NULL));assert(!pthread_join(thread,NULL));
  assert(failure()==-7);assert(read_pico_transfer_network_busy());
  read_pico_transfer_network_end();assert(!atomic_load(&stops));
  atomic_store(&work_finished,true);read_pico_transfer_network_end();
  assert(atomic_load(&stops)==1&&!read_pico_transfer_network_busy());
  assert(read_pico_transfer_network_generation()==before+2);
 }
 assert(failure()==-7&&!read_pico_transfer_network_busy());
 read_pico_transfer_network_request_stop();assert(atomic_load(&stops)==2&&!read_pico_transfer_network_busy());
 puts("PASS actual network guard: 200 cross-task sessions; nested requests, foreign stop/release refusal, deferred cleanup, early-return and free-radio stop");
}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp);(p/'freertos').mkdir()
 (p/'freertos/FreeRTOS.h').write_text('''#pragma once
#include <pthread.h>
#include <stdint.h>
typedef pthread_mutex_t portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED PTHREAD_MUTEX_INITIALIZER
#define portENTER_CRITICAL(p) pthread_mutex_lock(p)
#define portEXIT_CRITICAL(p) pthread_mutex_unlock(p)
''')
 (p/'freertos/task.h').write_text('''#pragma once
#include <stdint.h>
typedef void *TaskHandle_t;
TaskHandle_t xTaskGetCurrentTaskHandle(void);
''')
 (p/'test.c').write_text(unit+'\nvoid *xTaskGetCurrentTaskHandle(void){return (void*)task_id;}\n')
 exe=p/'test'
 subprocess.run(['cc','-std=gnu11','-Wall','-Wextra','-Werror','-fsanitize=undefined','-pthread','-I'+str(p),'-I'+str(ROOT/'components/read_pico_transfer/include'),str(p/'test.c'),str(ROOT/'components/read_pico_transfer/transfer_network_activity.c'),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
# Every owner keeps the guard until its own transport cleanup, not merely until connect.
import re
def function(name,path):
 text=path.read_text()
 scan=re.sub(r"//[^\n]*|/\*[\s\S]*?\*/|'(?:\\.|[^'\\])*'|\"(?:\\.|[^\"\\])*\"",lambda m:' '*len(m[0]),text)
 start=re.search(r'^(?:static )?(?:[A-Za-z_]\w*(?:\s+|\s*\*\s*))+'+name+r'\([^;{}]*?\)\s*\{',scan,re.M)
 assert start,name
 at=start.end();depth=1
 while depth:depth+=(scan[at]=='{')-(scan[at]=='}');at+=1
 return text[start.start():at]

for path,names in [('main/book/opds_client.c',['opds_fetch_feed','opds_download_book']),('main/book/kosync_client.c',['kosync_request','kosync_sync_book_impl'])]:
 for name in names:
  code=function(name,ROOT/path);assert 'TRANSFER_NETWORK_GUARD_OR_RETURN' in code or 'read_pico_transfer_network_begin()' in code
for path in ['main/font/online_font.c','boards/metalio47/src/main/ota_online.c']:
 worker=function('worker',ROOT/path);assert worker.index('read_pico_transfer_network_begin()')<worker.index('if (owned) read_pico_transfer_stop();' if 'ota_online' in path else 'if(owned)read_pico_transfer_stop();')<worker.index('read_pico_transfer_network_end();')
transfer=ROOT/'components/read_pico_transfer/read_pico_transfer.c'
for name in ['read_pico_transfer_start','read_pico_transfer_try_stop_if_idle','sync_time_run','read_pico_transfer_scan_wifi']:
 assert 'TRANSFER_NETWORK_GUARD_OR_RETURN' in function(name,transfer)
assert 'read_pico_transfer_network_request_stop()' in function('read_pico_transfer_stop',transfer)
print('PASS transport wiring: OPDS, KoSync, OTA, fonts, NTP, scans and start/stop admission')
