"""中文：执行真实堆诊断函数，验证能力标记和PSRAM快照失败。/ English: Execute real profiling with capability and snapshot-failure checks."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'freertos').mkdir()
 headers=['esp_heap_caps.h','esp_log.h','esp_memory_utils.h','freertos/FreeRTOS.h','freertos/task.h']
 for h in headers:(p/h).write_text('#include "stub.h"\n')
 (p/'stub.h').write_text(r'''
#ifndef STUB_H
#define STUB_H
#include <stddef.h>
#include <stdlib.h>
#include <assert.h>
#define MALLOC_CAP_INTERNAL 1
#define MALLOC_CAP_8BIT 2
#define MALLOC_CAP_SPIRAM 4
#define MALLOC_CAP_DMA 8
#define CONFIG_FREERTOS_USE_TRACE_FACILITY 1
#define ESP_LOGI(...) capture()
#define ESP_LOGW(...) capture()
typedef unsigned UBaseType_t;typedef unsigned char StackType_t;
typedef struct{char *pcTaskName;void *pxStackBase;unsigned usStackHighWaterMark,uxCurrentPriority;}TaskStatus_t;
static int logs,queries,allocations;static void capture(void){logs++;}
static size_t heap_caps_get_free_size(unsigned c){assert(c==3||c==6||c==9);queries++;return 100;}
static size_t heap_caps_get_largest_free_block(unsigned c){return heap_caps_get_free_size(c);}
static size_t heap_caps_get_minimum_free_size(unsigned c){return heap_caps_get_free_size(c);}
static size_t heap_caps_get_total_size(unsigned c){return heap_caps_get_free_size(c);}
static unsigned uxTaskGetStackHighWaterMark(void*p){return 100;}
static unsigned uxTaskGetNumberOfTasks(void){return 2;}
static void *heap_caps_calloc(size_t n,size_t s,unsigned c){assert(c==6);allocations++;return NULL;}
static unsigned uxTaskGetSystemState(TaskStatus_t*t,unsigned n,void*p){assert(0);return 0;}
static int esp_ptr_external_ram(void*p){return 0;}
#endif
'''.replace('#define ESP_LOGI(...) capture()','#define ESP_LOGI(tag,fmt,...) do { printf(fmt,__VA_ARGS__);capture();} while(0)').replace('#include <stddef.h>','#include <stddef.h>\n#include <stdio.h>'))
 (p/'test.c').write_text('#include "stub.h"\n#include "'+str(root/'components/metalio_hw/metalio_memory_profile.c')+'"\nint main(void){print_memory_profile("test");assert(queries==9&&logs==1);print_task_stack_profile();assert(allocations==1&&logs==2);puts("PASS: real memory helper queries internal/PSRAM/DMA free and largest; snapshot OOM has no internal fallback");}\n')
 subprocess.run(['cc','-std=gnu11','-I'+d,'-I'+str(root/'components/metalio_hw/include'),str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
