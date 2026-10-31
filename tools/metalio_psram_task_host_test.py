#!/usr/bin/env python3
"""Execute actual PSRAM task owner against a checked scheduler/capability model.
SPDX-License-Identifier: Apache-2.0
"""
from pathlib import Path
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[1]
header=r'''
#pragma once
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
typedef int BaseType_t;
typedef unsigned UBaseType_t;
typedef unsigned char StackType_t;
typedef void (*TaskFunction_t)(void*);
typedef struct {uintptr_t data[64];} StaticTask_t;
typedef struct {uintptr_t data[16];} StaticQueue_t;
typedef struct fake_task *TaskHandle_t;
typedef void *QueueHandle_t;
#define configNUMBER_OF_CORES 2
#define pdTRUE 1
#define pdPASS 1
#define pdFAIL 0
#define portMAX_DELAY 0xffffffffu
#define tskNO_AFFINITY -1
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_INTERNAL 4
#define MALLOC_CAP_8BIT 2
#define CONFIG_FREERTOS_TASK_CREATE_ALLOW_EXT_MEM 1
#define CONFIG_FREERTOS_SUPPORT_STATIC_ALLOCATION 1
#define configASSERT(c) assert(c)
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
TaskHandle_t xTaskCreateStaticPinnedToCore(TaskFunction_t,const char*,uint32_t,void*,UBaseType_t,StackType_t*,StaticTask_t*,BaseType_t);
BaseType_t xTaskCreatePinnedToCoreWithCaps(TaskFunction_t,const char*,uint32_t,void*,UBaseType_t,TaskHandle_t*,BaseType_t,UBaseType_t);
void vTaskDeleteWithCaps(TaskHandle_t);
void vTaskSuspend(TaskHandle_t);
TaskHandle_t xTaskGetCurrentTaskHandle(void);
TaskHandle_t xTaskGetCurrentTaskHandleForCore(BaseType_t core);
void vTaskDelay(unsigned ticks);
QueueHandle_t xQueueCreateStatic(unsigned,unsigned,uint8_t*,StaticQueue_t*);
BaseType_t xQueueSend(QueueHandle_t,const void*,unsigned);
BaseType_t xQueueReceive(QueueHandle_t,void*,unsigned);
'''
unit=r'''
#include "shim.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <setjmp.h>
#include "metalio_task.h"
struct fake_task {void *stack;StaticTask_t *tcb;TaskFunction_t entry;void *arg;unsigned bytes,priority;int core;};
static struct fake_task cleanup;
static TaskFunction_t cleanup_entry;
static TaskHandle_t current;
static TaskHandle_t pending[8];static unsigned queued,head,deleted,stack_live,tcb_live;
static unsigned queue_creates,reaper_creates,caps_creates;
static bool fail_queue,fail_reaper,fail_caps,fail_stack;
static jmp_buf stop;
QueueHandle_t xQueueCreateStatic(unsigned depth,unsigned size,uint8_t *items,StaticQueue_t *queue){assert(depth==8&&size==sizeof(TaskHandle_t)&&items&&queue);++queue_creates;return fail_queue?NULL:queue;}
TaskHandle_t xTaskCreateStaticPinnedToCore(TaskFunction_t fn,const char *name,uint32_t bytes,void *arg,UBaseType_t priority,StackType_t *stack,StaticTask_t *tcb,BaseType_t core){assert(!strcmp(name,"psram_reap")&&bytes==2048&&!arg&&priority==2&&core==-1&&stack&&tcb);++reaper_creates;cleanup_entry=fn;return fail_reaper?NULL:&cleanup;}
static UBaseType_t last_caps;
BaseType_t xTaskCreatePinnedToCoreWithCaps(TaskFunction_t fn,const char *name,uint32_t bytes,void *arg,UBaseType_t priority,TaskHandle_t *out,BaseType_t core,UBaseType_t caps){assert(fn&&name&&bytes&&(caps==3||caps==6));last_caps=caps;++caps_creates;if(fail_caps)return pdFAIL;struct fake_task *t=calloc(1,sizeof(*t));assert(t);t->tcb=calloc(1,sizeof(*t->tcb));assert(t->tcb);++tcb_live;
 if(fail_stack){free(t->tcb);--tcb_live;free(t);return pdFAIL;}
 t->stack=malloc(bytes);assert(t->stack);++stack_live;t->bytes=bytes;t->priority=priority;t->core=core;t->arg=arg;t->entry=fn;if(out)*out=t;else {assert(!"test expects handle for deletion");}return pdPASS;}
TaskHandle_t xTaskGetCurrentTaskHandle(void){return current;}
TaskHandle_t xTaskGetCurrentTaskHandleForCore(BaseType_t core){(void)core;return current;}
void vTaskDelay(unsigned ticks){(void)ticks;}
BaseType_t xQueueSend(QueueHandle_t q,const void *value,unsigned wait){assert(q&&wait==portMAX_DELAY&&queued<8);pending[(head+queued)%8]=*(TaskHandle_t*)value;++queued;return pdTRUE;}
BaseType_t xQueueReceive(QueueHandle_t q,void *value,unsigned wait){assert(q&&wait==portMAX_DELAY);if(!queued)longjmp(stop,2);*(TaskHandle_t*)value=pending[head];head=(head+1)%8;--queued;return pdTRUE;}
void vTaskSuspend(TaskHandle_t t){if(t){assert(t!=current);return;}assert(current!=&cleanup);longjmp(stop,1);}
void vTaskDeleteWithCaps(TaskHandle_t t){assert(t&&t!=current&&t!=&cleanup);free(t->stack);free(t->tcb);free(t);--stack_live;--tcb_live;++deleted;}
static void drain(void){current=&cleanup;if(!setjmp(stop))cleanup_entry(NULL);current=NULL;}
static void finish(TaskHandle_t t){current=t;if(!setjmp(stop))metalio_task_exit();current=NULL;}
static void worker(void *arg){(void)arg;}
int main(void){
 TaskHandle_t task=(TaskHandle_t)(uintptr_t)1;
 fail_queue=true;assert(metalio_task_create(worker,"test",12288,NULL,1,&task)==pdFAIL&&!task&&!caps_creates);fail_queue=false;
 fail_reaper=true;assert(metalio_task_create(worker,"test",12288,NULL,1,&task)==pdFAIL&&!task&&!caps_creates);fail_reaper=false;
 assert(metalio_task_create_pinned(worker,"book_prep",16384,(void*)123,3,&task,1)==pdPASS);
 assert(last_caps==3);
 assert(task->bytes==16384&&task->arg==(void*)123&&task->priority==3&&task->core==1);
 unsigned creates=reaper_creates,queues=queue_creates;
 current=&cleanup;metalio_task_delete(task);current=NULL;assert(!stack_live&&!tcb_live);
 for(unsigned round=0;round<10;round++){
  TaskHandle_t tasks[8];
  for(unsigned i=0;i<8;i++){assert(metalio_task_create(worker,"covers",12288,NULL,1,&tasks[i])==pdPASS);assert(tasks[i]->core==-1);finish(tasks[i]);}
  assert(stack_live==8&&tcb_live==8&&queued==8);drain();assert(!stack_live&&!tcb_live&&!queued);
 }
 assert(reaper_creates==creates&&queue_creates==queues);
 fail_caps=true;assert(metalio_task_create(worker,"avatar",12288,NULL,1,&task)==pdFAIL&&!task);fail_caps=false;
 fail_stack=true;assert(metalio_task_create(worker,"avatar",12288,NULL,1,&task)==pdFAIL&&!task&&!stack_live&&!tcb_live);fail_stack=false;
 assert(metalio_task_create(NULL,"bad",12288,NULL,1,&task)==pdFAIL&&!task);
 assert(metalio_task_create_internal(worker,"flash-cover",12288,NULL,1,&task)==pdPASS);
 assert(last_caps==6);finish(task);drain();assert(!stack_live&&!tcb_live);
 assert(metalio_task_create_pinned_internal(worker,"flash-prefetch",16384,NULL,3,&task,1)==pdPASS);
 assert(last_caps==6&&task->core==1);current=&cleanup;metalio_task_delete(task);current=NULL;
 assert(deleted==83);puts("PSRAM stack caps/affinity, static reaper, external delete, 80 exits, OOM/initialization failures: PASS");return 0;
}
'''
with tempfile.TemporaryDirectory() as temp:
    path=Path(temp);(path/'freertos').mkdir();(path/'shim.h').write_text(header)
    for name in ['FreeRTOS.h','task.h','queue.h','idf_additions.h']:(path/'freertos'/name).write_text('#include "shim.h"\n')
    for name in ['esp_heap_caps.h','esp_log.h','sdkconfig.h']:(path/name).write_text('#include "shim.h"\n')
    (path/'test.c').write_text(unit)
    binary=path/'test'
    subprocess.run(['cc','-std=c11','-DESP_PLATFORM=1','-DKIIKO_BOARD_METALIO47=1','-Wall','-Wextra','-Werror','-fsanitize=undefined','-I'+str(path),'-I'+str(ROOT/'components/metalio_hw/include'),str(path/'test.c'),str(ROOT/'components/metalio_hw/metalio_task.c'),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
