"""Exercise the real S31 FT6336U driver: IRQ idle, retries, replay and sleep lifecycle."""
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
STUB = r'''
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#define ESP_OK 0
#define ESP_ERR_INVALID_ARG 1
#define ESP_ERR_NO_MEM 2
#define ESP_ERR_INVALID_STATE 3
#define ESP_ERR_INVALID_RESPONSE 4
#define GPIO_NUM_NC -1
#define GPIO_MODE_INPUT 1
#define GPIO_PULLUP_ENABLE 1
#define GPIO_INTR_NEGEDGE 2
#define pdTRUE 1
#define pdFALSE 0
#define pdPASS 1
#define pdMS_TO_TICKS(n) (n)
#define portMAX_DELAY UINT32_MAX
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(p) ((void)(p))
#define portEXIT_CRITICAL(p) ((void)(p))
#define portYIELD_FROM_ISR() ((void)0)
void test_log(const char*,const char*,...);
#define ESP_LOGI(...) test_log(__VA_ARGS__)
#define ESP_LOGW(...) test_log(__VA_ARGS__)
typedef int esp_err_t,gpio_num_t,portMUX_TYPE,BaseType_t;
typedef void *i2c_master_bus_handle_t,*TaskHandle_t;
typedef struct sem {bool locked;} *SemaphoreHandle_t;
typedef struct {uint64_t pin_bit_mask;int mode,pull_up_en,intr_type;} gpio_config_t;
SemaphoreHandle_t xSemaphoreCreateMutex(void);
int xSemaphoreTake(SemaphoreHandle_t,uint32_t);
int xSemaphoreGive(SemaphoreHandle_t);
void vSemaphoreDelete(SemaphoreHandle_t);
int xTaskCreate(void(*)(void*),const char*,unsigned,void*,unsigned,TaskHandle_t*);
void vTaskDelete(TaskHandle_t);
void xTaskNotifyGive(TaskHandle_t);
void vTaskNotifyGiveFromISR(TaskHandle_t,BaseType_t*);
uint32_t ulTaskNotifyTake(int,uint32_t);
int gpio_config(const gpio_config_t*),gpio_install_isr_service(int);
int gpio_isr_handler_add(int,void(*)(void*),void*);
int gpio_isr_handler_remove(int),gpio_intr_disable(int),gpio_get_level(int);
esp_err_t metalio_read(uint8_t,uint8_t,void*,size_t),metalio_write8(uint8_t,uint8_t,uint8_t);
'''
UNIT = r'''
#include "stub.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <setjmp.h>
#include "cst836u.h"
static jmp_buf waited;
static uint32_t wait_ticks;
static uint8_t raw[15];
static int read_error,write_error,irq_error,level=1,notifications,reads,resets,removed,disabled,deleted,mutexes;
static bool fail_task,fail_mutex;
static void(*handler)(void*);static void *handler_arg;
void test_log(const char*a,const char*b,...){(void)a;(void)b;}
SemaphoreHandle_t xSemaphoreCreateMutex(void){if(fail_mutex)return NULL;mutexes++;return calloc(1,sizeof(struct sem));}
int xSemaphoreTake(SemaphoreHandle_t s,uint32_t n){assert(n==portMAX_DELAY&&!s->locked);s->locked=true;return 1;}
int xSemaphoreGive(SemaphoreHandle_t s){assert(s->locked);s->locked=false;return 1;}
void vSemaphoreDelete(SemaphoreHandle_t s){assert(!s->locked);mutexes--;free(s);}
int xTaskCreate(void(*f)(void*),const char*n,unsigned stack,void*a,unsigned p,TaskHandle_t*out){assert(f&&n&&stack==3072&&p==5);if(fail_task)return 0;*out=a;return 1;}
void vTaskDelete(TaskHandle_t t){assert(t);deleted++;}
void xTaskNotifyGive(TaskHandle_t t){assert(t);notifications++;}
void vTaskNotifyGiveFromISR(TaskHandle_t t,BaseType_t*h){assert(t);notifications++;*h=0;}
uint32_t ulTaskNotifyTake(int clear,uint32_t ticks){assert(clear==pdTRUE);wait_ticks=ticks;longjmp(waited,1);}
int gpio_config(const gpio_config_t*c){assert(c->pin_bit_mask==(1ULL<<5)&&c->intr_type==GPIO_INTR_NEGEDGE);return 0;}
int gpio_install_isr_service(int flags){assert(!flags);return ESP_ERR_INVALID_STATE;}
int gpio_isr_handler_add(int pin,void(*f)(void*),void*a){assert(pin==5);if(irq_error)return irq_error;handler=f;handler_arg=a;return 0;}
int gpio_isr_handler_remove(int p){assert(p==5);removed++;handler=NULL;return 0;}
int gpio_intr_disable(int p){assert(p==5);disabled++;return 0;}
int gpio_get_level(int p){assert(p==5);return level;}
esp_err_t metalio_read(uint8_t a,uint8_t r,void*out,size_t n){assert(a==0x38);if(read_error)return read_error;
 if(r==0){assert(n==15);memcpy(out,raw,n);reads++;}else {assert(n==1);*(uint8_t*)out=r==0xa3?0x64:r==0x9f?0x26:2;}return 0;}
esp_err_t metalio_write8(uint8_t a,uint8_t r,uint8_t v){assert(a==0x38&&(r==0xa4||r==0xa5));assert(v==0||v==3);return write_error;}
static int reset_pin(void){resets++;return 0;}
#include "DRIVER"
static void once(cst836u_handle_t h){if(!setjmp(waited))feed(h);assert(!h->io_lock->locked);}
static void contact(unsigned count,unsigned event){memset(raw,0,15);raw[2]=count;raw[3]=event<<6|1;raw[4]=85;raw[5]=2;raw[6]=96;}
int main(void){
 cst836u_config_t cfg=CST836U_CONFIG_DEFAULT();cfg.int_gpio=5;cfg.reset_fn=reset_pin;
 cst836u_handle_t h=NULL;cst836u_touch_t out;
 fail_mutex=true;assert(cst836u_init(NULL,&cfg,&h)==ESP_ERR_NO_MEM&&!mutexes);fail_mutex=false;
 fail_task=true;assert(cst836u_init(NULL,&cfg,&h)==ESP_ERR_NO_MEM&&!mutexes&&!handler);fail_task=false;
 assert(cst836u_init(NULL,&cfg,&h)==0&&handler&&h->irq_ready);
 once(h);assert(reads==1&&wait_ticks==portMAX_DELAY); // Idle: no 100Hz shared-bus polling.
 handler(handler_arg);assert(notifications==1);
 contact(1,0);level=0;once(h);assert(wait_ticks==10);
 assert(cst836u_read(h,&out)==0&&out.touched&&out.x==341&&out.y==608);
 contact(1,2);raw[4]=90;once(h);assert(cst836u_read(h,&out)==0&&out.x==346);
 contact(0,0);level=1;once(h);assert(wait_ticks==portMAX_DELAY&&cst836u_read(h,&out)==0&&!out.touched);
 contact(1,0);once(h);contact(0,0);once(h);
 assert(cst836u_read(h,&out)==0&&out.touched);assert(cst836u_read(h,&out)==0&&!out.touched); // Missed short tap replay.
 read_error=7;once(h);assert(wait_ticks==10&&cst836u_read(h,&out)==7);read_error=0;
 contact(0,0);level=0;once(h);assert(wait_ticks==10);level=1;
 assert(cst836u_set_mode(h,CST836U_MODE_DEEPSLEEP)==0);int before=reads;
 once(h);assert(reads==before&&wait_ticks==portMAX_DELAY&&cst836u_get_mode(h)==CST836U_MODE_DEEPSLEEP);
 write_error=7;assert(cst836u_wake(h)==7&&cst836u_get_mode(h)==CST836U_MODE_DEEPSLEEP);write_error=0;
 assert(cst836u_wake(h)==0&&cst836u_get_mode(h)==CST836U_MODE_NORMAL);once(h);assert(reads==before+1);
 assert(cst836u_set_mode(h,99)==ESP_ERR_INVALID_ARG);
 assert(cst836u_deinit(h)==0&&!mutexes&&!handler&&disabled==removed);
 irq_error=7;assert(cst836u_init(NULL,&cfg,&h)==0&&!h->irq_ready);once(h);assert(wait_ticks==10);cst836u_deinit(h);assert(!mutexes);
 cfg.int_gpio=GPIO_NUM_NC;assert(cst836u_init(NULL,&cfg,&h)==0);once(h);assert(wait_ticks==10);cst836u_deinit(h);
 puts("PASS: actual FT6336U IRQ-idle/no polling, 10ms contact/low-IRQ/error retry, zero Y offset, short-tap replay, sleep/wake/error serialization, existing ISR service, IRQ fallback and allocation/task failure cleanup");
}
'''
with tempfile.TemporaryDirectory() as directory:
    p = Path(directory)
    (p/'stub.h').write_text(STUB)
    for name in ('driver/gpio.h','driver/i2c_master.h','esp_err.h','metalio_hw.h','freertos/FreeRTOS.h','freertos/task.h','freertos/semphr.h','esp_log.h'):
        f=p/name;f.parent.mkdir(parents=True,exist_ok=True);f.write_text('#include "stub.h"\n')
    (p/'test.c').write_text(UNIT.replace('DRIVER',str(ROOT/'components/cst836u/metalio_ft6336.c')))
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(p),'-I'+str(ROOT/'components/cst836u/include'),str(p/'test.c'),str(ROOT/'components/cst836u/metalio_touch_decode.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
