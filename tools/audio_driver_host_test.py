#!/usr/bin/env python3
"""Execute actual output submission and cleanup under partial writes/faults."""
from pathlib import Path
import subprocess,tempfile
from home_cover_cache_host_test import function
R=Path(__file__).resolve().parents[1];s=R/'components/metalio_audio/metalio_audio.c'
unit=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdatomic.h>
#include <string.h>
#include <stdio.h>
typedef int esp_err_t;
#define ESP_OK 0
#define UART_NUM_1 1
static void *tx;static bool uart_ready,tx_enabled,pa,cleanup_failed;
static atomic_bool cancel;
static unsigned disables,deletes,uart_deletes,resets,writes;
static bool bad,error,zero,interrupt,fail_disable,fail_delete,fail_uart,fail_pa;
static const uint8_t *expected;static size_t total;
static int metalio_io_set(unsigned pin,bool on){assert(pin==4&&!on);if(fail_pa)return 1;pa=on;return ESP_OK;}
static int i2s_channel_disable(void *h){assert(h==tx);++disables;return fail_disable?1:ESP_OK;}
static int i2s_del_channel(void *h){assert(h==tx);++deletes;return fail_delete?1:ESP_OK;}
static int uart_driver_delete(int port){assert(port==1);++uart_deletes;return fail_uart?1:ESP_OK;}
static int gpio_reset_pin(int p){assert(p==38||p==39||p==42||p==43||p==44);++resets;return ESP_OK;}
static int i2s_channel_write(void *h,const void *data,size_t n,size_t *sent,unsigned wait){
 assert(h==tx&&wait==100&&n&&data==expected+total);++writes;
 *sent=zero?0:bad?n+1:n>8?8:n;if(!bad&&!zero)total+=*sent;
 if(interrupt)atomic_store(&cancel,true);
 return error?1:ESP_OK;
}
'''+function('release_output',s)+'\n'+function('write_slots',s)+r'''
int main(void){
 int32_t input[16]={0};tx=(void*)1;expected=(const uint8_t*)input;
 assert(write_slots(input,sizeof(input))&&writes==8&&total==sizeof(input));
 for(int scenario=0;scenario<4;++scenario){
  error=scenario==0;zero=scenario==1;bad=scenario==2;interrupt=scenario==3;total=0;writes=0;atomic_store(&cancel,false);
  assert(!write_slots(input,sizeof(input)));assert(writes==1);
 }
 pa=true;tx_enabled=uart_ready=true;release_output();assert(!tx&&!pa&&!uart_ready&&!tx_enabled&&disables==1&&deletes==1&&uart_deletes==1&&resets==5);
 release_output();assert(disables==1&&deletes==1&&uart_deletes==1&&resets==5);
 tx=(void*)2;tx_enabled=false;release_output();assert(disables==1&&deletes==2&&resets==8);
 tx=(void*)3;tx_enabled=true;uart_ready=true;pa=true;fail_disable=fail_uart=fail_pa=true;
 assert(!release_output()&&cleanup_failed&&tx&&tx_enabled&&uart_ready&&pa);
 fail_disable=fail_uart=fail_pa=false;fail_delete=true;
 assert(!release_output()&&cleanup_failed&&tx&&!tx_enabled&&!uart_ready&&!pa);
 fail_delete=false;assert(release_output()&&!cleanup_failed&&!tx);
 puts("PASS actual I2S: partialwrites, timeout/zero/invalidcount/cancel, PAoff, initialized/partial/idempotentcleanup, pinrelease/cleanupfail-retain-and-retry");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(unit)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(p/'test.c'),'-o',str(p/'test')],check=True);subprocess.run([str(p/'test')],check=True)
