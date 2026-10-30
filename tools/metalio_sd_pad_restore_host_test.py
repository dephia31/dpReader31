#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Exercise S31 SD restore from retained sleep pulls, before either SD bus width."""
from pathlib import Path
import subprocess,tempfile
from home_cover_cache_host_test import function
R=Path(__file__).resolve().parents[1]
fields=('clk','cmd','data0','data1','data2','data3')
unit='''#include <assert.h>
#include <stdio.h>
static int releases[6];
static void gpio_hold_dis(int pin){assert(pin>=20&&pin<=25);++releases[pin-20];}
static struct {
 struct {int sdio_pad_pin_ctrl_ded_sel;}ctrl;
'''
for f in fields:unit+=f' struct {{int sdio_{f}_fun_wpd,sdio_{f}_fun_wpu;}}sdio_{f};\n'
unit+='}CNNT_PAD_CTRL;\n'+function('metalio_sd_release_hold',R/'components/metalio_hw/metalio_hw.c')+'\nint main(void){\n'
unit+='for(int selected=0;selected<=1;++selected){CNNT_PAD_CTRL.ctrl.sdio_pad_pin_ctrl_ded_sel=selected;\n'
for f in fields:unit+=f'CNNT_PAD_CTRL.sdio_{f}.sdio_{f}_fun_wpd=1;CNNT_PAD_CTRL.sdio_{f}.sdio_{f}_fun_wpu=0;\n'
unit+='metalio_sd_release_hold();assert(CNNT_PAD_CTRL.ctrl.sdio_pad_pin_ctrl_ded_sel==selected);\n'
for f in fields:unit+=f'assert(CNNT_PAD_CTRL.sdio_{f}.sdio_{f}_fun_wpd==0&&CNNT_PAD_CTRL.sdio_{f}.sdio_{f}_fun_wpu=={0 if f=="clk" else 1});\n'
unit+='metalio_sd_release_hold();}\nfor(int i=0;i<6;++i)assert(releases[i]==4);puts("PASS actual S31 SD restore: six holds released, retained CNNT sleep pulls cleared, CMD/D0-D3 high-idle pulls restored, CLK neutral, dedicated selection unchanged, repeat safe");}\n'
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(unit)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
