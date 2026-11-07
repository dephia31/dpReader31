#!/usr/bin/env python3
"""Production BLE queues retain every accepted payload when wake reasons coalesce."""
from pathlib import Path
import tempfile,subprocess
from home_cover_cache_host_test import function
r=Path(__file__).resolve().parents[1];src=r/'components/ble_page_turner/src/ble_page_turner.c'
u=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "ble_page_turner.h"
#define KEY_RING_LEN 16
#define RAW_RING_LEN 17
#define METALIO_UI_WAKE_BLE 4
#define BLE_PT_CRUMB_KEY_PUSH 1
#define BLE_PT_CRUMB_POP 2
#define portENTER_CRITICAL(p) ((void)(p))
#define portEXIT_CRITICAL(p) ((void)(p))
static int s_ring_lock,wakes;static uint32_t s_input_serial;
static struct{ble_pt_event_t items[16];uint8_t head,tail;}s_keys;
static struct{ble_pt_raw_t items[17];uint8_t head,tail;}s_raws;
void ble_pt_crumb(uint8_t n){(void)n;}
static int64_t esp_timer_get_time(void){return 1000000;}
static void metalio_ui_wake_post(unsigned bits){assert(bits==4);++wakes;}
'''
for n in ['key_push','raw_push','ble_pt_pop_key','ble_pt_pop_raw','ble_pt_discard_input']:u+=function(n,src)+'\n'
u+=r'''
int main(void){
 for(int i=0;i<15;i++){ble_pt_event_t e={0};e.usage=i;key_push(&e);}assert(wakes==15);
 ble_pt_event_t e={0};key_push(&e);assert(wakes==15); // Full queue changes no accepted payload.
 for(int i=0;i<15;i++){assert(ble_pt_pop_key(&e)&&e.usage==i);}assert(!ble_pt_pop_key(&e));
 ble_pt_frame_t frame={.len=1,.bytes={1}};for(int i=0;i<8;i++){raw_push(true,&frame);raw_push(false,&frame);}assert(wakes==31);
 raw_push(true,&frame);assert(wakes==31);
 ble_pt_raw_t raw;for(int i=0;i<16;i++){assert(ble_pt_pop_raw(&raw)&&raw.pressed==(i%2==0)&&raw.frame.bytes[0]==1);}assert(!ble_pt_pop_raw(&raw));
 e.usage=42;key_push(&e);raw_push(true,&frame);unsigned serial=s_input_serial;
 ble_pt_discard_input();assert(!ble_pt_pop_key(&e)&&!ble_pt_pop_raw(&raw)&&s_input_serial==serial);
 e.usage=43;key_push(&e);raw_push(true,&frame);
 assert(ble_pt_pop_key(&e)&&e.usage==43&&ble_pt_pop_raw(&raw)&&raw.pressed);
 puts("PASS actual BLE wake producers:15 ordered keys,8 complete press/release pairs, bounded-full queues do not post phantom payloads");}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'esp_err.h').write_text('#pragma once\ntypedef int esp_err_t;\n');(p/'test.c').write_text(u)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined','-I'+str(p),'-I'+str(r/'components/ble_page_turner/include'),str(p/'test.c'),'-o',str(p/'test')],check=True);subprocess.run([str(p/'test')],check=True)
