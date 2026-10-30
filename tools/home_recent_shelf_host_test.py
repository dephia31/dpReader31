#!/usr/bin/env python3
"""Run actual recent-book shelf composition in gray/BW with bounded buffers."""
from pathlib import Path
import subprocess,tempfile,sys
from home_cover_cache_host_test import function
root=Path(__file__).resolve().parents[1]
source=root/'main/ui/ui_home_widget.c'
unit=r'''
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define W 320
#define H 350
#define UI_HOME_FOLDER_GRAY_BYTES (W*H/2)
#define UI_HOME_FOLDER_BW_BYTES (W*H/8)
typedef struct{const uint8_t *pixels;uint16_t width,height,stride;bool binary;}ui_home_cover_t;
static uint8_t ui_contrast_gray(uint8_t g){return g;}
static uint8_t ui_image_dither_bw(uint8_t g,int x,int y){(void)x;(void)y;return g>=128?255:0;}
static uint8_t ui_image_dither_gray(uint8_t g,int x,int y){(void)x;(void)y;return g;}
'''
for name in ('pixel','minimum','maximum','clamp','cover','ui_home_folder_compose'):
 unit+=function(name,source)+'\n'
unit+=r'''
static unsigned at(uint8_t *p,int x,int y,bool bw){return bw?((p[y*(W/8)+x/8]>>(x&7))&1)*255:((p[y*(W/2)+x/2]>>((x&1)*4))&15)*17;}
int main(int argc,char **argv){
 uint8_t *memory=malloc(W*H/2+2),*out=memory+1,*scratch=malloc(2*W*H);
 uint8_t covers[3][90*140];ui_home_cover_t c[3];
 for(int i=0;i<3;++i){memset(covers[i],i*40+32,sizeof(covers[i]));
  for(int y=35;y<40;++y)for(int x=14;x<76;++x)covers[i][y*90+x]=240;
  c[i]=(ui_home_cover_t){covers[i],90,140,90,false};}
 for(int bw=0;bw<2;++bw)for(int count=0;count<=3;++count){
  size_t bytes=bw?W*H/8:W*H/2;memory[0]=out[bytes]=0x5a;
  assert(ui_home_folder_compose(out,scratch,c,count,bw));
  assert(memory[0]==0x5a&&out[bytes]==0x5a);
  for(int x=12;x<308;++x)assert(at(out,x,285,bw)==(bw?0:51));
  assert(at(out,160,320,bw)==255);
  for(int i=0;i<count;++i)assert(at(out,54+106*i,230,bw)<255);
  if(!bw&&count==3&&argc>1){FILE*f=fopen(argv[1],"wb");assert(f);fprintf(f,"P5\n%d %d\n255\n",W,H);
   for(int y=0;y<H;++y)for(int x=0;x<W;++x)fputc(at(out,x,y,false),f);fclose(f);}
 }
 assert(!ui_home_folder_compose(out,scratch,c,4,false));
 free(memory);free(scratch);puts("PASS actual Home shelf:0–3 covers,gray/BW,guard bytes,black rail,white surroundings");
}
'''
with tempfile.TemporaryDirectory() as directory:
 p=Path(directory);(p/'test.c').write_text(unit)
 subprocess.run(['cc','-std=c11','-O2',str(p/'test.c'),'-lm','-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test'),*sys.argv[1:]],check=True)
