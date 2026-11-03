#!/usr/bin/env python3
"""Execute actual S31 governor and UI wake latch with SDK locks/faults simulated."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1]
u=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "metalio_cpu.h"
#include "metalio_ui_wake.h"
#include "esp_pm.h"
#include "esp_system.h"
#include "freertos/semphr.h"
static int reset_reason,create_error,acquire_error,release_error,config_error,config_calls;
static int count,minimum=320,maximum=320,seen_signal_mhz;
static int mhz(void){return count?maximum:minimum;}
SemaphoreHandle_t xSemaphoreCreateMutexStatic(StaticSemaphore_t*s){s->kind=1;s->held=false;return s;}
SemaphoreHandle_t xSemaphoreCreateBinaryStatic(StaticSemaphore_t*s){s->kind=2;s->token=false;return s;}
int xSemaphoreTake(SemaphoreHandle_t s,int timeout){if(s->kind==1){assert(timeout==portMAX_DELAY&&!s->held);s->held=true;return 1;}bool got=s->token;s->token=false;return got;}
int xSemaphoreGive(SemaphoreHandle_t s){if(s->kind==1){assert(s->held);s->held=false;}else{s->token=true;seen_signal_mhz=mhz();}return 1;}
int esp_reset_reason(void){return reset_reason;}
int esp_pm_lock_create(int type,int arg,const char*name,esp_pm_lock_handle_t*out){assert(type==ESP_PM_CPU_FREQ_MAX&&!arg&&!strcmp(name,"reader_cpu"));if(create_error)return create_error;*out=(void*)1;return 0;}
int esp_pm_lock_acquire(esp_pm_lock_handle_t h){assert(h);if(acquire_error){int err=acquire_error;acquire_error=0;return err;}++count;return 0;}
int esp_pm_lock_release(esp_pm_lock_handle_t h){assert(h&&count>0);if(release_error){int err=release_error;release_error=0;return err;}--count;return 0;}
int esp_pm_configure(const esp_pm_config_t*c){assert(c->max_freq_mhz==320&&!c->light_sleep_enable&&(c->min_freq_mhz==160||c->min_freq_mhz==320));++config_calls;if(config_error){int e=config_error;config_error=0;return e;}maximum=c->max_freq_mhz;minimum=c->min_freq_mhz;return 0;}
const char *esp_err_to_name(int err){(void)err;return "mock";}
#include "metalio_cpu.c"
#include "metalio_ui_wake.c"
static void reset(void){initialized=false;max_lock=NULL;memset(&state,0,sizeof(state));mutex=NULL;wake_signal=NULL;pending=0;memset(&stats,0,sizeof(stats));count=0;minimum=maximum=320;reset_reason=ESP_RST_POWERON;create_error=acquire_error=release_error=config_error=config_calls=0;}
static int critical_transaction(bool fail){METALIO_CPU_GUARD_OR_RETURN(-100);assert(mhz()==320);return fail?-7:0;}
int main(void){
 reset();assert(critical_transaction(false)==0&&!count&&mhz()==320); // Boot before initialization.
 assert(metalio_cpu_init()==ESP_OK&&state.enabled&&state.ui_pinned&&count==1&&mhz()==320);
 metalio_ui_wake_init();assert(!metalio_cpu_idle_begin(false,200)&&!metalio_cpu_idle_begin(true,40)&&mhz()==320);
 metalio_ui_wake_post(METALIO_UI_WAKE_TOUCH);assert(!metalio_cpu_idle_begin(true,200));assert(metalio_ui_wake_wait(0)==METALIO_UI_WAKE_TOUCH);
 assert(metalio_cpu_idle_begin(true,200)&&mhz()==160);
 metalio_cpu_guard_t a=metalio_cpu_guard_begin(),b=metalio_cpu_guard_begin();assert(!a.error&&!b.error&&mhz()==320&&state.leases==2);
 metalio_cpu_guard_end(&b);assert(mhz()==320);metalio_cpu_guard_end(&a);assert(mhz()==160&&!state.leases);
 assert(critical_transaction(true)==-7&&mhz()==160&&!state.leases); // Early-return cleanup.
 metalio_ui_wake_post(METALIO_UI_WAKE_KEY);assert(seen_signal_mhz==320&&state.ui_pinned&&mhz()==320);assert(metalio_ui_wake_wait(200)==METALIO_UI_WAKE_KEY);
 assert(metalio_cpu_idle_begin(true,150));(void)metalio_ui_wake_wait(150);assert(metalio_cpu_wake()==ESP_OK&&mhz()==320); // Deadline wake.
 // A job spanning UI unpin still owns max until its entire transaction is done.
 a=metalio_cpu_guard_begin();assert(metalio_cpu_idle_begin(true,200)&&mhz()==320);assert(critical_transaction(false)==0&&mhz()==320);metalio_cpu_guard_end(&a);assert(mhz()==160);
 acquire_error=ESP_FAIL;assert(metalio_cpu_wake()==ESP_OK&&!state.enabled&&minimum==320&&state.faults==1);assert(!metalio_cpu_idle_begin(true,200));
 reset();create_error=ESP_ERR_NO_MEM;assert(metalio_cpu_init()!=ESP_OK&&!state.enabled&&minimum==320);
 reset();acquire_error=ESP_FAIL;assert(metalio_cpu_init()!=ESP_OK&&!state.enabled&&minimum==320);
 reset();config_error=ESP_FAIL;assert(metalio_cpu_init()!=ESP_OK&&!state.enabled&&state.ui_pinned&&mhz()==320);
 for(int reason=ESP_RST_PANIC;reason<=ESP_RST_BROWNOUT;reason++){reset();reset_reason=reason;assert(metalio_cpu_init()!=ESP_OK&&!config_calls&&minimum==320);}
 reset();assert(metalio_cpu_init()==ESP_OK);release_error=ESP_FAIL;assert(!metalio_cpu_idle_begin(true,200)&&minimum==320&&!state.enabled);
 reset();assert(metalio_cpu_init()==ESP_OK);a=metalio_cpu_guard_begin();release_error=ESP_FAIL;metalio_cpu_guard_end(&a);assert(!state.enabled&&minimum==320&&!a.held);
 reset();assert(metalio_cpu_init()==ESP_OK);metalio_ui_wake_init();assert(metalio_cpu_idle_begin(true,200));
 acquire_error=config_error=ESP_FAIL;assert(metalio_cpu_wake()!=ESP_OK&&state.unsafe&&!state.enabled);
 assert(critical_transaction(false)==-100&&!metalio_cpu_idle_begin(true,200)); // Unconfirmed clock: refuse hardware work.
 puts("PASS actual CPU governor+wake:320 pinned,160 only eligible wait, nested/full-transaction leases, early returns, input boost before signal, deadline restore, init/PM errors freeze320, recovery boots skip DFS");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'freertos').mkdir()
 (p/'esp_err.h').write_text('#pragma once\ntypedef int esp_err_t;\nenum{ESP_OK,ESP_FAIL,ESP_ERR_INVALID_STATE,ESP_ERR_NO_MEM};\nconst char*esp_err_to_name(int);\n')
 (p/'esp_pm.h').write_text('#pragma once\n#include <stdbool.h>\ntypedef void*esp_pm_lock_handle_t;\nenum{ESP_PM_CPU_FREQ_MAX};\ntypedef struct{int max_freq_mhz,min_freq_mhz;bool light_sleep_enable;}esp_pm_config_t;\nint esp_pm_configure(const esp_pm_config_t*);int esp_pm_lock_create(int,int,const char*,esp_pm_lock_handle_t*);int esp_pm_lock_acquire(esp_pm_lock_handle_t);int esp_pm_lock_release(esp_pm_lock_handle_t);\n')
 (p/'esp_system.h').write_text('#pragma once\nenum{ESP_RST_POWERON,ESP_RST_PANIC,ESP_RST_INT_WDT,ESP_RST_TASK_WDT,ESP_RST_WDT,ESP_RST_BROWNOUT};\ntypedef int esp_reset_reason_t;int esp_reset_reason(void);\n')
 (p/'esp_log.h').write_text('#pragma once\n#define ESP_LOGW(...) ((void)0)\n')
 (p/'freertos/FreeRTOS.h').write_text('#pragma once\n#include <assert.h>\ntypedef int portMUX_TYPE,TickType_t,BaseType_t;\n#define portMUX_INITIALIZER_UNLOCKED 0\n#define portENTER_CRITICAL(p) ((void)(p))\n#define portEXIT_CRITICAL(p) ((void)(p))\n#define portMAX_DELAY 99999\n#define pdMS_TO_TICKS(n) (n)\n#define configASSERT(x) assert(x)\n')
 (p/'freertos/semphr.h').write_text('#pragma once\n#include "FreeRTOS.h"\n#include <stdbool.h>\ntypedef struct{int kind;bool held,token;}StaticSemaphore_t;typedef StaticSemaphore_t*SemaphoreHandle_t;\nSemaphoreHandle_t xSemaphoreCreateMutexStatic(StaticSemaphore_t*);SemaphoreHandle_t xSemaphoreCreateBinaryStatic(StaticSemaphore_t*);int xSemaphoreTake(SemaphoreHandle_t,int);int xSemaphoreGive(SemaphoreHandle_t);\n')
 (p/'freertos/task.h').write_text('#pragma once\n')
 (p/'test.c').write_text(u)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined','-I'+str(p),'-I'+str(r/'components/metalio_hw/include'),'-I'+str(r/'components/metalio_hw'),str(p/'test.c'),'-o',str(p/'test')],check=True);subprocess.run([str(p/'test')],check=True)
