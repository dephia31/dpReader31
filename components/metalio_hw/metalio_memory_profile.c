/*
 * 中文：记录真实能力堆与栈余量，DMA是内部堆子集，不能相加。
 * English: Record capability heaps and stack margins; DMA overlaps internal RAM and must not be added.
 */
#include "metalio_memory_profile.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_memory_utils.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdlib.h>

void print_memory_profile(const char *stage) {
    const unsigned internal=MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT;
    const unsigned psram=MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT;
    const unsigned dma=MALLOC_CAP_INTERNAL|MALLOC_CAP_DMA;
    ESP_LOGI("memory","%s bytes: internal free=%u largest=%u min=%u total=%u; psram free=%u largest=%u total=%u; DMA free=%u largest=%u; current_stack_hwm=%u",
        stage?stage:"unknown",(unsigned)heap_caps_get_free_size(internal),
        (unsigned)heap_caps_get_largest_free_block(internal),(unsigned)heap_caps_get_minimum_free_size(internal),
        (unsigned)heap_caps_get_total_size(internal),(unsigned)heap_caps_get_free_size(psram),
        (unsigned)heap_caps_get_largest_free_block(psram),(unsigned)heap_caps_get_total_size(psram),
        (unsigned)heap_caps_get_free_size(dma),(unsigned)heap_caps_get_largest_free_block(dma),
        (unsigned)(uxTaskGetStackHighWaterMark(NULL)*sizeof(StackType_t)));
}

void print_task_stack_profile(void) {
#if CONFIG_FREERTOS_USE_TRACE_FACILITY
    UBaseType_t capacity=uxTaskGetNumberOfTasks()+8;
    TaskStatus_t *tasks=heap_caps_calloc(capacity,sizeof(*tasks),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!tasks){ESP_LOGW("memory","task snapshot unavailable: PSRAM allocation failed");return;}
    UBaseType_t count=uxTaskGetSystemState(tasks,capacity,NULL);
    if(!count)ESP_LOGW("memory","task set grew during snapshot; retry at next checkpoint");
    for(UBaseType_t i=0;i<count;++i)
        ESP_LOGI("memory","task=%s stack=%s unused_min_bytes=%u priority=%u",tasks[i].pcTaskName,
            esp_ptr_external_ram(tasks[i].pxStackBase)?"PSRAM":"internal",
            (unsigned)(tasks[i].usStackHighWaterMark*sizeof(StackType_t)),(unsigned)tasks[i].uxCurrentPriority);
    free(tasks);
#endif
}
