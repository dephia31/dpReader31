"""中文：实际音量去抖和IRQ任务等待回归。/ English: Actual volume debounce and IRQ-task wait regression."""
from pathlib import Path
import subprocess,tempfile
from home_cover_cache_host_test import function
root=Path(__file__).resolve().parents[1]
unit=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <setjmp.h>
#define portENTER_CRITICAL(p) ((void)(p))
#define portEXIT_CRITICAL(p) ((void)(p))
static int volume_lock,key_lock;
static int volume_events[8];
static unsigned volume_head,volume_count;
static bool volume_sampling_pending;
'''
hardware=root/'components/metalio_hw/metalio_hw.c'
source=hardware.read_text()
unit+=source[source.index('static struct {uint16_t stable,candidate;'):source.index('void metalio_volume_sample(')]
for name in ('metalio_volume_sample','metalio_volume_sampling_pending','metalio_take_volume','metalio_volume_clear'):
 unit+=function(name,root/'components/metalio_hw/metalio_hw.c')+'\n'
unit+=r'''
typedef int TickType_t;
#define pdMS_TO_TICKS(n) (n)
#define portMAX_DELAY 99999
#define pdTRUE 1
#define READ_PICO_PMU_KEY_NONE 0
#define METALIO_AI_KEY_GPIO 61
static bool held,candidate,s_key_irq=true;
static int action,last_wait;
static jmp_buf end;
static void keys(void){}
static int gpio_get_level(int pin){assert(pin==61);return 1;}
static void ulTaskNotifyTake(int clear,int wait){assert(clear==1);last_wait=wait;longjmp(end,1);}
'''+function('key_feed',root/'components/read_pico_pmu/metalio_power_adapter.c')+r'''
static int wait_once(void){if(!setjmp(end))key_feed(NULL);return last_wait;}
int main(void){
 assert(wait_once()==portMAX_DELAY);
 int64_t now=100;
 metalio_volume_sample(0xffff,0);metalio_volume_sample(0xffff,30);
 for(int bit=8;bit<=9;++bit)for(int repeat=0;repeat<4;++repeat){
  uint16_t down=(uint16_t)(0xffffu&~(1u<<bit));int sign=bit==8?-1:1;
  metalio_volume_sample(down,now);assert(metalio_volume_sampling_pending());
  assert(wait_once()==10);assert(!metalio_take_volume());
  metalio_volume_sample(down,now+10);assert(!metalio_take_volume());
  metalio_volume_sample(down,now+30);assert(!metalio_take_volume());
  if(repeat&1){
   metalio_volume_sample(down,now+530);assert(metalio_take_volume()==sign*2);
   metalio_volume_sample(down,now+700);assert(!metalio_take_volume());assert(wait_once()==10);
  }
  int64_t end=now+((repeat&1)?710:80);
  metalio_volume_sample(0xffff,end);assert(wait_once()==10);
  metalio_volume_sample(0xffff,end+30);
  assert(metalio_take_volume()==((repeat&1)?0:sign));
  assert(!metalio_volume_sampling_pending());assert(wait_once()==portMAX_DELAY);now+=1000;
 }
 uint16_t down=0xffffu&~(1u<<8);
 metalio_volume_sample(down,now);metalio_volume_sample(down,now+30);metalio_volume_clear();
 metalio_volume_sample(down,now+900);assert(!metalio_take_volume());
 metalio_volume_sample(0xffff,now+910);metalio_volume_sample(0xffff,now+940);assert(!metalio_take_volume());
 // Both keys held together cancel either shortcut.
 uint16_t both=0xffffu&~((1u<<8)|(1u<<9));
 metalio_volume_sample(both,now+1000);metalio_volume_sample(both,now+1030);
 metalio_volume_sample(both,now+1700);assert(!metalio_take_volume());
 metalio_volume_sample(0xffff,now+1800);metalio_volume_sample(0xffff,now+1830);assert(!metalio_take_volume());
 puts("PASS: both volume keys: release-only shorts,one500ms hold,no short after hold,chord/clear cancellation,IRQ sampling through hold/debounce");
}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp)/'test.c';p.write_text(unit);binary=p.with_suffix('')
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror',str(p),'-o',str(binary)],check=True)
 subprocess.run([str(binary)],check=True)
