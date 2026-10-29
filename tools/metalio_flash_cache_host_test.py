#!/usr/bin/env python3
"""中文：验证S31生成的SDK回移与最终IRAM/取指保护。/ English: Verify generated S31 SDK backports and final IRAM/predictor protection."""
from pathlib import Path
import re,subprocess,tempfile
from footer_style_host_test import function
# Importing footer_style_host_test also runs its independent footer acceptance checks.
root=Path(__file__).resolve().parents[1]
build=root/'build/boards/metalio47/build'
cache=build/'s31_cache_compat/spi_flash/cache_utils.c'
ipc=build/'s31_cache_compat/esp_system/port/arch/riscv/esp_ipc_isr_routines.c'
panic=build/'s31_cache_compat/esp_system/port/panic_handler.c'
block=function('spi_flash_op_block_func',cache)
wait=function('esp_ipc_isr_waiting_for_finish_cmd',ipc)
busy=function('busy_wait',panic)
assert block.index('esp_cpu_branch_prediction_disable()')<block.index('s_flash_op_can_start = true')
assert wait.index('esp_cpu_branch_prediction_disable()')<wait.index('while (')<wait.index('esp_cpu_branch_prediction_enable()')
assert busy.index('esp_cpu_branch_prediction_disable()')<busy.index('ESP_INFINITE_LOOP()')
unit=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdatomic.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#define IRAM_ATTR
#define ESP_SYSTEM_IRAM_ATTR
#define SOC_BRANCH_PREDICTOR_SUPPORTED 1
#define CONFIG_FREERTOS_SMP 0
static atomic_bool prediction=true,s_flash_op_complete,s_flash_op_can_start;
static unsigned s_flash_op_cache_state[2],restores;
static void vTaskSuspendAll(void){}
static void esp_intr_noniram_disable(void){}
static void esp_cpu_branch_prediction_disable(void){atomic_store(&prediction,false);}
static void esp_cpu_branch_prediction_enable(void){atomic_store(&prediction,true);}
static void spi_flash_restore_cache(uint32_t c,unsigned state){(void)c;(void)state;assert(!prediction);++restores;esp_cpu_branch_prediction_enable();}
static void esp_intr_noniram_enable(void){assert(prediction);}
static void xTaskResumeAll(void){assert(prediction);}
static void *flash_core(void *p){(void)p;while(!s_flash_op_can_start)sched_yield();assert(!prediction);s_flash_op_complete=true;return NULL;}
'''+block+'\n'+wait+r'''
int main(void){
 for(int i=0;i<100;++i){s_flash_op_complete=false;s_flash_op_can_start=false;prediction=true;pthread_t t;assert(!pthread_create(&t,NULL,flash_core,NULL));spi_flash_op_block_func((void*)1);assert(!pthread_join(t,NULL));assert(prediction);}
 uint32_t released=1;esp_ipc_isr_waiting_for_finish_cmd(&released);assert(prediction&&restores==100);
 puts("PASS: actual generated flash guard disables prediction before admitting flash access and restores after completion; IPC stall has balanced protection");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(unit);subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-void-pointer-to-int-cast','-pthread',str(p/'test.c'),'-o',str(p/'test')],check=True);subprocess.run([str(p/'test')],check=True)
objdump=next((Path.home()/'.espressif/tools/riscv32-esp-elf').glob('*/riscv32-esp-elf/bin/riscv32-esp-elf-objdump'))
for name in ('spi_flash_op_block_func','esp_ipc_isr_waiting_for_finish_cmd'):
 out=subprocess.check_output([str(objdump),'-d','--disassemble='+name,str(build/'dpReader.elf')],text=True)
 match=re.search(r'([0-9a-f]+) <'+name+r'>:',out);assert match and 0x2f000000<=int(match[1],16)<0x2f080000,name
 assert 'csrrc' in out and 'csrrs' in out,name
 if name=='spi_flash_op_block_func':assert out.index('csrrc')<out.index('<s_flash_op_can_start>')
print('PASS: both protected parked-core routines are linked in internal IRAM and contain predictor disable/enable instructions')
