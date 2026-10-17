"""Check actual Metalio SC7A20H reference register writes and readback failures."""
from pathlib import Path
import re, subprocess, tempfile
root=Path(__file__).resolve().parents[1]
src=(root/'components/sc7a20h/sc7a20h.c').read_text()
unit=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_STATE 2
#define ESP_ERR_INVALID_RESPONSE 3
typedef int esp_err_t;
typedef struct {void *dev;uint8_t ctrl3,ctrl5;bool fifo_8bit,powered;}sensor_t;
typedef sensor_t *sc7a20h_handle_t;
#define SC7A20H_REG_CTRL1 0x20
#define SC7A20H_REG_CTRL3 0x22
#define SC7A20H_REG_CTRL5 0x24
#define SC7A20H_REG_CTRL6 0x25
#define SC7A20H_REG_AOI1_CFG 0x30
#define SC7A20H_REG_AOI1_SRC 0x31
static uint8_t registers[256];static int fault_write=-1,fault_read=-1,corrupt=-1,reads;
static esp_err_t write_reg(sc7a20h_handle_t h,uint8_t reg,uint8_t value){assert(h);if(reg==fault_write)return ESP_FAIL;registers[reg]=value;return ESP_OK;}
static esp_err_t read_regs(sc7a20h_handle_t h,uint8_t reg,uint8_t *out,size_t len){assert(h&&len==1);reads++;if(reg==fault_read)return ESP_FAIL;*out=reg==corrupt?0xff:registers[reg];return ESP_OK;}
'''
for name in ('sc7a20h_shared_irq_polling','sc7a20h_shared_irq_idle'):
    m=re.search(r'^esp_err_t '+name+r'\([^;{}]*\)\s*\{',src,re.M);a=m.end();depth=1
    while depth:
        depth+=(src[a]=='{')-(src[a]=='}');a+=1
    unit+=src[m.start():a]+'\n'
unit+=r'''
int main(void){
 sensor_t sensor={.dev=(void*)1,.ctrl3=255,.ctrl5=255,.fifo_8bit=true,.powered=true};
 assert(sc7a20h_shared_irq_polling(&sensor)==ESP_OK&&reads==5&&!sensor.ctrl3&&!sensor.ctrl5&&!sensor.fifo_8bit);
 assert(!registers[0x22]&&!registers[0x30]&&!registers[0x24]&&registers[0x25]==3);
 assert(sc7a20h_shared_irq_idle(&sensor)==ESP_OK&&registers[0x20]==0x10&&!sensor.powered);
 const uint8_t checked[]={0x22,0x30,0x24,0x25};
 for(unsigned i=0;i<sizeof(checked);i++){
  fault_write=checked[i];assert(sc7a20h_shared_irq_polling(&sensor)==ESP_FAIL);fault_write=-1;
  fault_read=checked[i];assert(sc7a20h_shared_irq_polling(&sensor)==ESP_FAIL);fault_read=-1;
  corrupt=checked[i];assert(sc7a20h_shared_irq_polling(&sensor)==ESP_ERR_INVALID_RESPONSE);corrupt=-1;
 }
 fault_read=0x31;assert(sc7a20h_shared_irq_polling(&sensor)==ESP_FAIL);fault_read=-1;
 sensor.powered=true;fault_write=0x20;assert(sc7a20h_shared_irq_idle(&sensor)==ESP_FAIL&&sensor.powered);
 assert(sc7a20h_shared_irq_polling(NULL)==ESP_ERR_INVALID_STATE);
 puts("PASS: actual S31 reference polling/idle registers, open-drain low-active GPIO2, readback and all write/read failures");
}
'''
with tempfile.TemporaryDirectory() as d:
    p=Path(d);(p/'test.c').write_text(unit)
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-g','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
