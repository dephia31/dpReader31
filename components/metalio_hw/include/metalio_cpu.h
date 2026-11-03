/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

typedef struct { bool held; esp_err_t error; } metalio_cpu_guard_t;
typedef struct {
    bool enabled, ui_pinned, unsafe;
    uint32_t idle_windows, rejected, wakes, leases, faults;
    esp_err_t last_error;
} metalio_cpu_stats_t;
/* Task context only. No automatic light sleep; active max320, eligible idle min160. */
esp_err_t metalio_cpu_init(void);
esp_err_t metalio_cpu_wake(void);
bool metalio_cpu_idle_begin(bool eligible, uint32_t wait_ms);
metalio_cpu_guard_t metalio_cpu_guard_begin(void);
void metalio_cpu_guard_end(metalio_cpu_guard_t *scope);
metalio_cpu_stats_t metalio_cpu_stats(void);
#define METALIO_CPU_GUARD_OR_RETURN(...) \
    metalio_cpu_guard_t cpu_scope __attribute__((cleanup(metalio_cpu_guard_end)))=metalio_cpu_guard_begin(); \
    if(cpu_scope.error!=ESP_OK)return __VA_ARGS__
