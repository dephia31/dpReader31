#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
"""Replay queued BLE reports across slow display ticks using production dispatch/mapping."""
from pathlib import Path
import re, subprocess, tempfile
root = Path(__file__).resolve().parents[1]
def function(path, name):
    source = path.read_text()
    m = re.search(r'^(?:static )?[^\n]+\b'+name+r'\([^;{}]*\)\s*\{', source, re.M)
    assert m, name
    pos=m.end(); depth=1
    while depth:
        depth += (source[pos]=='{')-(source[pos]=='}'); pos+=1
    return source[m.start():pos]+'\n'
ble=root/'components/ble_page_turner/src/ble_page_turner.c'
book=root/'firmware/shared/main/apps/app_book.c'
code=r'''
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "ble_page_turner.h"
#define BLE_REMOTE_DEBOUNCE_MS 350
#define YXT_CODE_PREV 1
#define YXT_CODE_NEXT 2
static uint32_t s_last_ble_remote_turn_ms,s_last_ble_learned_ms;
static bool s_has_keyboard_page;
static struct {ble_pt_frame_t frames[2];} s_bindings;
static ble_pt_event_t keys[32];static unsigned kh,kt;
static ble_pt_raw_t raws[32];static unsigned rh,rt;
static bool ble_pt_pop_key(ble_pt_event_t *out){if(kt==kh)return false;*out=keys[kt++];return true;}
static bool ble_pt_pop_raw(ble_pt_raw_t *out){if(rt==rh)return false;*out=raws[rt++];return true;}
static void key(uint8_t usage){keys[kh++]=(ble_pt_event_t){.usage=usage};}
static void raw(uint8_t value,uint32_t at,bool down){raws[rh++]=(ble_pt_raw_t){.frame={.len=1,.bytes={value}},.at_ms=at,.pressed=down};}
'''
# Public declarations in the actual header require non-static test implementations.
code=code.replace('static bool ble_pt_pop_', 'bool ble_pt_pop_')
for name in ('frame_same','ble_pt_binding','ble_pt_action_for_usage','ble_pt_action_for_frame'):
    code+=function(ble,name)
code+=function(book,'book_remote_direction')
code+=r'''
int main(void){
 s_bindings.frames[0]=(ble_pt_frame_t){.len=1,.bytes={0x91}};
 s_bindings.frames[1]=(ble_pt_frame_t){.len=1,.bytes={0x92}};
 raw(0x92,1000,true);key(0x4f);
 assert(book_remote_direction(1000)==1);
 // HID follow-up arrives while E-Ink draws; 436ms later it must not turn again.
 key(0x4f);key(0x51);raw(0x92,1030,true);raw(0x92,1300,true);raw(0x92,1600,true);
 assert(book_remote_direction(1800)==0&&kh==kt&&rh==rt);
 // A separate press still responds immediately, without waiting on the display clock.
 raw(0x92,2000,true);assert(book_remote_direction(2000)==1);
 raw(0x92,2030,false);assert(book_remote_direction(2436)==0);
 raw(0x91,2500,true);assert(book_remote_direction(2500)==-1);
 // Built-in native frames cannot override a learned peer, including a partial binding.
 ble_pt_frame_t native={.len=1,.bytes={2}};
 assert(ble_pt_action_for_frame(&native)==BLE_PT_ACTION_NONE);
 s_bindings.frames[1].len=0;
 key(0x4f);raw(2,3000,true);assert(book_remote_direction(3000)==0);
 raw(0x91,3100,true);assert(book_remote_direction(3100)==-1);
 // No learned bindings: preserve normal HID navigation and its existing repeat interval.
 memset(&s_bindings,0,sizeof(s_bindings));
 key(0x4f);assert(book_remote_direction(4000)==1);
 key(0x4f);assert(book_remote_direction(4100)==0);
 key(0x4f);assert(book_remote_direction(4500)==1);
 // Arrival-time arithmetic remains correct over timer wrap.
 s_bindings.frames[1]=(ble_pt_frame_t){.len=1,.bytes={0x92}};
 s_last_ble_learned_ms=UINT32_MAX-100;
 raw(0x92,20,true);assert(book_remote_direction(5000)==0);
 raw(0x92,500,true);assert(book_remote_direction(5500)==1);
 puts("PASS learned BLE: delayed HID duplicates, queued raw bursts, release, next/previous presses, partial mapping, default HID/repeat, timer wrap");
}
'''
with tempfile.TemporaryDirectory() as tmp:
    p=Path(tmp);(p/'test.c').write_text(code)
    (p/'esp_err.h').write_text('typedef int esp_err_t;\n')
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined','-I'+str(p),'-I'+str(root/'components/ble_page_turner/include'),str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
