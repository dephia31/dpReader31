/* SPDX-License-Identifier: Apache-2.0
 * Reserve one internal cleanup stack instead of allocating cleanup tasks during
 * PSRAM-worker exit. TCBs, queue storage and reaper remain internal SRAM.
 */
#include "metalio_task.h"
#include "freertos/queue.h"
#include "freertos/idf_additions.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>

_Static_assert(CONFIG_FREERTOS_TASK_CREATE_ALLOW_EXT_MEM,"PSRAM task stacks must be enabled");
_Static_assert(CONFIG_FREERTOS_SUPPORT_STATIC_ALLOCATION,"Static cleanup resources required");
#define REAPER_STACK_BYTES 2048u
#define REAPER_DEPTH 8u
static StaticTask_t s_reaper_tcb;
static StackType_t s_reaper_stack[REAPER_STACK_BYTES/sizeof(StackType_t)];
static StaticQueue_t s_queue_storage;
static uint8_t s_queue_items[REAPER_DEPTH*sizeof(TaskHandle_t)];
static QueueHandle_t s_queue;
static atomic_int s_ready;

/* Synchronize here as well: do not rely on an SDK-local WithCaps fix being
 * installed on every builder. Free only after the task left both CPU cores. */
static void destroy(TaskHandle_t task) {
    vTaskSuspend(task);
    for(;;) {
        bool running=false;
        for(BaseType_t core=0;core<configNUMBER_OF_CORES;++core)
            if(xTaskGetCurrentTaskHandleForCore(core)==task)running=true;
        if(!running)break;
        vTaskDelay(1);
    }
    vTaskDeleteWithCaps(task);
}
static void reaper(void *unused) {
    (void)unused;
    for(;;) {
        TaskHandle_t task=NULL;
        if(xQueueReceive(s_queue,&task,portMAX_DELAY)==pdTRUE&&task)
            destroy(task);
    }
}
static bool prepare_reaper(void) {
    int state=atomic_load_explicit(&s_ready,memory_order_acquire);
    if(state==2)return true;
    int empty=0;
    if(!atomic_compare_exchange_strong_explicit(&s_ready,&empty,1,memory_order_acq_rel,memory_order_acquire))return false;
    s_queue=xQueueCreateStatic(REAPER_DEPTH,sizeof(TaskHandle_t),s_queue_items,&s_queue_storage);
    TaskHandle_t task=s_queue?xTaskCreateStaticPinnedToCore(reaper,"psram_reap",REAPER_STACK_BYTES,
        NULL,2,s_reaper_stack,&s_reaper_tcb,tskNO_AFFINITY):NULL;
    if(!task){atomic_store_explicit(&s_ready,0,memory_order_release);return false;}
    atomic_store_explicit(&s_ready,2,memory_order_release);
    return true;
}
static BaseType_t create(TaskFunction_t entry,const char *name,uint32_t stack_bytes,
                         void *arg,UBaseType_t priority,TaskHandle_t *out,BaseType_t core,UBaseType_t caps) {
    if(out)*out=NULL;
    if(!entry||!stack_bytes||!prepare_reaper())return pdFAIL;
    /* Explicit PSRAM request: do not consume scarce SRAM on allocation failure. */
    BaseType_t result=xTaskCreatePinnedToCoreWithCaps(entry,name,stack_bytes,arg,priority,out,core,
        caps|MALLOC_CAP_8BIT);
    if(result==pdPASS)ESP_LOGI("memory","worker=%s stack=%s bytes=%u",name,caps==MALLOC_CAP_SPIRAM?"PSRAM":"internal",(unsigned)stack_bytes);
    else ESP_LOGW("memory","worker stack unavailable: %s (%u bytes)",name,(unsigned)stack_bytes);
    return result;
}
BaseType_t metalio_task_create_pinned(TaskFunction_t entry,const char *name,uint32_t bytes,
                                      void *arg,UBaseType_t priority,TaskHandle_t *out,BaseType_t core) {
    return create(entry,name,bytes,arg,priority,out,core,MALLOC_CAP_SPIRAM);
}
BaseType_t metalio_task_create_pinned_internal(TaskFunction_t entry,const char *name,uint32_t bytes,
                                               void *arg,UBaseType_t priority,TaskHandle_t *out,BaseType_t core) {
    return create(entry,name,bytes,arg,priority,out,core,MALLOC_CAP_INTERNAL);
}
BaseType_t metalio_task_create_internal(TaskFunction_t entry,const char *name,uint32_t bytes,
                                        void *arg,UBaseType_t priority,TaskHandle_t *out) {
    return create(entry,name,bytes,arg,priority,out,tskNO_AFFINITY,MALLOC_CAP_INTERNAL);
}
BaseType_t metalio_task_create(TaskFunction_t entry,const char *name,uint32_t stack_bytes,
                               void *arg,UBaseType_t priority,TaskHandle_t *out) {
    return metalio_task_create_pinned(entry,name,stack_bytes,arg,priority,out,tskNO_AFFINITY);
}
void metalio_task_exit(void) {
    TaskHandle_t self=xTaskGetCurrentTaskHandle();
    configASSERT(atomic_load_explicit(&s_ready,memory_order_acquire)==2&&self);
    /* Queue storage is static. No allocation is needed to return the stack. SDK
     * WithCaps deletion suspends/waits for the other core before freeing it. */
    BaseType_t queued=xQueueSend(s_queue,&self,portMAX_DELAY);
    if(queued!=pdTRUE)abort();
    for(;;)vTaskSuspend(NULL);
}
void metalio_task_delete(TaskHandle_t task) {
    configASSERT(task&&task!=xTaskGetCurrentTaskHandle());
    destroy(task);
}
