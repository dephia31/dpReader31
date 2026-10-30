#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Real AI sampler plus real IRQ task wait: release must settle before sleeping."""
from pathlib import Path
import re,subprocess,tempfile
from home_cover_cache_host_test import function
R=Path(__file__).resolve().parents[1]
u=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <setjmp.h>
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(x) ((void)(x))
#define portEXIT_CRITICAL(x) ((void)(x))
typedef int portMUX_TYPE;typedef int TickType_t;
enum {METALIO_AI_NONE,METALIO_AI_SHORT,METALIO_AI_LONG};
#define METALIO_AI_KEY_GPIO 61
#define READ_PICO_PMU_KEY_NONE 0
#define portMAX_DELAY -1
#define pdMS_TO_TICKS(x) (x)
#define pdTRUE 1
'''+re.sub(r'^#include.*\n','',(R/'components/metalio_hw/metalio_ai_key.c').read_text(),flags=re.M)+r'''
static bool held,candidate,s_key_irq=true,pin_down;
static int action,key_lock,last_wait;static int64_t now;
static jmp_buf stop;
static int gpio_get_level(int pin){assert(pin==61);return !pin_down;}
static bool metalio_volume_sampling_pending(void){return false;}
static void keys(void){metalio_ai_sample(pin_down,now);}
static void ulTaskNotifyTake(int clear,int wait){assert(clear==1);last_wait=wait;longjmp(stop,1);}
'''+function('key_feed',R/'components/read_pico_pmu/metalio_power_adapter.c')+r'''
static int sample(bool down,int64_t time){pin_down=down;now=time;if(!setjmp(stop))key_feed(NULL);return last_wait;}
int main(void){
 assert(sample(false,0)==portMAX_DELAY);assert(sample(false,30)==portMAX_DELAY);
 bool light=true;unsigned longs=0,shorts=0;
 for(int i=0;i<12;++i){int64_t t=100+i*1000;
  assert(sample(true,t)==10);assert(sample(true,t+30)==10);assert(sample(true,t+529)==10);assert(!metalio_take_ai());
  sample(true,t+530);int e=metalio_take_ai();assert(e==METALIO_AI_LONG);++longs;light=!light;assert(light==(i%2!=0));
  sample(true,t+700);assert(!metalio_take_ai());
  assert(sample(false,t+710)==10); // physical release IRQ alone must NOT sleep indefinitely
  assert(sample(false,t+720)==10);assert(sample(false,t+740)==portMAX_DELAY);assert(!metalio_take_ai());
 }
 assert(longs==12&&light);
 sample(true,13000);sample(true,13030);sample(false,13100);sample(false,13130);assert(metalio_take_ai()==METALIO_AI_SHORT);++shorts;
 sample(true,14000);sample(true,14030);metalio_ai_clear();sample(true,14600);assert(!metalio_take_ai());assert(sample(false,14700)==10);sample(false,14730);assert(!metalio_take_ai());
 sample(true,15000);sample(true,15030);sample(true,15530);assert(metalio_take_ai()==METALIO_AI_LONG);sample(false,15600);sample(false,15630);assert(!metalio_take_ai());
 assert(shorts==1);puts("PASS actual AI sampler+IRQ wait:12 holds alternate light off/on, release debounce continues after GPIO high, no duplicate/short after hold, clear-held suppression and rearm");}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(u);subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(p/'test.c'),'-o',str(p/'test')],check=True);subprocess.run([str(p/'test')],check=True)
