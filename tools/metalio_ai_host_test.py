#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Execute actual AI debounce, chooser raster/input, persistent setting and action dispatch."""
from pathlib import Path
import re,subprocess,tempfile
from home_cover_cache_host_test import function
r=Path(__file__).resolve().parents[1]
common=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
'''
sampler=common+r'''
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(x) ((void)(x))
#define portEXIT_CRITICAL(x) ((void)(x))
typedef int portMUX_TYPE;
enum {METALIO_AI_NONE,METALIO_AI_SHORT,METALIO_AI_LONG};
'''+re.sub(r'^#include.*\n','',(r/'components/metalio_hw/metalio_ai_key.c').read_text(),flags=re.M)+r'''
static void reset(void){memset(&ai,0,sizeof(ai));}
static void press(int ms){metalio_ai_sample(true,ms);metalio_ai_sample(true,ms+25);}
static void release(int ms){metalio_ai_sample(false,ms);metalio_ai_sample(false,ms+25);}
int main(void){
 reset();press(0);metalio_ai_sample(true,1000);release(1100);assert(!metalio_take_ai()); // boot-held suppresses
 reset();release(0);metalio_ai_sample(true,50);release(60);assert(!metalio_take_ai()); // bounce
 press(100);release(200);assert(metalio_take_ai()==METALIO_AI_SHORT&&!metalio_take_ai());
 press(300);metalio_ai_sample(true,824);assert(!metalio_take_ai());metalio_ai_sample(true,825);
 assert(metalio_take_ai()==METALIO_AI_LONG);metalio_ai_sample(true,2000);release(2100);assert(!metalio_take_ai());
 press(2200);metalio_ai_clear();metalio_ai_sample(true,3500);release(3600);assert(!metalio_take_ai());
 press(3700);release(3800);assert(metalio_take_ai()==METALIO_AI_SHORT);
 for(int i=0;i<12;i++){press(4000+i*100);release(4050+i*100);}assert(ai.count==8);
 for(int i=0;i<8;i++)assert(metalio_take_ai()==METALIO_AI_SHORT);assert(!metalio_take_ai());
 puts("PASS: GPIO61 AI debounce,boot/sleep-held suppression,one500ms long event,no release tap,bounded8-event FIFO");
}
'''
# The old chooser changed the AI tap mapping. Its replacement executes a
# configurable action list; keep the original sampler regression and run the
# actual replacement's raster/persistence/dispatch coverage.
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'sampler';p.with_suffix('.c').write_text(sampler)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(p.with_suffix('.c')),'-o',str(p)],check=True)
 subprocess.run([str(p)],check=True)
subprocess.run(['python3',str(r/'tools/action_menu_host_test.py')],check=True)
