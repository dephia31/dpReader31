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
static struct {ble_pt_frame_t frames[BLE_PT_MAX_BINDINGS];uint8_t actions[BLE_PT_MAX_BINDINGS];} s_bindings;
static ble_pt_frame_t s_last_ble_frame;
static ble_pt_event_t keys[32];static unsigned kh,kt;
static ble_pt_raw_t raws[32];static unsigned rh,rt;
static bool ble_pt_pop_key(ble_pt_event_t *out){if(kt==kh)return false;*out=keys[kt++];return true;}
static bool ble_pt_pop_raw(ble_pt_raw_t *out){if(rt==rh)return false;*out=raws[rt++];return true;}
static void key(uint8_t usage){keys[kh++]=(ble_pt_event_t){.usage=usage};}
static void raw(uint8_t value,uint32_t at,bool down){raws[rh++]=(ble_pt_raw_t){.frame={.len=1,.bytes={value}},.at_ms=at,.pressed=down};}
'''
# Public declarations in the actual header require non-static test implementations.
code=code.replace('static bool ble_pt_pop_', 'bool ble_pt_pop_')
for name in ('frame_same','ble_pt_binding_slot','ble_pt_has_bindings','ble_pt_action_for_usage','ble_pt_action_for_frame'):
    code+=function(ble,name)
code+=function(book,'book_remote_action')
code+=r'''
int main(void){
 s_bindings.actions[0]=BLE_PT_ACTION_PREV;
 s_bindings.frames[0]=(ble_pt_frame_t){.len=1,.bytes={0x91}};
 s_bindings.actions[1]=BLE_PT_ACTION_NEXT;
 s_bindings.frames[1]=(ble_pt_frame_t){.len=1,.bytes={0x92}};
 raw(0x92,1000,true);key(0x4f);
 assert(book_remote_action(1000)==BLE_PT_ACTION_NEXT);
 // HID follow-up arrives while E-Ink draws; 436ms later it must not turn again.
 key(0x4f);key(0x51);raw(0x92,1030,true);raw(0x92,1300,true);raw(0x92,1600,true);
 assert(book_remote_action(1800)==BLE_PT_ACTION_NONE&&kh==kt&&rh==rt);
 // A separate press still responds immediately, without waiting on the display clock.
 raw(0x92,2000,true);assert(book_remote_action(2000)==BLE_PT_ACTION_NEXT);
 raw(0x92,2030,false);assert(book_remote_action(2436)==BLE_PT_ACTION_NONE);
 raw(0x91,2500,true);assert(book_remote_action(2500)==BLE_PT_ACTION_PREV);
 s_bindings.frames[4]=(ble_pt_frame_t){.len=1,.bytes={0xa1}};
 s_bindings.frames[5]=(ble_pt_frame_t){.len=1,.bytes={0xa2}};
 s_bindings.actions[4]=s_bindings.actions[5]=BLE_PT_ACTION_LIGHT_OFF;
 raw(0xa1,2510,true);raw(0xa2,2520,true);
 assert(book_remote_action(2800)==BLE_PT_ACTION_LIGHT_OFF);
 assert(book_remote_action(2801)==BLE_PT_ACTION_LIGHT_OFF);
 // Built-in native frames cannot override a learned peer, including a partial binding.
 ble_pt_frame_t native={.len=1,.bytes={2}};
 assert(ble_pt_action_for_frame(&native)==BLE_PT_ACTION_NONE);
 s_bindings.frames[1].len=0;
 key(0x4f);raw(2,3000,true);assert(book_remote_action(3000)==BLE_PT_ACTION_NONE);
 raw(0x91,3100,true);assert(book_remote_action(3100)==BLE_PT_ACTION_PREV);
 // No learned bindings: preserve normal HID navigation and its existing repeat interval.
 memset(&s_bindings,0,sizeof(s_bindings));
 key(0x4f);assert(book_remote_action(4000)==BLE_PT_ACTION_NEXT);
 key(0x4f);assert(book_remote_action(4100)==BLE_PT_ACTION_NONE);
 key(0x4f);assert(book_remote_action(4500)==BLE_PT_ACTION_NEXT);
 // Arrival-time arithmetic remains correct over timer wrap.
 s_bindings.actions[1]=BLE_PT_ACTION_NEXT;
 s_bindings.frames[1]=(ble_pt_frame_t){.len=1,.bytes={0x92}};
 s_last_ble_learned_ms=UINT32_MAX-100;
 s_last_ble_frame=s_bindings.frames[1];
 raw(0x92,20,true);assert(book_remote_action(5000)==BLE_PT_ACTION_NONE);
 raw(0x92,500,true);assert(book_remote_action(5500)==BLE_PT_ACTION_NEXT);
 puts("PASS learned BLE: delayed HID duplicates, queued raw bursts, release, next/previous presses, partial mapping, default HID/repeat, timer wrap");
}
'''
with tempfile.TemporaryDirectory() as tmp:
    p=Path(tmp);(p/'test.c').write_text(code)
    (p/'esp_err.h').write_text('typedef int esp_err_t;\n')
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined','-I'+str(p),'-I'+str(root/'components/ble_page_turner/include'),str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
