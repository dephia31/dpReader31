/* SPDX-License-Identifier: Apache-2.0
 * Single-consumer wake latch. Payload stays in its existing owner's queue.
 * Separate static semaphore avoids consuming touch/renderer task notifications.
 */
#include "metalio_ui_wake.h"
#include "metalio_cpu.h"
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static StaticSemaphore_t storage;
static SemaphoreHandle_t wake_signal;
static portMUX_TYPE lock=portMUX_INITIALIZER_UNLOCKED;
static uint32_t pending;
static metalio_ui_wake_stats_t stats;

void metalio_ui_wake_init(void) {
    // Called once by the UI before its first wait. Early producer bits are retained.
    if(wake_signal)return;
    SemaphoreHandle_t created=xSemaphoreCreateBinaryStatic(&storage);
    configASSERT(created);
    portENTER_CRITICAL(&lock);
    wake_signal=created;bool ready=pending!=0;
    portEXIT_CRITICAL(&lock);
    if(ready)xSemaphoreGive(created);
}
void metalio_ui_wake_post(uint32_t reasons) {
    if(!reasons)return;
    portENTER_CRITICAL(&lock);
    bool empty=pending==0;
    pending|=reasons;++stats.posts;
    SemaphoreHandle_t target=wake_signal;
    portEXIT_CRITICAL(&lock);
    // Publish pending first: idle admission rechecks it under the CPU policy lock.
    // Restore the UI max-clock pin before signaling its consumer.
    if(target){(void)metalio_cpu_wake();if(empty)xSemaphoreGive(target);}
}
uint32_t metalio_ui_wake_wait(uint32_t timeout_ms) {
    configASSERT(wake_signal);
    TickType_t ticks=pdMS_TO_TICKS(timeout_ms);
    if(timeout_ms && !ticks)ticks=1;
    BaseType_t signaled=xSemaphoreTake(wake_signal,ticks);
    portENTER_CRITICAL(&lock);
    uint32_t reasons=pending;pending=0;
    ++stats.waits;
    if(reasons){++stats.event_wakes;stats.reasons|=reasons;}
    else if(signaled)++stats.empty_wakes;
    else ++stats.timeouts;
    portEXIT_CRITICAL(&lock);
    return reasons;
}
void metalio_ui_wake_iteration(void) {
    portENTER_CRITICAL(&lock);++stats.iterations;portEXIT_CRITICAL(&lock);
}
metalio_ui_wake_stats_t metalio_ui_wake_stats(void) {
    portENTER_CRITICAL(&lock);metalio_ui_wake_stats_t copy=stats;portEXIT_CRITICAL(&lock);
    return copy;
}

uint32_t metalio_ui_wake_pending(void) {
    portENTER_CRITICAL(&lock);uint32_t bits=pending;portEXIT_CRITICAL(&lock);return bits;
}
