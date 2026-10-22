#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
"""中文：真实高段滤波的时间/插拔/低电边界。/ English: Real upper-SOC timing, cable and low-battery boundaries."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
unit=r'''
#include "pico_soc_filter.h"
#include <assert.h>
#include <stdio.h>
int main(void){
 pico_soc_filter_t s={0};
 assert(pico_soc_filter_update(&s,1000,4200,true,true,false,0)==1000);
 assert(pico_soc_filter_update(&s,940,4080,false,false,false,1000)==1000);
 for(unsigned t=1100;t<31000;t+=100)assert(pico_soc_filter_update(&s,940,4080,false,false,false,t)==1000);
 unsigned prior=1000;
 for(unsigned t=31000;t<95000;t+=1000){unsigned v=pico_soc_filter_update(&s,940,4080,false,false,false,t);assert(v<=prior&&prior-v<=1);prior=v;}
 assert(prior==940);
 // 插入时的电压重新定基同样不能瞬间掉几个百分点。
 // Insertion rebasing must not drop several points immediately either.
 pico_soc_filter_t plug={0};pico_soc_filter_update(&plug,1000,4200,false,false,false,0);
 assert(pico_soc_filter_update(&plug,950,4100,true,false,false,1000)==1000);
 for(unsigned t=2000;t<31000;t+=1000)assert(pico_soc_filter_update(&plug,950,4100,true,false,false,t)==1000);
 // 快速轮询不能让电量下降更快；真正低压与低SOC不被平滑掩盖。
 // Poll frequency cannot accelerate descent; real low voltage/SOC bypass smoothing.
 pico_soc_filter_t a={0},b={0};
 pico_soc_filter_update(&a,1000,4200,false,false,false,0);b=a;
 for(unsigned t=100;t<=50000;t+=100)pico_soc_filter_update(&a,950,4100,false,false,false,t);
 for(unsigned t=1000;t<=50000;t+=1000)pico_soc_filter_update(&b,950,4100,false,false,false,t);
 assert(a.shown==b.shown&&a.shown>=970);
 assert(pico_soc_filter_update(&a,100,3400,false,false,true,50100)==100);
 assert(pico_soc_filter_update(&b,600,3750,false,false,false,51000)==600);
 // 真满才直接确认满；一次尖峰不会变成永久值，断线/时钟回绕重新定基。
 // Only confirmed full promotes directly; spikes do not persist, and gaps/clock rollover rebase.
 assert(pico_soc_filter_update(&a,1000,4200,true,true,false,52000)==1000);
 assert(pico_soc_filter_update(&a,940,4080,false,false,false,53000)==1000);
 assert(pico_soc_filter_update(&a,940,4080,false,false,false,120000)==940);
 pico_soc_filter_update(&a,1000,4200,true,true,false,121000);
 pico_soc_filter_update(&a,950,4100,false,false,false,122000);
 for(unsigned t=123000;t<=170000;t+=1000)pico_soc_filter_update(&a,t==150000?900:950,4100,false,false,false,t);
 assert(a.shown>=980&&a.shown<=1000);
 assert(pico_soc_filter_update(&a,650,3800,false,false,false,171000)==650);
 s=(pico_soc_filter_t){0};assert(pico_soc_filter_update(&s,870,4000,false,false,false,0xfffffff0)==870);
 assert(pico_soc_filter_update(&s,860,4000,false,false,false,1010)<=870);
 puts("PASS: real upper-SOC unplug plateau, median outliers, bounded time-rate/poll independence, true-full gating, low voltage/SOC immediate, outage/reset/uint32-wrap rebase");
}
'''
with tempfile.TemporaryDirectory() as directory:
 p=Path(directory);(p/'test.c').write_text(unit)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(root/'components/read_pico_pmu/include'),str(p/'test.c'),str(root/'components/read_pico_pmu/pico_soc_filter.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
