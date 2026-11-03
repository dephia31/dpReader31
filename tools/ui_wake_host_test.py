#!/usr/bin/env python3
"""Run the actual static UI wake latch against pthread producers and wait races."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
unit=r'''
#include <assert.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include "metalio_ui_wake.h"
#include "freertos/semphr.h"
static void (*before_wait)(void);
static int last_ticks;
SemaphoreHandle_t xSemaphoreCreateBinaryStatic(StaticSemaphore_t*s){pthread_mutex_init(&s->mutex,NULL);pthread_cond_init(&s->cond,NULL);s->token=false;return s;}
int xSemaphoreGive(SemaphoreHandle_t s){pthread_mutex_lock(&s->mutex);s->token=true;pthread_cond_signal(&s->cond);pthread_mutex_unlock(&s->mutex);return 1;}
int xSemaphoreTake(SemaphoreHandle_t s,int ticks){last_ticks=ticks;if(before_wait){void(*f)(void)=before_wait;before_wait=NULL;f();}
 struct timespec until;clock_gettime(CLOCK_REALTIME,&until);until.tv_sec+=ticks/1000;until.tv_nsec+=(ticks%1000)*1000000L;if(until.tv_nsec>=1000000000){until.tv_nsec-=1000000000;until.tv_sec++;}
 pthread_mutex_lock(&s->mutex);int rc=0;while(!s->token&&!rc)rc=pthread_cond_timedwait(&s->cond,&s->mutex,&until);
 bool got=s->token;s->token=false;pthread_mutex_unlock(&s->mutex);return got;
}
#include "metalio_ui_wake.c"
static void post_at_wait(void){metalio_ui_wake_post(METALIO_UI_WAKE_BLE);}
static pthread_mutex_t payload_lock=PTHREAD_MUTEX_INITIALIZER;
static unsigned payload[4],completed;
static void *producer(void *arg){unsigned index=(unsigned)(uintptr_t)arg;
 for(unsigned i=0;i<2000;i++){
  pthread_mutex_lock(&payload_lock);++payload[index];pthread_mutex_unlock(&payload_lock);
  metalio_ui_wake_post(1u<<index);
 }
 pthread_mutex_lock(&payload_lock);++completed;pthread_mutex_unlock(&payload_lock);metalio_ui_wake_post(1u<<index);return NULL;
}
int main(void){
 metalio_ui_wake_post(METALIO_UI_WAKE_TOUCH);metalio_ui_wake_init();metalio_ui_wake_init();
 assert(metalio_ui_wake_wait(40)==METALIO_UI_WAKE_TOUCH&&last_ticks==40);
 assert(!metalio_ui_wake_wait(0));
 metalio_ui_wake_post(METALIO_UI_WAKE_KEY);metalio_ui_wake_post(METALIO_UI_WAKE_BLE);metalio_ui_wake_post(METALIO_UI_WAKE_KEY);
 assert(metalio_ui_wake_wait(8)==(METALIO_UI_WAKE_KEY|METALIO_UI_WAKE_BLE));assert(!metalio_ui_wake_wait(0));
 before_wait=post_at_wait;assert(metalio_ui_wake_wait(200)==METALIO_UI_WAKE_BLE&&last_ticks==200);
 // Notifications coalesce wakeups only; real payload remains separately counted.
 pthread_t threads[4];for(unsigned i=0;i<4;i++)assert(!pthread_create(threads+i,NULL,producer,(void*)(uintptr_t)i));
 unsigned received=0;
 while(received<8000){(void)metalio_ui_wake_wait(40);metalio_ui_wake_iteration();pthread_mutex_lock(&payload_lock);for(int i=0;i<4;i++){received+=payload[i];payload[i]=0;}pthread_mutex_unlock(&payload_lock);}
 for(int i=0;i<4;i++)pthread_join(threads[i],NULL);
 assert(completed==4);(void)metalio_ui_wake_wait(0);assert(!metalio_ui_wake_wait(0));
 xSemaphoreGive(wake_signal);assert(!metalio_ui_wake_wait(0));
 metalio_ui_wake_stats_t s=metalio_ui_wake_stats();assert(s.posts==8009&&s.iterations&&s.event_wakes&&s.timeouts&&s.empty_wakes==1&&(s.reasons&15)==15);
 puts("PASS actual wake latch: early events, coalescing, check-to-block race, zero-timeout,8000 payloads across4 concurrent producers, no heap or task notification reuse");
}
'''
with tempfile.TemporaryDirectory() as t:
 p=Path(t);(p/'freertos').mkdir()
 (p/'freertos/FreeRTOS.h').write_text('''#pragma once
#include <pthread.h>
#include <stdbool.h>
#include <assert.h>
typedef pthread_mutex_t portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED PTHREAD_MUTEX_INITIALIZER
#define portENTER_CRITICAL(p) pthread_mutex_lock(p)
#define portEXIT_CRITICAL(p) pthread_mutex_unlock(p)
#define configASSERT(x) assert(x)
#define pdMS_TO_TICKS(x) (x)
typedef int TickType_t,BaseType_t;
''')
 (p/'freertos/semphr.h').write_text('''#pragma once
#include "FreeRTOS.h"
typedef struct {pthread_mutex_t mutex;pthread_cond_t cond;bool token;}StaticSemaphore_t;
typedef StaticSemaphore_t *SemaphoreHandle_t;
SemaphoreHandle_t xSemaphoreCreateBinaryStatic(StaticSemaphore_t*);int xSemaphoreGive(SemaphoreHandle_t);int xSemaphoreTake(SemaphoreHandle_t,int);
''')
 (p/'freertos/task.h').write_text('#pragma once\n')
 (p/'test.c').write_text(unit)
 subprocess.run(['cc','-std=c11','-D_POSIX_C_SOURCE=200809L','-Wall','-Wextra','-Werror','-fsanitize=undefined','-pthread','-I'+str(p),'-I'+str(root/'components/metalio_hw/include'),'-I'+str(root/'components/metalio_hw'),str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
