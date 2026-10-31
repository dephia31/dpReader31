#!/usr/bin/env python3
"""Compare real font placement functions' geometry visits against actual pixel positions.
SPDX-License-Identifier: Apache-2.0
"""
from pathlib import Path
import subprocess
from home_cover_cache_host_test import function
root=Path(__file__).resolve().parents[1]
font=root/'main/font/ttf_font.c'
unit=r'''
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef struct {int x,y,width,height;} EpdRect;
enum EpdFontFlags {EPD_DRAW_ALIGN_LEFT=0,EPD_DRAW_ALIGN_CENTER=1,EPD_DRAW_ALIGN_RIGHT=2};
typedef struct {const uint8_t *bitmap;int height,width,left,top,advance_x;} glyph_entry_t;
typedef void (*ttf_geometry_fn)(void*,size_t,size_t,EpdRect);
static ttf_geometry_fn s_geometry_fn;
static void *s_geometry_ctx;
static bool font_ready=true;
static int pixels[2048],count,visits,face;
static size_t last_end;
static uint8_t bitmap;
static int clamp_px(int px){return px;}
static void ttf_cover_lut_init(void){}
static void warm_runs(int px,const char *text){(void)px;(void)text;}
static void run_apply(size_t at){face=at>=5?2:1;}
static uint32_t decode_utf8(const char **text){const unsigned char *p=(const unsigned char *)*text;uint32_t c=*p++;unsigned n=c<128?0:c<224?1:c<240?2:3;if(n)c&=n==1?31:n==2?15:7;while(n--)c=(c<<6)|(*p++&63);*text=(const char*)p;return c;}
static const glyph_entry_t *get_glyph(uint32_t cp,int px){(void)px;static glyph_entry_t g;g=(glyph_entry_t){&bitmap,1,1,0,0,cp>=0x300&&cp<=0x36f?0:face*7};return &g;}
static int measure_width(int px,const char *text){int w=0;const char *p=text;while(*p){run_apply((size_t)(p-text));w+=get_glyph(decode_utf8(&p),px)->advance_x;}return w;}
static int ttf_ascender_px(int px){return px*3/4;}
static uint8_t glyph_alpha(const glyph_entry_t *g,int x,int y){(void)g;(void)x;(void)y;return 255;}
static uint8_t mix_glyph_ink(const glyph_entry_t *g,uint8_t a,uint8_t f,uint8_t b){(void)g;(void)a;(void)b;return f;}
static void epd_draw_pixel(int x,int y,uint8_t color,uint8_t *fb){(void)y;(void)color;(void)fb;assert(count<2048);pixels[count++]=x;}
'''
for name in ['ttf_geometry_visit','geometry_glyph','ttf_draw_text_px','ttf_draw_text_px_spaced','cjk_spacing_point','ttf_draw_text_px_fitted']:
    unit+=function(name,font)+'\n'
unit+=r'''
static void visit(void *ctx,size_t start,size_t bytes,EpdRect r){(void)ctx;assert(start==last_end);last_end=start+bytes;assert(visits<count);assert(r.x==pixels[visits++]);assert(r.width>=1);assert(r.y==100-ttf_ascender_px(24));}
int main(void){
 const char *texts[]={"Một đoạn tiếng Việt", "a\xcc\x81 hello", "中文 hello world", "abcdefghijklmnop"};
 for(unsigned t=0;t<sizeof(texts)/sizeof(texts[0]);t++)for(int mode=0;mode<3;mode++)for(int tracking=-2;tracking<=4;tracking+=3)for(int delta=-24;delta<=100;delta+=31){
  uint8_t fb=0;int width=measure_width(24,texts[t])+delta;
  count=visits=0;last_end=0;ttf_geometry_visit(NULL,NULL);
  if(mode==0)ttf_draw_text_px(&fb,36,100,24,texts[t],EPD_DRAW_ALIGN_LEFT,0,15);
  else if(mode==1)ttf_draw_text_px_spaced(&fb,36,100,24,texts[t],tracking,0,15);
  else ttf_draw_text_px_fitted(&fb,36,100,24,texts[t],tracking,width,0,15);
  int painted=count;ttf_geometry_visit(visit,NULL);
  if(mode==0)ttf_draw_text_px(&fb,36,100,24,texts[t],EPD_DRAW_ALIGN_LEFT,0,15);
  else if(mode==1)ttf_draw_text_px_spaced(&fb,36,100,24,texts[t],tracking,0,15);
  else ttf_draw_text_px_fitted(&fb,36,100,24,texts[t],tracking,width,0,15);
  assert(count==painted&&visits==painted&&last_end==strlen(texts[t]));
 }
 ttf_geometry_visit(NULL,NULL);puts("real font geometry matches rendered glyph positions: PASS");return 0;
}
'''
out=root/'build/notes-geometry';out.mkdir(parents=True,exist_ok=True)
(out/'test.c').write_text(unit)
subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(out/'test.c'),'-o',str(out/'test')],check=True)
subprocess.run([str(out/'test')],check=True)
