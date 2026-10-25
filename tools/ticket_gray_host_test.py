#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
"""真实票根两处封面绘制保留16级灰，不把已量化的白色再次点阵化。"""
from pathlib import Path
import subprocess,tempfile
from home_cover_cache_host_test import function
ROOT=Path(__file__).resolve().parents[1]
unit=r'''
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define BOOK_COVER_W 176
#define BOOK_COVER_H 240
#define UI_LOCK_WIDTH 684
#define UI_LOCK_HEIGHT 1216
#define UI_GRAY_LIGHT 0xe0
#define UI_GRAY_BLACK 0
#define EPD_DRAW_ALIGN_CENTER 0
typedef struct {int x,y,width,height;} EpdRect;
typedef struct {int x,y,width,height;} book_crop_t;
static uint8_t fb[684*1216];static int contrast=100;
static int app_settings_system_contrast(void){return contrast;}
static void epd_draw_pixel(int x,int y,uint8_t v,uint8_t *dst){assert(x>=0&&x<684&&y>=0&&y<1216);dst[y*684+x]=v>>4;}
static void ui_fill_rect_fast(EpdRect r,uint8_t v,uint8_t *dst){for(int y=r.y;y<r.y+r.height;y++)for(int x=r.x;x<r.x+r.width;x++)epd_draw_pixel(x,y,v,dst);}
static void ui_draw_control_frame(uint8_t *dst,EpdRect r,int radius,int color){(void)dst;(void)r;(void)radius;(void)color;}
static void ui_text_vc(uint8_t *dst,int x,int y,int px,const char *t,int a,bool inv){(void)dst;(void)x;(void)y;(void)px;(void)t;(void)a;(void)inv;}
static void text_fit(char *t,int px,int w){(void)t;(void)px;(void)w;}
static void ui_draw_round_rect(uint8_t *dst,EpdRect r,int radius,uint8_t v){(void)dst;(void)r;(void)radius;(void)v;}
static void ui_outline_rect(uint8_t *dst,EpdRect r,uint8_t v){(void)dst;(void)r;(void)v;}
'''
for name,path in [('book_cover_crop','main/book/book_cover.c'),('ui_contrast_gray','main/ui/ui_kit.c'),('ui_image_dither_gray','main/ui/ui_image_dither.c'),('cover_background_draw','main/book/book_ticket.c'),('cover_draw','main/book/book_ticket.c')]:
 unit+=function(name,ROOT/path)+'\n'
unit+=r'''
int main(void){uint8_t gray[176*240];unsigned tones=0;
 for(int c=50;c<=150;c+=25){contrast=c;
  for(unsigned v=0;v<256;v++){
   memset(gray,v,sizeof(gray));uint8_t expected=ui_contrast_gray(v)>>4;
   memset(fb,0x77,sizeof(fb));cover_draw(fb,gray,176,240,"book");
   // 平坦源色应得到平坦灰阶；边缘框线不参与。/ Flat source tones remain flat, excluding the outline.
   for(int y=380;y<718;y+=17)for(int x=218;x<466;x+=13)assert(fb[y*684+x]==expected);
   if(c==100)tones|=1u<<fb[400*684+240];
   cover_background_draw(fb,gray,176,240);
   for(int y=0;y<1216;y+=83)for(int x=0;x<684;x+=31)assert(fb[y*684+x]==expected);
  }
 }
 assert(tones==0xffffu);
 contrast=100;memset(gray,255,sizeof(gray));cover_draw(fb,gray,176,240,"book");assert(fb[400*684+240]==15);
 memset(gray,0,sizeof(gray));cover_background_draw(fb,gray,176,240);assert(fb[0]==0);
 for(unsigned w=1;w<=321;w+=80)for(unsigned h=1;h<=241;h+=60){
  uint8_t *natural=malloc((size_t)w*h);assert(natural);memset(natural,255,(size_t)w*h);
  cover_draw(fb,natural,w,h,"book");cover_background_draw(fb,natural,w,h);assert(fb[0]==15);free(natural);
 }
 puts("PASS: actual ticket cover/background retain all 16 tones and solid white/black across contrast levels; no second ordered-dither quantization");}
'''
with tempfile.TemporaryDirectory() as folder:
 p=Path(folder);(p/'test.c').write_text(unit)
 subprocess.run(['cc','-std=gnu11','-Wall','-Wextra','-Werror','-Wno-unused-function','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
