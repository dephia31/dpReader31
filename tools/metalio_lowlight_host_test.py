#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
"""低亮度调节范围和旧值逆映射。/ Low-light travel and inverse mapping of existing levels."""
from pathlib import Path
import subprocess,tempfile
R=Path(__file__).resolve().parents[1]
u=r'''
#include <assert.h>
#include <stdio.h>
#define UI_QUICK_SLIDER_RANGE 240
#include "ui_quick_brightness.h"
int main(void){
 assert(ui_quick_brightness_value(0)==0&&ui_quick_brightness_value(120)==48&&ui_quick_brightness_value(240)==240);
 assert(ui_quick_brightness_value(999)==240&&ui_quick_brightness_position(999)==240);
 unsigned last=0;for(unsigned p=0;p<=240;++p){unsigned v=ui_quick_brightness_value(p);assert(v>=last&&v<=240);last=v;}
 last=0;for(unsigned v=0;v<=240;++v){unsigned p=ui_quick_brightness_position(v);assert(p>=last&&p<=240);last=p;unsigned back=ui_quick_brightness_value(p);assert((back>v?back-v:v-back)<=1);if(v<=48)assert(back==v);}
 for(unsigned v=1;v<=48;++v){unsigned a=ui_quick_brightness_position(v-1),b=ui_quick_brightness_position(v);assert(b-a>=2&&b-a<=3);}
 puts("PASS: half-track 0..20% low light, monotonic/clamped full range, exact legacy low-level inverse, bounded high-level rounding and 2..3 pixels per low-light step");}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp);(p/'test.c').write_text(u)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(R/'main/ui'),str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
