#!/usr/bin/env python3
"""Exercise actual UI glyph drawing and S31 output policy without a display."""
from pathlib import Path
import subprocess
import tempfile
from home_cover_cache_host_test import function
root = Path(__file__).resolve().parents[1]
unit = r'''
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
enum EpdFontFlags { EPD_DRAW_ALIGN_LEFT=0, EPD_DRAW_ALIGN_CENTER=1, EPD_DRAW_ALIGN_RIGHT=2 };
typedef struct {uint8_t *bitmap; int width,height,x0,y0,advance;} ui_glyph_t;
static uint8_t s_cover[256], pixel;
static ui_glyph_t sample={&pixel,1,1,0,0,1};
static bool ready(void){return true;}
static unsigned next_cp(const char **p){return *(*p)++;}
static ui_glyph_t *glyph(unsigned cp,int px){(void)cp;(void)px;return &sample;}
static int ui_font_text_width_px(int px,const char *text){(void)px;return strlen(text);}
static void epd_draw_pixel(int x,int y,uint8_t shade,uint8_t *fb){assert(x==0&&y==0);*fb=shade>>4;}
static bool s_system_binary,fast;
static bool app_settings_main_fast_refresh(void){return fast;}
'''
unit += function('draw_text_px', root/'main/font/ui_font.c') + '\n'
unit += function('display_system_binary_set', root/'boards/metalio47/src/main/display.c') + '\n'
unit += r'''
int main(void){
    for(int a=0;a<256;++a)s_cover[a]=a;
    for(int fg=0;fg<16;++fg)for(int bg=0;bg<16;++bg)for(int a=0;a<256;++a){
        pixel=a;uint8_t fb=bg;
        draw_text_px(&fb,0,0,24,"a",EPD_DRAW_ALIGN_LEFT,fg,bg,0);
        double ideal=(bg*(255-s_cover[a])+fg*s_cover[a])/255.0;
        assert(fabs(fb-ideal)<=0.5);
        for(int threshold=96;threshold<=128;threshold+=32){
            fb=bg;draw_text_px(&fb,0,0,24,"a",0,fg,bg,threshold);
            assert(fb==(a>=threshold?fg:bg));
        }
    }
    for(int mode=0;mode<3;++mode){
        fast=mode==1;s_system_binary=false;
        assert(!display_system_binary_set(true));assert(s_system_binary==fast);
        assert(display_system_binary_set(false)==fast);assert(!s_system_binary);
    }
    puts("PASS actual UI glyph output: all gray pairs/coverage, thresholds96/128; S31 normal/water retain AA, explicit Fast retains BW");
}
'''
with tempfile.TemporaryDirectory() as directory:
    tmp=Path(directory);(tmp/'test.c').write_text(unit)
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror',str(tmp/'test.c'),'-lm','-o',str(tmp/'test')],check=True)
    subprocess.run([str(tmp/'test')],check=True)
