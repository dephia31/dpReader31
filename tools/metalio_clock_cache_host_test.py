#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
# 中文：真实时钟冷绘制与暖缓存逐字节比较，覆盖样式/时间/裁剪/方向和失败回退。
# English: Compare actual cold clock output with warm rasters across styles, time, crop, rotation and cache fallback.
from pathlib import Path
import subprocess,tempfile
from home_cover_cache_host_test import function
root=Path(__file__).resolve().parents[1]
header=(root/'main/ui/ui_home_clock.h').read_text().replace('#include "ui_home_widget.h"','').replace('#pragma once','')
unit=r'''
#include <assert.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#define UI_HOME_WIDGET_W 320
#define UI_HOME_WIDGET_H 350
#define KIIKO_BOARD_METALIO47 1
typedef struct {int x,y,width,height;} EpdRect;
enum EpdRotation {EPD_ROT_LANDSCAPE,EPD_ROT_PORTRAIT,EPD_ROT_INVERTED_LANDSCAPE,EPD_ROT_INVERTED_PORTRAIT};
static enum EpdRotation rotation=EPD_ROT_INVERTED_PORTRAIT;
static unsigned paints,texts;
static int font_variant;
static int epd_width(void){return 1216;}
static int epd_height(void){return 684;}
static enum EpdRotation epd_get_rotation(void){return rotation;}
static int epd_rotated_display_width(void){return rotation%2?684:1216;}
static int epd_rotated_display_height(void){return rotation%2?1216:684;}
static EpdRect ui_rotate_rect_to_fb(EpdRect rect);
static uint8_t epd_get_pixel(int x,int y,int w,int h,const uint8_t *fb){assert(x>=0&&y>=0&&x<w&&y<h);return ((fb[(size_t)y*w/2+x/2]>>((x&1)*4))&15)*17;}
static void epd_draw_pixel(int x,int y,int gray,uint8_t *fb){
 if(x<0||y<0||x>=epd_rotated_display_width()||y>=epd_rotated_display_height())return;
 int px=x,py=y;
 switch(rotation){
 case EPD_ROT_PORTRAIT:px=1216-y-1;py=x;break;
 case EPD_ROT_INVERTED_PORTRAIT:px=y;py=684-x-1;break;
 case EPD_ROT_INVERTED_LANDSCAPE:px=1216-x-1;py=684-y-1;break;
 default:break;}
 unsigned shift=(px&1)*4;size_t pos=(size_t)py*608+px/2;
 fb[pos]=(fb[pos]&~(15u<<shift))|((gray>>4)<<shift);++paints;
}
static void epd_fill_rect(EpdRect r,int gray,uint8_t *fb){for(int y=r.y;y<r.y+r.height;++y)for(int x=r.x;x<r.x+r.width;++x)epd_draw_pixel(x,y,gray,fb);}
static void ui_text_fixed_ink_vc(uint8_t *fb,int x,int y,int px,const char *text,int align,int ink){(void)px;(void)text;(void)align;++texts;epd_draw_pixel(x+font_variant,y,ink,fb);}
static void ui_home_clock_draw(uint8_t *,EpdRect,const struct tm *,bool);
static void ui_home_clock_body_draw(uint8_t *,EpdRect,bool);
static void ui_home_clock_hands_draw(uint8_t *,EpdRect,const struct tm *);
static void epd_draw_line(int x,int y,int xx,int yy,int ink,uint8_t *fb){
 int dx=abs(xx-x),sx=x<xx?1:-1,dy=-abs(yy-y),sy=y<yy?1:-1,err=dx+dy;
 for(;;){epd_draw_pixel(x,y,ink,fb);if(x==xx&&y==yy)break;int e=2*err;if(e>=dy){err+=dy;x+=sx;}if(e<=dx){err+=dx;y+=sy;}}
}
static void epd_fill_circle(int x,int y,int r,int ink,uint8_t *fb){for(int yy=-r;yy<=r;++yy)for(int xx=-r;xx<=r;++xx)if(xx*xx+yy*yy<=r*r)epd_draw_pixel(x+xx,y+yy,ink,fb);}
#define EPD_DRAW_ALIGN_CENTER 1
'''+header+'\n'
unit+=function('ui_rotate_rect_to_fb',root/'main/ui/ui_kit.c')+'\n'
clock=(root/'main/ui/ui_home_clock.c').read_text().replace('#include "ui_home_clock.h"','')
unit+=clock+'\n#define W UI_HOME_WIDGET_W\n#define H UI_HOME_WIDGET_H\n#include "ui_home_shapes.h"\n'
widget='\n'.join(function(name,root/'main/ui/ui_home_widget.c') for name in ['minimum','maximum','edge_distance','tx','ty','hand','ui_home_clock_body_draw','ui_home_clock_hands_draw','ui_home_clock_draw'])
unit+=widget+r'''
#define BYTES (1216*684/2)
static uint8_t cold[BYTES],warm[BYTES],memory[UI_HOME_CLOCK_BASE_CACHE_BYTES];
static void draw(uint8_t *fb,struct tm date,ui_home_clock_style_t style,bool bw,EpdRect clip,unsigned phase,unsigned trail){
 ui_home_clock_trail_draw_region(fb,(EpdRect){24,120,320,350},&date,bw,style,clip,phase,17,trail);
}
int main(void){
 struct tm date={.tm_year=126,.tm_mon=9,.tm_mday=19,.tm_hour=13,.tm_min=31,.tm_sec=5};
 const ui_home_clock_style_t styles[]={UI_HOME_CLOCK_PS1995,UI_HOME_CLOCK_MOON,UI_HOME_CLOCK_ASTERISK,UI_HOME_CLOCK_DALI,UI_HOME_CLOCK_KEHAI,UI_HOME_CLOCK_CAT,UI_HOME_CLOCK_HOUSE};
 for(rotation=0;rotation<=3;++rotation)for(int bw=0;bw<2;++bw){
  EpdRect clip={24,120,320,326};
  ui_home_clock_bind_base_cache(NULL,0);
  for(unsigned i=0;i<sizeof(styles)/sizeof(styles[0]);++i){
   ui_home_clock_style_t style=styles[i];
   memset(cold,0xff,BYTES);paints=0;draw(cold,date,style,bw,clip,50,0);unsigned cold_paints=paints;
   ui_home_clock_bind_base_cache(memory,sizeof(memory));
   memset(warm,0xff,BYTES);draw(warm,date,style,bw,clip,50,0);assert(!memcmp(cold,warm,BYTES));
   memset(warm,0xff,BYTES);paints=0;draw(warm,date,style,bw,clip,50,0);assert(!memcmp(cold,warm,BYTES));
   if(style==UI_HOME_CLOCK_MOON||style==UI_HOME_CLOCK_ASTERISK||style==UI_HOME_CLOCK_DALI)assert(!paints);
   else assert(paints);
   if(style==UI_HOME_CLOCK_HOUSE||style==UI_HOME_CLOCK_PS1995){
    assert(paints<cold_paints/4);
    // 小屋缓存不含数字；系统字体变化时数字必须立即重画，房体仍可复用。
    // House snapshots exclude numerals; a new system face redraws them immediately while reusing the body.
    font_variant=1;memset(warm,0xff,BYTES);texts=0;draw(warm,date,style,bw,clip,50,0);assert(texts==(style==UI_HOME_CLOCK_HOUSE?4u:12u));
    ui_home_clock_bind_base_cache(NULL,0);memset(cold,0xff,BYTES);draw(cold,date,style,bw,clip,50,0);assert(!memcmp(cold,warm,BYTES));
    font_variant=0;ui_home_clock_bind_base_cache(memory,sizeof(memory));
   }

   struct tm next=date;next.tm_min++;
   memset(warm,0xff,BYTES);paints=0;draw(warm,next,style,bw,clip,60,0);assert(paints);
   ui_home_clock_bind_base_cache(NULL,0);memset(cold,0xff,BYTES);draw(cold,next,style,bw,clip,60,0);assert(!memcmp(cold,warm,BYTES));
   if(style==UI_HOME_CLOCK_MOON){
    ui_home_clock_bind_base_cache(memory,sizeof(memory));memset(warm,0xff,BYTES);draw(warm,date,style,bw,clip,50,0);
    struct tm tomorrow=date;tomorrow.tm_mday++;tomorrow.tm_yday++;paints=0;
    memset(warm,0xff,BYTES);draw(warm,tomorrow,style,bw,clip,50,0);assert(paints);
    ui_home_clock_bind_base_cache(NULL,0);memset(cold,0xff,BYTES);draw(cold,tomorrow,style,bw,clip,50,0);assert(!memcmp(cold,warm,BYTES));
   }

  }
  // 相同分钟换样式不能命中旧表盘；变化裁剪与轨迹必须与无缓存完全相同。
  // Style changes within one minute must miss; clipped/trail output must equal uncached drawing.
  ui_home_clock_bind_base_cache(memory,sizeof(memory));
  for(unsigned i=0;i<3;++i){memset(warm,0xff,BYTES);draw(warm,date,styles[i],bw,clip,50,0);
   ui_home_clock_bind_base_cache(NULL,0);memset(cold,0xff,BYTES);draw(cold,date,styles[i],bw,clip,50,0);assert(!memcmp(cold,warm,BYTES));ui_home_clock_bind_base_cache(memory,sizeof(memory));}
  for(unsigned trail=0;trail<=600;trail+=600){EpdRect partial={64,185,100,80};memset(warm,0xff,BYTES);draw(warm,date,UI_HOME_CLOCK_KEHAI,bw,partial,77,trail);
   ui_home_clock_bind_base_cache(NULL,0);memset(cold,0xff,BYTES);draw(cold,date,UI_HOME_CLOCK_KEHAI,bw,partial,77,trail);assert(!memcmp(cold,warm,BYTES));ui_home_clock_bind_base_cache(memory,sizeof(memory));}
  ui_home_clock_bind_base_cache(memory,1);paints=0;memset(warm,0xff,BYTES);draw(warm,date,UI_HOME_CLOCK_MOON,bw,clip,50,0);assert(paints);
 }

 // 大小表盘、奇数裁剪与不同轨迹命中缓存；逐字节保持范围外哨兵，重绘量必须下降。
 // Warm small/enlarged faces, odd crops and trails; preserve outside sentinels byte-for-byte and reduce raster work.
 for(rotation=0;rotation<=3;++rotation)for(int bw=0;bw<2;++bw)for(int large=0;large<2;++large){
  EpdRect bounds={24,120,large?460:320,large?503:350};
  EpdRect partial={bounds.x+75,bounds.y+85,97,101};
  for(int style=UI_HOME_CLOCK_KEHAI;style<=UI_HOME_CLOCK_CAT;style+=2){
   for(unsigned trail=0;trail<=600;trail+=150){
    ui_home_clock_bind_base_cache(NULL,0);memset(cold,0xab,BYTES);paints=0;
    ui_home_clock_trail_draw_region(cold,bounds,&date,bw,style,partial,77,17,trail);unsigned uncached=paints;
    ui_home_clock_bind_base_cache(memory,sizeof(memory));memset(warm,0xab,BYTES);
    ui_home_clock_trail_draw_region(warm,bounds,&date,bw,style,bounds,50,17,0);
    memset(warm,0xab,BYTES);paints=0;
    ui_home_clock_trail_draw_region(warm,bounds,&date,bw,style,partial,77,17,trail);
    assert(!memcmp(cold,warm,BYTES));assert(paints<uncached);
   }
  }
  // 分钟、日期、旋转、黑白或边界变化都不能错误复用旧底图。
  // Minute/date/rotation/BW/bounds changes cannot reuse an incompatible base.
  for(int style=0;style<UI_HOME_CLOCK_COUNT;++style){
   ui_home_clock_bind_base_cache(memory,sizeof(memory));memset(warm,0xff,BYTES);
   ui_home_clock_trail_draw_region(warm,bounds,&date,bw,style,bounds,77,17,300);
   memset(warm,0xff,BYTES);paints=0;
   ui_home_clock_trail_draw_region(warm,bounds,&date,bw,style,bounds,78,17,301);unsigned cached=paints;
   ui_home_clock_bind_base_cache(NULL,0);memset(cold,0xff,BYTES);paints=0;
   ui_home_clock_trail_draw_region(cold,bounds,&date,bw,style,bounds,78,17,301);
   assert(!memcmp(cold,warm,BYTES));assert(cached<paints);
  }
 }

 // 宜家分钟局刷恢复白色表盘，擦除旧指针；不重算机身，字体仍实时。
 // IKEA minute crops restore the white dial and erase old hands without recalculating the body; glyphs stay live.
 for(rotation=0;rotation<=3;++rotation)for(int bw=0;bw<2;++bw){
  ui_home_clock_bind_base_cache(memory,sizeof(memory));memset(warm,0xff,BYTES);
  EpdRect full={24,120,320,326};draw(warm,date,UI_HOME_CLOCK_PS1995,bw,full,50,0);
  memcpy(cold,warm,BYTES);struct tm next=date;next.tm_min++;
  EpdRect face={78,164,198,198};paints=0;texts=0;
  draw(warm,next,UI_HOME_CLOCK_PS1995,bw,face,60,0);unsigned hot=paints;assert(texts==12);
  ui_home_clock_bind_base_cache(NULL,0);paints=0;draw(cold,next,UI_HOME_CLOCK_PS1995,bw,face,60,0);
  assert(hot<paints/4);assert(!memcmp(warm,cold,BYTES));
 }
 puts("PASS: real cold/warm gray/BW clock rasters match across4rotations, style/minute changes, partial clips, trails, live fonts, enlarged faces, odd crops, reduced motion work and unavailable scratch");
}
'''
with tempfile.TemporaryDirectory(prefix='s31-clock-cache-') as directory:
 c=Path(directory)/'clock.c';binary=c.with_suffix('');c.write_text(unit)
 subprocess.run(['cc','-std=c11','-O1','-Wall','-Wextra','-Werror','-Wno-sign-compare','-fsanitize=address,undefined','-I'+str(root/'main/ui'),str(c),str(root/'main/ui/ui_image_dither.c'),'-lm','-o',str(binary)],check=True)
 subprocess.run([str(binary)],check=True)
