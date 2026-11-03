#!/usr/bin/env python3
"""Stress actual CPU policy + wake latch with real mutexes and four workers."""
from pathlib import Path
import runpy,subprocess,tempfile
r=Path(__file__).resolve().parents[1]
base=runpy.run_path(str(r/'tools/cpu_policy_host_test.py'))
u=base['u']
u='#include <pthread.h>\n#include <stdatomic.h>\n'+u
u=u.replace('static int count,minimum=320,maximum=320,seen_signal_mhz;','static atomic_int count;static int minimum=320,maximum=320,seen_signal_mhz;')
a=u.index('SemaphoreHandle_t xSemaphoreCreateMutexStatic(');b=u.index('int esp_reset_reason(void)',a)
u=u[:a]+r'''
SemaphoreHandle_t xSemaphoreCreateMutexStatic(StaticSemaphore_t*s){s->kind=1;s->held=false;pthread_mutex_init(&s->native,NULL);return s;}
SemaphoreHandle_t xSemaphoreCreateBinaryStatic(StaticSemaphore_t*s){s->kind=2;s->token=false;pthread_mutex_init(&s->native,NULL);return s;}
int xSemaphoreTake(SemaphoreHandle_t s,int timeout){(void)timeout;pthread_mutex_lock(&s->native);if(s->kind==1){assert(!s->held);s->held=true;return 1;}bool got=s->token;s->token=false;pthread_mutex_unlock(&s->native);return got;}
int xSemaphoreGive(SemaphoreHandle_t s){if(s->kind==1){assert(s->held);s->held=false;pthread_mutex_unlock(&s->native);}else{pthread_mutex_lock(&s->native);s->token=true;seen_signal_mhz=mhz();pthread_mutex_unlock(&s->native);}return 1;}
'''+u[b:]
u=u.replace('int main(void){','int baseline_policy_main(void){',1)
u=u.rstrip();assert u.endswith('}');u=u[:-1]+'return 0;\n}\n'
u+=r'''
static void *work(void*arg){(void)arg;for(int i=0;i<2000;i++){
 metalio_cpu_guard_t a=metalio_cpu_guard_begin(),b=metalio_cpu_guard_begin();assert(!a.error&&!b.error&&mhz()==320);
 if((i%17)==0)metalio_ui_wake_post(METALIO_UI_WAKE_PREP);
 metalio_cpu_guard_end(&b);assert(mhz()==320);metalio_cpu_guard_end(&a);
}return NULL;}
int main(void){reset();assert(metalio_cpu_init()==0);metalio_ui_wake_init();pthread_t t[4];
 for(int i=0;i<4;i++)assert(!pthread_create(t+i,NULL,work,NULL));
 for(int i=0;i<2000;i++){(void)metalio_cpu_idle_begin(true,200);metalio_ui_wake_post(METALIO_UI_WAKE_TOUCH);(void)metalio_ui_wake_wait(0);assert(metalio_cpu_wake()==0);}
 for(int i=0;i<4;i++)pthread_join(t[i],NULL);
 assert(metalio_cpu_wake()==0);metalio_cpu_stats_t s=metalio_cpu_stats();assert(s.enabled&&s.ui_pinned&&!s.unsafe&&!s.faults&&!s.leases&&count==1&&mhz()==320&&s.idle_windows);
 puts("PASS actual concurrent governor+latch:8000 nested worker transactions race2000idle/input cycles, no max-clock loss, lease leak or SDK handle overlap");}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'freertos').mkdir()
 (p/'esp_err.h').write_text('#pragma once\ntypedef int esp_err_t;\nenum{ESP_OK,ESP_FAIL,ESP_ERR_INVALID_STATE,ESP_ERR_NO_MEM};\nconst char*esp_err_to_name(int);\n')
 (p/'esp_pm.h').write_text('#pragma once\n#include <stdbool.h>\ntypedef void*esp_pm_lock_handle_t;\nenum{ESP_PM_CPU_FREQ_MAX};\ntypedef struct{int max_freq_mhz,min_freq_mhz;bool light_sleep_enable;}esp_pm_config_t;\nint esp_pm_configure(const esp_pm_config_t*);int esp_pm_lock_create(int,int,const char*,esp_pm_lock_handle_t*);int esp_pm_lock_acquire(esp_pm_lock_handle_t);int esp_pm_lock_release(esp_pm_lock_handle_t);\n')
 (p/'esp_system.h').write_text('#pragma once\nenum{ESP_RST_POWERON,ESP_RST_PANIC,ESP_RST_INT_WDT,ESP_RST_TASK_WDT,ESP_RST_WDT,ESP_RST_BROWNOUT};\ntypedef int esp_reset_reason_t;int esp_reset_reason(void);\n')
 (p/'esp_log.h').write_text('#pragma once\n#define ESP_LOGW(...) ((void)0)\n')
 (p/'freertos/FreeRTOS.h').write_text('#pragma once\n#include <assert.h>\n#include <pthread.h>\ntypedef pthread_mutex_t portMUX_TYPE;typedef int TickType_t,BaseType_t;\n#define portMUX_INITIALIZER_UNLOCKED PTHREAD_MUTEX_INITIALIZER\n#define portENTER_CRITICAL(p) pthread_mutex_lock(p)\n#define portEXIT_CRITICAL(p) pthread_mutex_unlock(p)\n#define portMAX_DELAY 99999\n#define pdMS_TO_TICKS(n) (n)\n#define configASSERT(x) assert(x)\n')
 (p/'freertos/semphr.h').write_text('#pragma once\n#include "FreeRTOS.h"\n#include <stdbool.h>\ntypedef struct{int kind;bool held,token;pthread_mutex_t native;}StaticSemaphore_t;typedef StaticSemaphore_t*SemaphoreHandle_t;\nSemaphoreHandle_t xSemaphoreCreateMutexStatic(StaticSemaphore_t*);SemaphoreHandle_t xSemaphoreCreateBinaryStatic(StaticSemaphore_t*);int xSemaphoreTake(SemaphoreHandle_t,int);int xSemaphoreGive(SemaphoreHandle_t);\n')
 (p/'freertos/task.h').write_text('#pragma once\n');(p/'test.c').write_text(u)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined','-pthread','-I'+str(p),'-I'+str(r/'components/metalio_hw/include'),'-I'+str(r/'components/metalio_hw'),str(p/'test.c'),'-o',str(p/'test')],check=True);subprocess.run([str(p/'test')],check=True)
