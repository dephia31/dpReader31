/* SPDX-License-Identifier: Apache-2.0
 * PSRAM stacks only for audited CPU/SD workers, never cache-off/flash/ISR paths.
 */
#pragma once
#if defined(ESP_PLATFORM) && defined(KIIKO_BOARD_METALIO47) && KIIKO_BOARD_METALIO47
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
BaseType_t metalio_task_create(TaskFunction_t entry,const char *name,uint32_t stack_bytes,
                               void *arg,UBaseType_t priority,TaskHandle_t *out);
BaseType_t metalio_task_create_pinned(TaskFunction_t entry,const char *name,uint32_t stack_bytes,
                                      void *arg,UBaseType_t priority,TaskHandle_t *out,BaseType_t core);
/* Internal-stack variants for a worker which may touch the flash filesystem;
 * same WithCaps ownership/reaper contract as the PSRAM variants. */
BaseType_t metalio_task_create_internal(TaskFunction_t entry,const char *name,uint32_t stack_bytes,
                                        void *arg,UBaseType_t priority,TaskHandle_t *out);
BaseType_t metalio_task_create_pinned_internal(TaskFunction_t entry,const char *name,uint32_t stack_bytes,
                                               void *arg,UBaseType_t priority,TaskHandle_t *out,BaseType_t core);
/* Only after worker releases every caller-owned handle/buffer. Deferred internal
 * reaper avoids the SDK's allocation of a temporary task for each self-delete. */
void metalio_task_exit(void) __attribute__((noreturn));
/* External owner must first stop/join resource use (same as normal task deletion). */
void metalio_task_delete(TaskHandle_t task);
#else
/* Existing native host harnesses model task scheduling, not ESP heap placement. */
#define metalio_task_create_internal xTaskCreate
#define metalio_task_create_pinned_internal xTaskCreatePinnedToCore
#define metalio_task_create xTaskCreate
#define metalio_task_create_pinned xTaskCreatePinnedToCore
#define metalio_task_exit() vTaskDelete(NULL)
#define metalio_task_delete vTaskDelete
#endif
