"""Execute actual shared-expander writes: simultaneous TPS rise, neighbor preservation and failures."""
from pathlib import Path
import subprocess,tempfile
from home_cover_cache_host_test import function
r=Path(__file__).resolve().parents[1]
u=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_ERR_INVALID_STATE -1
#define ESP_ERR_TIMEOUT -2
#define pdTRUE 1
#define pdMS_TO_TICKS(x) (x)
static int mutex=1,ioe=1,held,takes,gives,writes,fail_lock,fail_write;
static uint8_t outputs[2]={0xa0,0x14},physical[2]={0xa0,0x14};
static int xSemaphoreTake(int m,int ms){assert(m==1&&ms==100&&!held);++takes;if(fail_lock)return 0;held=1;return 1;}
static void xSemaphoreGive(int m){assert(m==1&&held);++gives;held=0;}
static int fca9555_set_output(int io,unsigned port,uint8_t next){assert(io==1&&port<2&&held);++writes;if(fail_write)return -3;physical[port]=next;return 0;}
'''
u+=function('metalio_io_mask',r/'components/metalio_hw/metalio_hw.c')+'\n'+function('metalio_io_set',r/'components/metalio_hw/metalio_hw.c')+r'''
int main(void){
 assert(metalio_io_mask(0,3,3)==0&&writes==1&&physical[0]==0xa3&&outputs[0]==0xa3&&physical[1]==0x14);
 assert(metalio_io_set(3,true)==0&&physical[0]==0xab&&outputs[0]==0xab);
 assert(metalio_io_set(12,false)==0&&physical[1]==4&&outputs[1]==4);
 fail_write=1;int prior=writes;assert(metalio_io_mask(0,3,0)==-3&&writes==prior+1&&physical[0]==0xab&&outputs[0]==0xab&&!held);fail_write=0;
 fail_lock=1;prior=writes;assert(metalio_io_mask(0,3,0)==ESP_ERR_TIMEOUT&&writes==prior&&!held&&outputs[0]==0xab);fail_lock=0;
 assert(metalio_io_set(16,true)==ESP_ERR_INVALID_STATE&&metalio_io_mask(2,3,0)==ESP_ERR_INVALID_STATE&&writes==prior);
 assert(takes==gives+1);puts("PASS: actual shared IO mask, one-write TPS rise, preserved motor/P05/P14 bits, lock timeout, failed write/cache and invalid pins");}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(u);subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-g',str(p/'test.c'),'-o',str(p/'test')],check=True);subprocess.run([str(p/'test')],check=True)
