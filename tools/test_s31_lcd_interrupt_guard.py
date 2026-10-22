#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-3.0-or-later
"""Exercise the production VSYNC guard with stopped, preparing, active and repeated interrupts."""
from pathlib import Path
import re,subprocess,tempfile
from home_cover_cache_host_test import function
r=Path(__file__).resolve().parents[1]
s=(r/'boards/metalio47/src/components/epdiy/src/output_lcd/lcd_driver.c').read_text()
s=re.sub(r'__attribute__\(\(optimize\("O3"\)\)\)', '',s)
s='\n'.join(line.lstrip() for line in s.splitlines())
prefix=r'''
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include <stdio.h>
#define IRAM_ATTR
#define LINE_BATCH 1000
#define LCD_LL_EVENT_VSYNC_END 1
#define portYIELD_FROM_ISR() ((void)0)
static struct {bool frame_finishing;int64_t frame_t0_us;int display_lines;unsigned batches;int ckv_period_us;struct {void*dev;}hal;}lcd;
static unsigned irq,cleared,finished,started;
static unsigned lcd_ll_get_interrupt_status(void*d){(void)d;return irq;}
static void lcd_ll_clear_interrupt_status(void*d,unsigned x){(void)d;cleared|=x;irq&=~x;}
static void lcd_finish_frame_from_isr(void){++finished;lcd.frame_finishing=true;}
static void lcd_ll_enable_auto_next_frame(void*d,bool b){(void)d;(void)b;}
static void lcd_ll_set_vertical_timing(void*d,int a,int b,int c,int e){(void)d;(void)a;(void)b;(void)c;(void)e;}
static void lcd_ll_start(void*d){(void)d;++started;}
static void esp_rom_delay_us(int x){(void)x;}
static void start_ckv_cycles(int x){assert(x>0);}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'normalized.c').write_text(s)
 src=prefix+function('lcd_isr_vsync',p/'normalized.c')+r'''
int main(void){
 lcd.display_lines=688;
 for(int stopping=0;stopping<2;++stopping){lcd.frame_finishing=stopping;lcd.frame_t0_us=stopping?10:0;lcd.batches=0;irq=1;cleared=0;lcd_isr_vsync(NULL);assert(!irq&&cleared==1&&!finished&&!started&&!lcd.batches);}
 lcd.frame_finishing=false;lcd.frame_t0_us=10;irq=1;lcd_isr_vsync(NULL);assert(finished==1&&lcd.batches==1);
 irq=1;lcd_isr_vsync(NULL);assert(finished==1&&lcd.batches==1&&!irq);
 puts("PASS: production VSYNC ignores stale startup/stopped IRQs, clears status, completes one active frame and suppresses repeated completion");
}
'''
 (p/'test.c').write_text(src)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-Wno-sign-compare','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
