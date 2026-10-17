"""Pixel equivalence for production packed axis lines and pixel hot path."""
from pathlib import Path
import subprocess,tempfile
from home_cover_cache_host_test import function
r=Path(__file__).resolve().parents[1]
p=r/'components/epdiy/src/epdiy.c'
s=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define IRAM_ATTR
#define W 1216
#define H 684
#define BYTES (W*H/2)
enum EpdRotation {EPD_ROT_LANDSCAPE, EPD_ROT_PORTRAIT, EPD_ROT_INVERTED_LANDSCAPE, EPD_ROT_INVERTED_PORTRAIT};
static enum EpdRotation display_rotation;
static struct {int width,height;} panel={W,H},*display=&panel;
'''
for name in ['draw_axis_line','epd_draw_hline','epd_draw_vline','epd_draw_pixel']:
 s+=function(name,p)+'\n'
s+=r'''
static void reference_pixel(int x,int y,uint8_t color,uint8_t *fb){
 int px=x,py=y;
 if(display_rotation==EPD_ROT_PORTRAIT){px=W-1-y;py=x;}
 if(display_rotation==EPD_ROT_INVERTED_LANDSCAPE){px=W-1-x;py=H-1-y;}
 if(display_rotation==EPD_ROT_INVERTED_PORTRAIT){px=y;py=H-1-x;}
 if(px<0||py<0||px>=W||py>=H)return;
 unsigned pos=(unsigned)py*W+(unsigned)px, shift=(pos&1)*4;
 fb[pos/2]=(fb[pos/2]&~(15u<<shift))|((color>>4)<<shift);
}
static uint32_t state=0x47af31;
static uint32_t rnd(void){state^=state<<13;state^=state>>17;state^=state<<5;return state;}
int main(void){
 uint8_t *actual=malloc(BYTES),*expected=malloc(BYTES);assert(actual&&expected);
 for(int rot=0;rot<4;++rot){display_rotation=rot;
  int lw=(rot&1)?H:W,lh=(rot&1)?W:H;
  for(int j=0;j<4000;++j){
   for(int i=0;i<BYTES;++i)actual[i]=(uint8_t)(i*7+j*13);
   memcpy(expected,actual,BYTES);
   int x=(int)(rnd()%(lw+200))-100,y=(int)(rnd()%(lh+200))-100;
   int n=(int)(rnd()%1600)-100;uint8_t color=rnd();int kind=j%3;
   if(kind==0){epd_draw_hline(x,y,n,color,actual);for(int i=0;i<n;++i)reference_pixel(x+i,y,color,expected);}
   if(kind==1){epd_draw_vline(x,y,n,color,actual);for(int i=0;i<n;++i)reference_pixel(x,y+i,color,expected);}
   if(kind==2){epd_draw_pixel(x,y,color,actual);reference_pixel(x,y,color,expected);}
   assert(!memcmp(actual,expected,BYTES));
  }
  // Full span/odd edge/zero and negative length at every logical boundary.
  for(int edge=-1;edge<=1;++edge)for(int kind=0;kind<2;++kind){
   memset(actual,0xa5,BYTES);memcpy(expected,actual,BYTES);
   int x=kind?lw-1:edge,y=kind?edge:lh-1,n=kind?lh+1:lw+1;
   if(kind)epd_draw_vline(x,y,n,0xb3,actual);else epd_draw_hline(x,y,n,0xb3,actual);
   for(int i=0;i<n;++i)reference_pixel(x+(kind?0:i),y+(kind?i:0),0xb3,expected);
   assert(!memcmp(actual,expected,BYTES));
  }
 }
 free(actual);free(expected);puts("PASS: production packed lines/pixels,16000 randomized canvases,all rotations,clipping,nibble neighbors,odd edges,nonpositive lengths");
}
'''
with tempfile.TemporaryDirectory() as d:
 d=Path(d);(d/'test.c').write_text(s)
 subprocess.run(['cc','-std=c11','-O2','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(d/'test.c'),'-o',str(d/'test')],check=True)
 subprocess.run([str(d/'test')],check=True)
