"""中文：检查打开书壳的像素边界与封面采样。/ English: Check open-book raster bounds and cover sampling."""
from pathlib import Path
import subprocess,tempfile,sys
from home_cover_cache_host_test import function
root=Path(__file__).resolve().parents[1]
home=root/'main/apps/app_dashboard.c'
u=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct {int x,y,width,height;} EpdRect;
typedef struct {uint8_t *cover;unsigned cover_width,cover_height;bool cover_fast;} home_book_t;
static unsigned writes;
static uint8_t ui_contrast_gray(uint8_t g){return g;}
static void epd_draw_pixel(int x,int y,uint8_t g,uint8_t *fb){assert(x>=130&&x<554&&y>=235&&y<801);fb[y*684+x]=g;++writes;}
'''
for name in ['open_book_sample','draw_open_book']:u+=function(name,home)+'\n'
u+=r'''
int main(int argc,char **argv){
 uint8_t *fb=malloc(684*1216);assert(fb);memset(fb,255,684*1216);
 uint8_t gray[4]={0,255,255,0};home_book_t b={gray,2,2,false};
 assert(open_book_sample(&b,0,0)==0&&open_book_sample(&b,65535,0)>250);
 assert(open_book_sample(&b,32768,32768)>=126&&open_book_sample(&b,32768,32768)<=128);
 uint8_t bit=1;b=(home_book_t){&bit,1,1,true};assert(open_book_sample(&b,65535,65535)==255);
 for(unsigned shape=0;shape<4;++shape){unsigned w=shape==0?1:shape==1?176:shape==2?240:33,h=shape==0?1:shape==1?240:shape==2?176:800;
  for(unsigned packed=0;packed<2;++packed){size_t n=(packed?(w+7)/8:w)*h;uint8_t *p=malloc(n);assert(p);memset(p,0x77,n);
   b=(home_book_t){p,w,h,packed};draw_open_book(fb,&b,(EpdRect){130,235,424,566});free(p);}}
 b=(home_book_t){0};draw_open_book(fb,&b,(EpdRect){130,235,424,566});
 if(argc==3){FILE *f=fopen(argv[1],"rb");assert(f);uint8_t cover[176*240];assert(fread(cover,1,sizeof(cover),f)==sizeof(cover));fclose(f);
  b=(home_book_t){cover,176,240,false};draw_open_book(fb,&b,(EpdRect){130,235,424,566});f=fopen(argv[2],"wb");assert(f);fprintf(f,"P5\n684 1216\n255\n");assert(fwrite(fb,1,684*1216,f)==684*1216);fclose(f);}
 assert(writes>=424*566*9);free(fb);puts("PASS grayscale/packed/null covers, portrait/landscape/narrow/1px sources, bilinear samples and bounded raster");
}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp)/'test.c';p.write_text(u);exe=p.with_suffix('')
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined,bounds',str(p),'-o',str(exe)],check=True)
 subprocess.run([str(exe),*sys.argv[1:]],check=True)
s=home.read_text();assert '.fast=app_settings_main_fast_refresh()&&!app_settings_simple_full_ui()' in s
assert 'app_settings_simple_full_ui()!=s_cached_simple_full_ui' in function('on_enter',home)
assert 'if(full)draw_open_book' in function('render_simple_root',home)
