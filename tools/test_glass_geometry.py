#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
"""中文：验证真实玻璃绘制的全帧像素、方向、奇偶尺寸及分配失败。/ English: Check real glass pixels, rotations, odd/even shapes and OOM."""
from pathlib import Path
import re,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[1]
PRE=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "ui/ui_image_dither.h"
#define KIIKO_BOARD_METALIO47 1
#define UI_LOCK_WIDTH 684
#define UI_LOCK_HEIGHT 1216
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
typedef struct {int x,y,width,height;} EpdRect;
static int rotation,live;
static size_t reads,allocated;
static bool oom;
#define W 1216
#define H 684
#define BYTES (W*H/2)
static uint8_t image[BYTES],original[BYTES];
static void point(int *x,int *y){int a=*x,b=*y;switch(rotation){case 1:*x=W-b-1;*y=a;break;case 2:*x=W-a-1;*y=H-b-1;break;case 3:*x=b;*y=H-a-1;break;}}
static uint8_t ui_read_pixel(const uint8_t *fb,int x,int y){point(&x,&y);assert(x>=0&&x<W&&y>=0&&y<H);++reads;return (fb[y*(W/2)+x/2]>>(4*(x&1)))&15;}
static void epd_draw_pixel(int x,int y,uint8_t g,uint8_t *fb){point(&x,&y);assert(x>=0&&x<W&&y>=0&&y<H);size_t at=(size_t)y*W/2+x/2;unsigned shift=4*(x&1);fb[at]=(fb[at]&~(15u<<shift))|((g>>4)<<shift);}
static void *heap_caps_malloc(size_t n,unsigned caps){assert(caps==3&&n<=UI_LOCK_WIDTH*391u&&!live);allocated=n;if(oom)return NULL;++live;return malloc(n);}
static void heap_caps_free(void *p){assert(p&&live==1);--live;free(p);}
static uint32_t random_state=0x17ab4419u;
static uint32_t next(void){random_state^=random_state<<13;random_state^=random_state>>17;random_state^=random_state<<5;return random_state;}
'''
POST=r'''
int main(void){
 const EpdRect fixed[]={{18,88,648,280},{31,102,108,108},{19,89,647,279},{0,0,1,1},{0,0,2,2},{0,0,3,1},{0,0,11,2},{20,40,41,7},{20,40,41,8}};
 const uint64_t expected[4]={UINT64_C(9937206119259187668),UINT64_C(1822063921018596244),UINT64_C(14767842650114344299),UINT64_C(9287558141316805199)};
 for(rotation=0;rotation<4;++rotation){
  uint64_t hash=UINT64_C(14695981039346656037);random_state=0x17ab4419u;
  for(unsigned i=0;i<57;++i){
   EpdRect r=i<sizeof(fixed)/sizeof(fixed[0])?fixed[i]:(EpdRect){(int)(next()%21),(int)(next()%100),1+(int)(next()%650),1+(int)(next()%300)};
   const int radii[]={-3,0,1,22,48,54,400};const int whites[]={-3,0,25,100,130};const int refracts[]={-1,0,10,25,99,100,160,200,240};
   int radius=radii[i%7],white=whites[i%5],refract=refracts[i%9];
   for(size_t b=0;b<BYTES;++b)image[b]=(uint8_t)next();memcpy(original,image,BYTES);
   size_t before=reads;
   assert(ui_glass_panel(image,r,radius,white,refract));
   assert(!live&&reads-before==(size_t)r.width*r.height&&allocated==(size_t)r.width*(r.height+7));
   for(size_t b=0;b<BYTES;++b){hash^=image[b];hash*=UINT64_C(1099511628211);}
   oom=true;assert(!ui_glass_panel(image,r,radius,white,refract)&&!live);oom=false;
  }
  printf("rotation%d: %llu\n",rotation,(unsigned long long)hash);
  assert(hash==expected[rotation]);
 }
 puts("PASS: 228 real glass full-frame goldens, four rotations, even/odd/one-pixel panels, radius/white/refraction clamps and OOM; no added allocation");
}
'''
with tempfile.TemporaryDirectory() as t:
 p=Path(t)
 body=re.sub(r'^#include[^\n]*\n','',(ROOT/'main/ui/ui_glass.c').read_text(),flags=re.M)
 (p/'test.c').write_text(PRE+body+POST)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-g','-fsanitize=address,undefined','-I'+str(ROOT/'main'),str(p/'test.c'),str(ROOT/'main/ui/ui_image_dither.c'),'-lm','-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
