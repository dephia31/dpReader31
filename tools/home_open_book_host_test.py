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
static bool border;
#define UI_LOCK_WIDTH 684
#define UI_LOCK_HEIGHT 1216
static EpdRect simple_resume_rect(void){return (EpdRect){48,58,588,1100};}
static uint8_t ui_contrast_gray(uint8_t g){return g;}
static void epd_draw_pixel(int x,int y,uint8_t g,uint8_t *fb){
 assert(x>=0&&x<684&&y>=0&&y<1216);
 if(!border)assert(x>=130&&x<554&&y>=235&&y<801);
 fb[y*684+x]=g;++writes;
}
'''
u+='#include "startup_logo.h"\n'
logo=(root/'main/assets/startup_logo_4bpp.bin').read_bytes()
u+='static const uint8_t home_logo_4bpp[]={'+','.join(map(str,logo))+'};\n'
for name in ['home_diamond_gray','open_book_placeholder','open_book_sample','draw_open_book','draw_simple_border']:
 u+=function(name,home)+'\n'
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
 unsigned dark=0,light=0;
 for(int y=340;y<695;++y)for(int x=200;x<430;++x){
  dark+=fb[y*684+x]==0x11;
  light+=fb[y*684+x]>=0x99;
 }
 assert(dark>10000&&light>500);
 unsigned before=writes;
 border=true;draw_simple_border(fb);border=false;
 assert(writes-before==684*152+96*1064);
 assert(fb[0]==0x33&&fb[75*684+342]==home_diamond_gray(342,75));
 assert(fb[200*684+342]==255&&fb[500*684]==home_diamond_gray(0,500));
 if(argc==3){FILE *f=fopen(argv[1],"rb");assert(f);uint8_t cover[176*240];assert(fread(cover,1,sizeof(cover),f)==sizeof(cover));fclose(f);
  b=(home_book_t){cover,176,240,false};draw_open_book(fb,&b,(EpdRect){130,235,424,566});f=fopen(argv[2],"wb");assert(f);fprintf(f,"P5\n684 1216\n255\n");assert(fwrite(fb,1,684*1216,f)==684*1216);fclose(f);}
 assert(writes>=424*566*9);free(fb);puts("PASS grayscale/packed/null covers, portrait/landscape/narrow/1px sources, bilinear samples and bounded raster");
}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp)/'test.c';p.write_text(u);exe=p.with_suffix('')
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined,bounds','-I'+str(root/'main/assets'),str(p),'-o',str(exe)],check=True)
 subprocess.run([str(exe),*sys.argv[1:]],check=True)
s=home.read_text();assert '.fast=app_settings_main_fast_refresh()&&!app_settings_simple_full_ui()' in s
assert 'app_settings_simple_full_ui()!=s_cached_simple_full_ui' in function('on_enter',home)
assert 'if(full)draw_open_book' in function('render_simple_root',home)
