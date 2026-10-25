#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
"""Compile the real double-click detector and Pico FIFO consumer, including delayed consumption."""
from pathlib import Path
import subprocess
import tempfile
from home_cover_cache_host_test import function

ROOT = Path(__file__).resolve().parents[1]
unit = (ROOT/'components/read_pico_pmu/include/power_double_click.h').read_text().replace('#pragma once', '')
unit += r'''
#include <assert.h>
#include <stdio.h>
#define PMU_EVENT_FIFO_DEPTH 16
#define ESP_OK 0
#define ESP_LOGI(...) ((void)0)
enum {PMU_EVT_KEY_DOWN=1,PMU_EVT_KEY_SHORT=2,PMU_EVT_KEY_LONG=3};
static power_double_click_t s_wake_click;
static uint32_t s_lock_key_serial;
static bool s_scan_short,s_boot_short;
static struct {bool event_ok;unsigned pending_events;struct {unsigned type,event_id,timestamp_ms;}event;} s_snap;
static typeof(s_snap.event) events[16];static unsigned count,index_at;static bool ack_error;
static int read_pico_pmu_poll(void){s_snap.event_ok=true;s_snap.pending_events=count-index_at;if(index_at<count)s_snap.event=events[index_at];return 0;}
static int read_pico_pmu_event_ack(unsigned id){assert(id==events[index_at].event_id);if(ack_error)return -1;++index_at;return 0;}
static void add(unsigned type,unsigned ms){assert(count<16);events[count]=(typeof(events[0])){type,count+1,ms};++count;}
static void reset(void){s_wake_click=(power_double_click_t){0};count=index_at=0;s_scan_short=s_boot_short=ack_error=false;}
'''
unit += function('read_pico_pmu_take_key_wakeup', ROOT/'components/read_pico_pmu/read_pico_pmu.c')
unit += function('read_pico_pmu_take_key_wakeup_single', ROOT/'components/read_pico_pmu/read_pico_pmu.c')
unit += function('read_pico_pmu_take_boot_short_press', ROOT/'components/read_pico_pmu/read_pico_pmu.c')
unit += r'''
int main(void){
 reset();add(PMU_EVT_KEY_DOWN,100);assert(!read_pico_pmu_take_key_wakeup_single());
 add(PMU_EVT_KEY_SHORT,200);assert(read_pico_pmu_take_key_wakeup_single());assert(!read_pico_pmu_take_key_wakeup_single());
 reset();add(PMU_EVT_KEY_LONG,200);assert(read_pico_pmu_take_key_wakeup_single());
 reset();add(PMU_EVT_KEY_SHORT,100);add(PMU_EVT_KEY_LONG,200);assert(read_pico_pmu_take_key_wakeup_single());
 reset();add(PMU_EVT_KEY_SHORT,200);ack_error=true;assert(!read_pico_pmu_take_key_wakeup_single());
 ack_error=false;assert(read_pico_pmu_take_key_wakeup_single());
 reset();assert(!read_pico_pmu_take_boot_short_press());s_boot_short=true;
 assert(read_pico_pmu_take_boot_short_press()&&!read_pico_pmu_take_boot_short_press());
 // 单击开机认长按：按住稍久被记成长按，也仍是开机那一次按键。
 // One-press boot accepts a long hold: the PMU calls it long, yet it is still the booting press.
 reset();add(PMU_EVT_KEY_LONG,200);assert(!read_pico_pmu_take_key_wakeup());assert(s_scan_short);
 reset();add(PMU_EVT_KEY_DOWN,100);add(PMU_EVT_KEY_SHORT,180);assert(!read_pico_pmu_take_key_wakeup());
 add(PMU_EVT_KEY_DOWN,250);add(PMU_EVT_KEY_SHORT,320);assert(read_pico_pmu_take_key_wakeup());assert(!read_pico_pmu_take_key_wakeup());
 // UI解码阻塞期间两个短按排队，使用PMU时间而不是消费时间。/ Queued clicks use PMU event time, not UI consumption time.
 reset();add(PMU_EVT_KEY_SHORT,100);add(PMU_EVT_KEY_SHORT,500);assert(read_pico_pmu_take_key_wakeup());
 reset();add(PMU_EVT_KEY_SHORT,100);add(PMU_EVT_KEY_SHORT,701);assert(!read_pico_pmu_take_key_wakeup());add(PMU_EVT_KEY_SHORT,900);assert(read_pico_pmu_take_key_wakeup());
 reset();add(PMU_EVT_KEY_SHORT,100);add(PMU_EVT_KEY_LONG,400);add(PMU_EVT_KEY_SHORT,600);assert(!read_pico_pmu_take_key_wakeup());
 reset();add(PMU_EVT_KEY_SHORT,100);add(PMU_EVT_KEY_SHORT,120);assert(!read_pico_pmu_take_key_wakeup());
 reset();add(PMU_EVT_KEY_SHORT,UINT32_MAX-100);add(PMU_EVT_KEY_SHORT,100);assert(read_pico_pmu_take_key_wakeup());
 for(unsigned gap=59;gap<=601;++gap){power_double_click_t state={0};power_double_click_push(&state,1000);power_double_click_push(&state,1000+gap);assert(power_double_click_take(&state)==(gap>=60&&gap<=600));assert(!power_double_click_take(&state));}
 puts("PASS: Pico real FIFO double-click, DOWN/bounce/hold rejection, delayed queue, 60..600ms boundaries, wraparound and single consumption");
}
'''
with tempfile.TemporaryDirectory() as folder:
    p=Path(folder)
    (p/'test.c').write_text(unit)
    subprocess.run(['cc','-std=gnu11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
