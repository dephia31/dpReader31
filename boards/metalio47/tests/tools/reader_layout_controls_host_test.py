#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
# 中文：真实边距输入/几何与扩展缩进编码，覆盖不同字体和旧配置。
# English: Actual margin input/geometry and extended indent encoding, across font sizes and legacy configurations.
from pathlib import Path
import os,re
import subprocess,tempfile
from home_cover_cache_host_test import function
r=Path(__file__).resolve().parents[1]
f=r/'main/apps/app_book.c'
u=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#define UI_LOCK_WIDTH 684
#define UI_LOCK_HEIGHT 1216
#define UI_NAV_READER_STATUS_HEIGHT 54
#define APP_BOOK_PROGRESS_STYLE_BOTTOM 4
#define BOOK_MARGIN_MIN 0
#define BOOK_MARGIN_MAX 48
#define BOOK_MARGIN_CHOICES 9
typedef struct {int x,y,width,height;} EpdRect;
static bool vertical, status, footer, chapter;
static int progress_style;
static int s_status_layout_override=-1, s_footer_layout_override=-1;
static bool app_settings_book_status_enabled(void){return status;}
static bool app_settings_book_footer_enabled(void){return footer;}
static bool app_settings_book_footer_chapter(void){return chapter;}
static int app_settings_book_progress_style(void){return progress_style;}
static int s_margin;
static bool app_settings_book_vertical(void){return vertical;}
static int slider_index(EpdRect r,unsigned x,int n){int p=(int)x-r.x;if(p<0)p=0;if(p>=r.width)p=r.width-1;return(p*(n-1)+(r.width-1)/2)/(r.width-1);}
static unsigned s_book_indent_adjust=20;
static void nvs_put_u8(const char *key,uint8_t v){(void)key;assert(v==s_book_indent_adjust);}
#define NVS_KEY_BOOK_INDENT_ADJUST "bk_indent_adj"
'''
for name in ('READER_FOOTER_HEIGHT','READER_FOOTER_COMPACT_HEIGHT','READER_FOOTER_BOTTOM_BAR_HEIGHT'):
    u+=re.search(r'^#define '+name+r' .*',f.read_text(),re.M)[0]+'\n'
for n in ('reader_footer_height','reader_footer_top','reader_content_top','reader_content_bottom',
          'body_rect_for_tracking','reader_margin_width','reader_margin_levels',
          'reader_margin_level_for','reader_margin_for_level','reader_margin_input'):
    u+=function(n,f)+'\n'
for n in ('app_settings_book_indent_adjust','app_settings_set_book_indent_adjust'):
    u+=function(n,r/'main/settings.c')+'\n'
u+=r'''
int main(void){
 for(int px=36;px<=72;++px)for(int tr=-4;tr<=4;tr+=2){
  int widths[9],values[9];assert(reader_margin_levels(px,tr,widths,values)==9);
  for(int i=0;i<9;++i){assert(values[i]==i*6&&widths[i]==684-i*12);assert(reader_margin_level_for(i*6,px,tr)==i);}
  for(int i=0;i<=48;++i)assert(reader_margin_for_level(reader_margin_level_for(i,px,tr),i,px,tr)==i);
 }
 const int footer_heights[]={2,54,92,66};
 for(int bar=0;bar<2;++bar)for(int mode=0;mode<4;++mode)for(int v=0;v<2;++v){
  status=bar;footer=mode!=0;chapter=mode==2;vertical=v;
  progress_style=mode==3?APP_BOOK_PROGRESS_STYLE_BOTTOM:0;
  int top=bar?60:2;
  int bottom=1216-footer_heights[mode]-2;
  for(int margin=0;margin<=48;++margin){
   s_margin=margin;EpdRect a=body_rect_for_tracking(2);
   if(v){assert(a.x==24&&a.width==636);assert(a.y==top+margin&&a.height==bottom-top-2*margin);}
   else {assert(a.x==margin&&a.width==684-2*margin);assert(a.y==top&&a.height==bottom-top);}
  }
 }
 for(int i=-100;i<=100;++i){app_settings_set_book_indent_adjust(i);assert(app_settings_book_indent_adjust()==i&&s_book_indent_adjust<=200);}
 for(int old=0;old<=40;++old){s_book_indent_adjust=old;assert(app_settings_book_indent_adjust()==old-20);}
 puts("PASS: physical margins across both orientations and status/footer layouts, preserved legacy margin values, all 201 indent adjustments and legacy encoding");
}
'''
# 中文：默认启用；不支持 sanitizer 的主机可显式关闭。/ English: Enabled by default; unsupported hosts may opt out.
sanitizers = [] if os.environ.get('HOST_TEST_SANITIZERS') == '0' else ['-fsanitize=address,undefined']
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.c';p.write_text(u);out=p.with_suffix('')
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function',*sanitizers,str(p),'-o',str(out)],check=True)
 subprocess.run([str(out)],check=True)
