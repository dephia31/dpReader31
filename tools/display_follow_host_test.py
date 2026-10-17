"""Native-size continuous scrolling phases and display exit barriers, ASan/UBSan.
SPDX-License-Identifier: Apache-2.0
"""
from pathlib import Path
import re,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
def function(source,name):
 m=re.search(r'^(?:static )?[^\n;{}]+\b'+name+r'\([^;{}]*\)\s*\{',source,re.M);assert m,name
 start=m.start();i=source.index('{',start)+1;depth=1;quote=None;escape=False
 while depth:
  c=source[i]
  if quote:
   if escape:escape=False
   elif c=='\\':escape=True
   elif c==quote:quote=None
  elif c in "\"'":quote=c
  elif c=='{':depth+=1
  elif c=='}':depth-=1
  i+=1
 return source[start:i]+'\n'
continuous=(root/'components/continuous_du/continuous_du.c').read_text();display=(root/'main/display.c').read_text()
unit=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ui_follow_pixels.h"
#define W 1216
#define H 684
#define BYTES ((size_t)W*H/2)
typedef struct{int x,y,width,height;}EpdRect;
typedef struct{int id;}EpdWaveform;
enum EpdDrawError{EPD_DRAW_SUCCESS,EPD_DRAW_EMPTY_LINE_QUEUE};
enum EpdDrawMode{MODE_DU=1,MODE_GC16=2,MODE_GL16=5};
#define MODE_PACKING_1PPB_DIFFERENCE 16
#define READ_PICO_EPD_SCAN_FAST 1
typedef struct{uint8_t *front_fb,*back_fb,*difference_fb,*dirty_columns;bool *dirty_lines;}EpdiyHighlevelState;
static int8_t *s_states;
static int s_pending;
static bool s_follow_active,s_full_refresh_requested;
static int phase_calls,full_calls,area_calls,fast_calls,ota_depth;
static enum EpdDrawError failure;
static EpdRect last_area;
static const EpdWaveform E0470_WAVEFORM={0},E0470_FOLLOW_WAVEFORM={1},continuous_du_waveform={2};
static int epd_width(void){return W;}
static int epd_height(void){return H;}
static EpdRect epd_full_screen(void){return(EpdRect){0,0,W,H};}
static void read_pico_epd_use_scan(int scan){assert(scan==READ_PICO_EPD_SCAN_FAST);}
static enum EpdDrawError epd_draw_base(EpdRect a,const uint8_t *diff,EpdRect crop,int mode,int temp,const bool *lines,const uint8_t *cols,const EpdWaveform *wave){
 assert(ota_depth&&a.width==W&&crop.height==H&&mode==(16|MODE_DU)&&temp==25&&wave==&continuous_du_waveform);
 for(int y=0;y<H;++y)for(int x=0;x<W;++x){
  if(s_states[(size_t)y*W+x]){assert(lines[y]&&cols[x/2]);assert(diff[(size_t)y*W+x]==(s_states[(size_t)y*W+x]>0?0x0f:0xf0));}
 }
 ++phase_calls;return failure;
}
static bool main_nav_geometry(void){return true;}
static bool pico_online_display_begin(void){++ota_depth;return true;}
static void pico_online_display_end(bool held){assert(held&&ota_depth==1);--ota_depth;}
static void display_main_transition_cancel(void){}
static void epd_poweron(void){}
static void rails_keepalive(void){}
static void epd_hl_waveform(EpdiyHighlevelState *hl,const EpdWaveform *wave){(void)hl;(void)wave;}
static enum EpdDrawError hl_update(EpdiyHighlevelState *hl,const EpdWaveform *wave,int mode,bool full,void *area,bool diff){
 (void)diff;assert(ota_depth&&wave==&E0470_WAVEFORM&&mode==MODE_GC16&&full&&!area);memcpy(hl->back_fb,hl->front_fb,BYTES);++full_calls;return EPD_DRAW_SUCCESS;
}
static enum EpdDrawError update_display_area_full_with_unlocked(EpdiyHighlevelState *hl,const EpdWaveform *wave,int mode,EpdRect area){
 assert(ota_depth&&wave==&E0470_WAVEFORM&&mode==MODE_GL16);last_area=area;++area_calls;
 ui_follow_pixels_remember(hl->front_fb,hl->back_fb,W,H,area.y,area.y+area.height);return EPD_DRAW_SUCCESS;
}
static int gray_diff_calls,du_diff_calls;
static enum EpdDrawError update_display_area_diff_with_unlocked(EpdiyHighlevelState *hl,const EpdWaveform *wave,int mode,EpdRect area){
 assert(ota_depth);last_area=area;
 if(mode==MODE_GL16){assert(wave==&E0470_WAVEFORM);++gray_diff_calls;}
 else{assert(mode==MODE_DU&&wave==&E0470_FOLLOW_WAVEFORM);++du_diff_calls;}
 ui_follow_pixels_remember(hl->front_fb,hl->back_fb,W,H,area.y,area.y+area.height);return EPD_DRAW_SUCCESS;
}
static enum EpdDrawError update_display_fast_page_unlocked(EpdiyHighlevelState *hl){(void)hl;++fast_calls;return EPD_DRAW_SUCCESS;}
static int continuous_du_dark_phases(void){return 6;}
'''
for name in ('continuous_du_reset','continuous_du_busy','continuous_du_rect_from_logical','fb_nibble','mark_pixel','continuous_du_mark_diff','continuous_du_scan'):
 unit+=function(continuous,name)
for name in ('display_request_full_refresh','display_follow_busy','update_display_full_unlocked','display_follow_area','display_follow_settle','update_display_fast_page','update_display_area_full_with'):
 unit+=function(display,name)
unit+=r'''
int main(void){
 s_states=calloc((size_t)W*H,1);assert(s_states);
 EpdiyHighlevelState hl={.front_fb=malloc(BYTES),.back_fb=malloc(BYTES),.difference_fb=calloc((size_t)W*H,1),.dirty_columns=calloc(W/2,1),.dirty_lines=calloc(H,sizeof(bool))};
 assert(hl.front_fb&&hl.back_fb&&hl.difference_fb&&hl.dirty_columns&&hl.dirty_lines);
 memset(hl.front_fb,0x78,BYTES);memset(hl.back_fb,0x78,BYTES);EpdRect area={0,160,684,936};
 assert(display_follow_area(&hl,area,true,true)==EPD_DRAW_SUCCESS&&phase_calls==1&&display_follow_busy());
 for(int y=0;y<H;++y)for(int col=0;col<W/2;++col){size_t at=(size_t)y*W/2+col;assert(hl.front_fb[at]==(col>=80&&col<548?0x0f:0x78));assert(hl.back_fb[at]==hl.front_fb[at]);}
 for(int i=0;i<5;++i)display_follow_area(&hl,area,false,true);
 assert(!display_follow_busy()&&phase_calls==6);display_follow_area(&hl,area,false,true);assert(phase_calls==6);
 // Black target, then white target before the drive finishes: newest direction wins.
 for(int y=0;y<H;++y)memset(hl.front_fb+(size_t)y*W/2+80,0,468);
 display_follow_area(&hl,area,true,true);assert(s_states[160]==5);
 for(int y=0;y<H;++y)memset(hl.front_fb+(size_t)y*W/2+80,255,468);
 display_follow_area(&hl,area,true,true);assert(s_states[160]==-5);
 // Finishing cannot discard in-flight drives. Caller drains them before drawing gray.
 display_follow_settle(&hl,area,false);assert(display_follow_busy()&&s_follow_active&&!gray_diff_calls&&!area_calls);
 while(display_follow_busy())display_follow_area(&hl,area,false,true);
 memset(hl.front_fb,0x78,BYTES);display_follow_settle(&hl,area,true);
 assert(!display_follow_busy()&&!s_follow_active&&gray_diff_calls==1&&!area_calls&&last_area.y==160&&last_area.height==936);
 assert(hl.back_fb[80]==0x78&&hl.back_fb[79]==0x78&&hl.back_fb[548]==0x78);
 // Fast completion keeps the binary reference without a physical scan or a next-action full refresh.
 display_follow_area(&hl,area,true,true);while(display_follow_busy())display_follow_area(&hl,area,false,true);
 int before=phase_calls;display_follow_settle(&hl,area,false);
 assert(!s_follow_active&&!display_follow_busy()&&phase_calls==before&&gray_diff_calls==1&&!area_calls);
 assert(!memcmp(hl.front_fb,hl.back_fb,BYTES));update_display_fast_page(&hl);assert(!full_calls&&fast_calls==1);
 // Memory-pressure fallback also presents binary targets and retains fixed chrome.
 memset(hl.front_fb,0x78,BYTES);display_follow_area(&hl,area,true,false);assert(du_diff_calls==1&&!display_follow_busy());
 assert(hl.front_fb[80]==0x0f&&hl.front_fb[79]==0x78&&hl.front_fb[548]==0x78);
 memset(hl.front_fb,0x00,BYTES);
 // A page change midway through driving must establish a complete baseline once.
 display_follow_area(&hl,area,true,true);assert(display_follow_busy());update_display_fast_page(&hl);
 assert(full_calls==1&&!s_follow_active&&!display_follow_busy());update_display_fast_page(&hl);assert(fast_calls==2&&full_calls==1);
 display_request_full_refresh();update_display_area_full_with(&hl,&E0470_WAVEFORM,MODE_GL16,area);assert(full_calls==2&&!s_full_refresh_requested);
 update_display_fast_page(&hl);assert(fast_calls==3&&full_calls==2);
 memset(hl.front_fb,0xff,BYTES);display_follow_area(&hl,area,true,true);failure=EPD_DRAW_EMPTY_LINE_QUEUE;
 display_follow_area(&hl,area,false,true);assert(!display_follow_busy()&&s_full_refresh_requested&&!s_follow_active);failure=EPD_DRAW_SUCCESS;
 update_display_fast_page(&hl);assert(full_calls==3&&!s_full_refresh_requested);
 uint8_t original=hl.front_fb[0];assert(!ui_follow_pixels(hl.front_fb,hl.back_fb,W,H,159,1096)&&hl.front_fb[0]==original);
 assert(!ui_follow_pixels(hl.front_fb,hl.back_fb,W,H,160,1218));
 // Word-packed thresholding matches all 256 bytes, including unaligned rows and scalar tails.
 for(int top=160;top<=242;top+=2){
  for(size_t i=0;i<BYTES;++i)hl.front_fb[i]=hl.back_fb[i]=(uint8_t)i;
  assert(ui_follow_pixels(hl.front_fb,hl.back_fb,W,H,top,1096));
  for(int row=0;row<H;++row)for(int col=0;col<W/2;++col){
   size_t i=(size_t)row*(W/2)+col;uint8_t old=hl.back_fb[i];
   uint8_t expected=col>=top/2&&col<548?(old&8?15:0)|(old&128?240:0):old;
   assert(hl.front_fb[i]==expected&&hl.back_fb[i]==(uint8_t)i);
  }
 }
 // Byte-pair skipping must match the original per-pixel algorithm, including odd crop edges and in-flight phases.
 int8_t *reference=malloc((size_t)W*H);assert(reference);
 for(int trial=0;trial<16;++trial){
  for(size_t i=0;i<BYTES;++i){hl.front_fb[i]=(uint8_t)(i*13+trial*17);hl.back_fb[i]=(uint8_t)(i*7+trial*19);}
  for(size_t i=0;i<(size_t)W*H;++i)s_states[i]=(int8_t)(i%11)-5;
  memcpy(reference,s_states,(size_t)W*H);s_pending=0;
  for(size_t i=0;i<(size_t)W*H;++i)if(s_states[i])++s_pending;
  EpdRect crop={trial-4,trial*7-4,W/2+trial,H/2+trial};bool invert=trial&1;
  for(int y=0;y<H;++y)for(int x=0;x<W;++x){
   if(x<crop.x||x>=crop.x+crop.width||y<crop.y||y>=crop.y+crop.height)continue;
   unsigned dest=fb_nibble(hl.front_fb,W,x,y),src=fb_nibble(hl.back_fb,W,x,y);
   if(dest!=src)reference[(size_t)y*W+x]=(dest<8?6:-6)*(invert?-1:1);
  }
  continuous_du_mark_diff(hl.front_fb,hl.back_fb,crop,6,invert);
  assert(!memcmp(reference,s_states,(size_t)W*H));int pending=0;
  for(size_t i=0;i<(size_t)W*H;++i)if(reference[i])++pending;assert(pending==s_pending);
 }
 free(reference);
 assert(!ota_depth);free(s_states);free(hl.front_fb);free(hl.back_fb);free(hl.difference_fb);free(hl.dirty_lines);free(hl.dirty_columns);
 puts("PASS: real continuous pixel/phase and display functions: one scan per tick, reversal, fixed header/nav nibble boundaries, changed-gray-only settle / no-scan fast finish, page/wake/error barriers once, invalid crop rejected; no new frame allocation");
}
'''
with tempfile.TemporaryDirectory() as directory:
 c=Path(directory)/'follow.c';binary=c.with_suffix('');c.write_text(unit)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+str(root/'main/ui'),str(c),'-o',str(binary)],check=True)
 subprocess.run([str(binary)],check=True)
