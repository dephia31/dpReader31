#!/usr/bin/env python3
"""Execute shared OTA/light row rectangles and check non-overlapping large controls."""
from pathlib import Path
import subprocess,tempfile
from home_cover_cache_host_test import function
root=Path(__file__).resolve().parents[1];source=root/'main/apps/app_device_settings.c'
unit='#include <assert.h>\n#include <stdbool.h>\n#include <stdio.h>\ntypedef struct{int x,y,width,height;}EpdRect;\n'
for f in ['upgrade_confirm_button','frontlight_row_rect']:unit+=function(f,source)+'\n'
unit+=r'''
int main(void){for(int online=0;online<2;++online){EpdRect a=upgrade_confirm_button(online,true),b=upgrade_confirm_button(online,false);assert(a.x==60&&a.width==564&&a.height==80&&b.width==564&&b.height==80);assert(b.y+b.height<a.y&&a.y+a.height<=1064);}
 int bottom=0;int order[]={0,1,2,4,5,3};for(int i=0;i<6;++i){EpdRect r=frontlight_row_rect(order[i]);assert(r.x==36&&r.width==612&&r.height==80&&r.y>=bottom&&r.y+r.height<=884);bottom=r.y+r.height;}
 puts("PASS real OTA confirm/cancel and frontlight rows: full-width80px controls, no overlap or footer intrusion");}
'''
with tempfile.TemporaryDirectory() as folder:
 p=Path(folder);(p/'test.c').write_text(unit);subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(p/'test.c'),'-o',str(p/'test')],check=True);subprocess.run([str(p/'test')],check=True)
