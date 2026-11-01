#!/usr/bin/env python3
"""Draw the actual icon masks and verify normalized visible ink size/alignment."""
from pathlib import Path
import subprocess,tempfile,sys
from home_cover_cache_host_test import function
root=Path(__file__).resolve().parents[1];source=root/'main/ui/ui_kit.c'
unit=r'''
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include "ui_icons.h"
static uint8_t ui_read_pixel(const uint8_t *fb,int x,int y){assert(x>=0&&x<256&&y>=0&&y<256);return fb[y*256+x];}
static uint8_t ui_contrast_gray(uint8_t gray){return gray;}
static void epd_draw_pixel(int x,int y,int gray,uint8_t *fb){assert(x>=0&&x<256&&y>=0&&y<256);fb[y*256+x]=gray>>4;}
'''
for name in ('icon_nibble','ui_draw_icon','ui_draw_icon_normalized'):
 unit+=function(name,source)+'\n'
unit+=r'''
int main(int argc,char **argv){
 uint8_t fb[256*256];
 for(int size=32;size<=64;size+=32)for(int icon=0;icon<UI_ICON_COUNT;++icon){
  memset(fb,15,sizeof(fb));ui_draw_icon_normalized(fb,128,128,size,icon,0);
  int left=256,top=256,right=-1,bottom=-1;
  for(int y=0;y<256;++y)for(int x=0;x<256;++x)if(fb[y*256+x]<14){if(x<left)left=x;if(x>right)right=x;if(y<top)top=y;if(y>bottom)bottom=y;}
  assert(right>=left&&bottom>=top);int w=right-left+1,h=bottom-top+1,extent=w>h?w:h;
  if(extent<size-3||extent>size+2||abs(left+right-255)>4||abs(top+bottom-255)>4){fprintf(stderr,"icon=%d size=%d bbox=%d,%d,%d,%d\n",icon,size,left,top,right,bottom);return 1;}
  if(argc>1&&size==64&&icon==UI_ICON_BLUETOOTH){FILE *f=fopen(argv[1],"wb");assert(f);fprintf(f,"P5\n256 256\n255\n");for(unsigned i=0;i<sizeof(fb);++i)fputc(fb[i]*17,f);fclose(f);}
 }
 puts("PASS actual icons: every visible glyph centered and uniformly sized at32/64px, bounds checked");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(unit)
 subprocess.run(['cc','-std=c11','-O2','-Wall','-Wextra','-Werror','-fsanitize=undefined','-I'+str(root/'boards/metalio47/src/main/assets'),str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test'),*sys.argv[1:]],check=True)
