#!/usr/bin/env python3
"""Actual anti-fade deadline and due logic agree across blocking and clock regression."""
from pathlib import Path
import subprocess,tempfile
from home_cover_cache_host_test import function
r=Path(__file__).resolve().parents[1]
u='''#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
static unsigned minutes=5;static bool s_clock_ready;static int64_t s_last_refresh_ms;
static unsigned anti_fade_minutes(void){return minutes;}
'''+function('anti_fade_due',r/'main/anti_fade.c')+'\n'+function('anti_fade_next_ms',r/'main/anti_fade.c')+'''
int main(void){
 assert(anti_fade_next_ms(1000,1000)==1000);assert(!anti_fade_due(1000,1000,false));
 assert(anti_fade_next_ms(1001,1000)==301000);assert(!anti_fade_due(300999,1000,false));
 assert(anti_fade_due(301000,1000,false));assert(anti_fade_next_ms(301000,1000)==601000);
 assert(!anti_fade_due(400000,399000,true));assert(anti_fade_next_ms(400001,399000)==700000);
 assert(anti_fade_next_ms(399999,399000)==399999);assert(!anti_fade_due(399999,399000,false));
 minutes=0;assert(anti_fade_next_ms(500000,400000)==INT64_MAX);
 puts("PASS actual anti-fade deadline: exact due boundary, blocked deferral, disabled and monotonic regression agree with existing refresh policy");}
'''
with tempfile.TemporaryDirectory() as t:
 p=Path(t);(p/'test.c').write_text(u);subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(p/'test.c'),'-o',str(p/'test')],check=True);subprocess.run([str(p/'test')],check=True)
