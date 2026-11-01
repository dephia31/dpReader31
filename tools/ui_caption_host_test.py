#!/usr/bin/env python3
"""Exercise real caption fitting with long Vietnamese text, paths and large fonts."""
from pathlib import Path
import subprocess,tempfile
from home_cover_cache_host_test import function
root=Path(__file__).resolve().parents[1]
unit=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef struct{int x,y,width,height;}EpdRect;
enum EpdFontFlags{EPD_DRAW_ALIGN_LEFT,EPD_DRAW_ALIGN_CENTER,EPD_DRAW_ALIGN_RIGHT};
static int scale;static unsigned calls;static char painted[1024];static EpdRect bounds;
static int ui_text_effective_px(int px){return px*scale/100;}
static int ui_text_fixed_width_px(int px,const char*t){int n=0;for(;*t;++t)if(((unsigned char)*t&0xc0)!=0x80)++n;return n*px*3/5;}
static void ui_text_box(uint8_t*f,EpdRect b,int px,const char*t,enum EpdFontFlags a,bool i){
 (void)f;(void)px;(void)a;(void)i;
 assert(b.x>=bounds.x&&b.y>=bounds.y&&b.x+b.width<=bounds.x+bounds.width&&b.y+b.height<=bounds.y+bounds.height);
 assert(!((unsigned char)t[0]>=0x80&&(unsigned char)t[0]<0xc0));
 assert(strlen(painted)+strlen(t)<sizeof(painted));strcat(painted,t);++calls;
}
'''+function('ui_text_caption',root/'main/ui/ui_kit.c')+function('settings_log_preview',root/'main/apps/app_device_settings.c')+r'''
static void unspace(char *out,const char *in){for(;*in;++in)if(*in!=' ')*out++=*in;*out=0;}
int main(void){uint8_t fb=0;char a[1024],b[1024];
 const char *texts[]={"Xóa tất cả tệp trong thư mục hiện tại?","Bản sao lưu có chứa mật khẩu WiFi, hãy bảo quản thẻ nhớ cẩn thận.","/sdcard/Thư_mục_rất_dài/ảnh_đẹp/tên_tệp_rất_dài.epub","Ngắn"};
 for(scale=100;scale<=320;scale+=10)for(unsigned n=0;n<4;++n)for(int align=0;align<3;++align){
  bounds=(EpdRect){48,844,588,128};painted[0]=0;calls=0;
  ui_text_caption(&fb,bounds,22,texts[n],align,false);assert(calls>=1&&calls<=2);
  unspace(a,painted);unspace(b,texts[n]);assert(!strcmp(a,b));
 }
 for(scale=100;scale<=320;scale+=10){
  bounds=(EpdRect){52,630,580,210};painted[0]=0;calls=0;
  settings_log_preview(&fb,"E (15805) epd_lcd: phase=0 prefill=128 first_missing_row=638 reason=1 consumed=688/688\nW previous sleep state and very long diagnostic details continue here\nline3\nline4\nline5\nline6\nline7");
  assert(calls>=1&&calls<=6);
 }
 calls=0;ui_text_caption(&fb,(EpdRect){0,0,0,40},20,"test",0,false);assert(!calls);
 puts("PASS actual caption: 100..320%, UTF8 intact, entire text preserved, two bounded lines, long paths and all alignments");
}
'''
with tempfile.TemporaryDirectory() as folder:
 p=Path(folder);(p/'test.c').write_text(unit)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
