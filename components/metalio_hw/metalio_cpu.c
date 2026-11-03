/* SPDX-License-Identifier: Apache-2.0
 * S31 clock policy: pin max outside explicit ready-reader waits. Serialize SDK
 * PM handle operations; scoped leases protect complete peripheral transactions.
 */
#include "metalio_cpu.h"
#include "metalio_ui_wake.h"
#include "esp_pm.h"
#include "esp_system.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <stdatomic.h>

static StaticSemaphore_t mutex_storage;
static SemaphoreHandle_t mutex;
static atomic_bool initialized;
static esp_pm_lock_handle_t max_lock;
static metalio_cpu_stats_t state;

static esp_err_t freeze_locked(esp_err_t reason) {
    ++state.faults;state.last_error=reason;
    const esp_pm_config_t safe={.max_freq_mhz=320,.min_freq_mhz=320,.light_sleep_enable=false};
    esp_err_t err=esp_pm_configure(&safe);
    state.enabled=false;state.unsafe=err!=ESP_OK;
    ESP_LOGW("cpu","DFS disabled, fixed320 result=%s cause=%s",esp_err_to_name(err),esp_err_to_name(reason));
    return err;
}
esp_err_t metalio_cpu_init(void) {
    if(atomic_load_explicit(&initialized,memory_order_acquire)) {
        xSemaphoreTake(mutex,portMAX_DELAY);esp_err_t prior=state.last_error;xSemaphoreGive(mutex);return prior;
    }
    mutex=xSemaphoreCreateMutexStatic(&mutex_storage);
    if(!mutex)return ESP_ERR_NO_MEM;
    xSemaphoreTake(mutex,portMAX_DELAY);
    atomic_store_explicit(&initialized,true,memory_order_release);
    // Recovery boots keep the known max clock; never repeat DFS after panic/WDT/brownout.
    esp_reset_reason_t reset=esp_reset_reason();
    if(reset==ESP_RST_PANIC || reset==ESP_RST_INT_WDT || reset==ESP_RST_TASK_WDT ||
       reset==ESP_RST_WDT || reset==ESP_RST_BROWNOUT) {
        state.last_error=ESP_ERR_INVALID_STATE;xSemaphoreGive(mutex);return state.last_error;
    }
    esp_err_t err=esp_pm_lock_create(ESP_PM_CPU_FREQ_MAX,0,"reader_cpu",&max_lock);
    if(err==ESP_OK)err=esp_pm_lock_acquire(max_lock);
    if(err==ESP_OK) {
        state.ui_pinned=true;
        const esp_pm_config_t config={.max_freq_mhz=320,.min_freq_mhz=160,.light_sleep_enable=false};
        err=esp_pm_configure(&config);
        if(err==ESP_OK)state.enabled=true;
        // Initial max lock remains held if configuration fails.
    }
    state.last_error=err;
    xSemaphoreGive(mutex);
    return err;
}
esp_err_t metalio_cpu_wake(void) {
    if(!atomic_load_explicit(&initialized,memory_order_acquire))return ESP_OK;
    xSemaphoreTake(mutex,portMAX_DELAY);
    esp_err_t err=state.unsafe?ESP_ERR_INVALID_STATE:ESP_OK;
    if(state.enabled && !state.ui_pinned) {
        err=esp_pm_lock_acquire(max_lock);
        if(err==ESP_OK){state.ui_pinned=true;++state.wakes;}
        else err=freeze_locked(err);
    }
    xSemaphoreGive(mutex);return err;
}
bool metalio_cpu_idle_begin(bool eligible,uint32_t wait_ms) {
    if(!atomic_load_explicit(&initialized,memory_order_acquire))return false;
    xSemaphoreTake(mutex,portMAX_DELAY);
    bool allowed=state.enabled && state.ui_pinned && eligible && wait_ms>40 &&
        !state.unsafe && !metalio_ui_wake_pending();
    if(allowed) {
        esp_err_t err=esp_pm_lock_release(max_lock);
        if(err==ESP_OK){state.ui_pinned=false;++state.idle_windows;}
        else {allowed=false;(void)freeze_locked(err);}
    } else ++state.rejected;
    xSemaphoreGive(mutex);return allowed;
}
metalio_cpu_guard_t metalio_cpu_guard_begin(void) {
    metalio_cpu_guard_t scope={0};
    if(!atomic_load_explicit(&initialized,memory_order_acquire))return scope;
    xSemaphoreTake(mutex,portMAX_DELAY);
    if(state.unsafe)scope.error=ESP_ERR_INVALID_STATE;
    else if(state.enabled) {
        scope.error=esp_pm_lock_acquire(max_lock);
        if(scope.error==ESP_OK){scope.held=true;++state.leases;}
        else scope.error=freeze_locked(scope.error);
    }
    xSemaphoreGive(mutex);return scope;
}
void metalio_cpu_guard_end(metalio_cpu_guard_t *scope) {
    if(!scope->held)return;
    xSemaphoreTake(mutex,portMAX_DELAY);
    esp_err_t err=esp_pm_lock_release(max_lock);
    if(state.leases)--state.leases;
    if(err!=ESP_OK)(void)freeze_locked(err);
    xSemaphoreGive(mutex);scope->held=false;
}
metalio_cpu_stats_t metalio_cpu_stats(void) {
    if(!atomic_load_explicit(&initialized,memory_order_acquire))return (metalio_cpu_stats_t){0};
    xSemaphoreTake(mutex,portMAX_DELAY);metalio_cpu_stats_t snapshot=state;xSemaphoreGive(mutex);return snapshot;
}
