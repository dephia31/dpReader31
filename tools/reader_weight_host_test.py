"""Actual one-pixel stroke morph and minus/plus hit geometry.
SPDX-License-Identifier: Apache-2.0
"""
from pathlib import Path
import subprocess,tempfile
from home_cover_cache_host_test import function
r=Path(__file__).resolve().parents[1]
unit=r"""
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "ttf_stroke_filter.h"
typedef struct{int x,y,width,height;}EpdRect;
static bool ui_rect_hit(EpdRect a,int x,int y){return x>=a.x&&y>=a.y&&x<a.x+a.width&&y<a.y+a.height;}
"""+function('reader_weight_rect',r/'main/apps/app_book.c')+'\n'+function('reader_weight_step',r/'main/apps/app_book.c')+'\n'+function('reader_weight_value',r/'main/apps/app_book.c')+r"""
int main(void){
 uint8_t src[121]={0},out[225];src[5*11+5]=255;
 // 圆形距离扩张保持端点；每个小档位单调加深，空白区不生成薄雾。
 // Rounded distance expansion preserves endpoints; fine steps are monotonic and blank regions stay clear.
 for(int step=-32;step<=32;++step){
  int pad=(abs(step)+15)/16,w=11+2*pad;
  memset(out,0xa5,sizeof(out));ttf_stroke_filter_fine(out,src,11,11,48,step);
  for(int i=w*w;i<225;++i)assert(out[i]==0xa5);
  if(step>=0)assert(out[(5+pad)*w+5+pad]==255);
  assert(out[0]==0);
 }
 memset(src,0,sizeof(src));for(int y=0;y<11;++y){src[y*11+5]=64;for(int x=6;x<11;++x)src[y*11+x]=255;}
 int last=64;
 for(int step=1;step<=16;++step){
  ttf_stroke_filter_fine(out,src,11,11,48,step);
  int edge=out[6*13+6];assert(edge>=last&&edge-last<=16);last=edge;
  if(step<12)assert(out[6*13+5]==0); // 旧插值会在这里产生另一层灰边。/ Old interpolation produced an extra halo here.
  assert(out[6*13+8]==255&&out[6*13+1]==0);
 }
 memset(src,0,sizeof(src));ttf_stroke_filter_fine(out,src,11,11,48,32);
 for(int i=0;i<225;++i)assert(!out[i]);
 memset(src,255,sizeof(src));ttf_stroke_filter_fine(out,src,11,11,48,-1);assert(out[1*13+1]==239&&out[6*13+6]==255);
 char value[20];reader_weight_value(value,sizeof(value),1);assert(!strcmp(value,"+0.0625"));reader_weight_value(value,sizeof(value),-3);assert(!strcmp(value,"-0.1875"));reader_weight_value(value,sizeof(value),32);assert(!strcmp(value,"+2"));
 assert(reader_weight_step(99,952)==-1&&reader_weight_step(585,952)==1);
 assert(!reader_weight_step(342,952)&&!reader_weight_step(99,900)&&!reader_weight_step(585,1000));
 puts("PASS: 1/16-pixel coverage increments, sharp rounded erosion/dilation, bounded glyph buffers, numeric labels and capsule hits");
}
"""
with tempfile.TemporaryDirectory() as folder:
 p=Path(folder)/'test.c';p.write_text(unit);exe=p.with_suffix('')
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(r/'main/font'),str(p),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
