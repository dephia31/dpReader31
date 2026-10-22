#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
"""中文：真实设置/文件绘制包装器不能再量化抗锯齿边缘。/ English: Actual system render wrappers retain antialiased edge pixels."""
from pathlib import Path
import subprocess,tempfile
from home_cover_cache_host_test import function
r=Path(__file__).resolve().parents[1]
for filename in ['app_device_settings.c','app_files.c']:
 code=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef struct {int root;}app_ctx_t;
static bool s_render_secondary;
static bool main_page_visible(app_ctx_t*c){return c->root;}
static void render_content(app_ctx_t*c,uint8_t*f){(void)c;for(int i=0;i<128;++i)f[i]=(uint8_t)i;}
static void settings_navigation(uint8_t*f){memset(f+124,0xee,4);}
'''
 code+=function('render',r/'main/apps'/filename)
 code+='\nint main(void){uint8_t fb[128];for(int root=0;root<2;++root){app_ctx_t c={root};render(&c,fb);for(int i=0;i<124;++i)assert(fb[i]==i);}puts("PASS: actual '+filename+' renderer retains all gray edge values in roots and children");}\n'
 with tempfile.TemporaryDirectory() as d:
  p=Path(d);(p/'test.c').write_text(code)
  subprocess.run(['cc','-std=c11','-DKIIKO_BOARD_METALIO47=0','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
  subprocess.run([str(p/'test')],check=True)
