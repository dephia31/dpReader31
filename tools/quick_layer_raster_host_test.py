"""实际快捷层/点击缓存/汉字缩放的像素与内存检查。
Actual quick-layer, pressed-tile and Han scaling pixel/memory checks.
SPDX-License-Identifier: Apache-2.0
"""
from pathlib import Path
import re, subprocess, tempfile, sys
root = Path(__file__).resolve().parents[1]

def body(path):
    return re.sub(r'^#include[^\n]*\n', '', (root/path).read_text(), flags=re.M)

def function(path, name):
    text=(root/path).read_text()
    start=re.search(r'^(?:static )?[^\n]+\b'+name+r'\([^;{}]*\)\s*\{',text,re.M)
    assert start, name
    at=start.end(); depth=1
    while depth:
        depth += (text[at]=='{')-(text[at]=='}'); at+=1
    return text[start.start():at]+'\n'

code=r'''
#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "ui/ui_nav_layout.h"
#include "ui/ui_image_dither.h"
#include "assets/ui_icons.h"
#include "font/ui_hanzi_scale.h"
#define UI_LOCK_WIDTH 684
#define UI_LOCK_HEIGHT 1216
#define UI_TOUCH_SLOP_PX 24
#define UI_GRAY_WHITE 255
#define UI_GRAY_BLACK 0
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
typedef struct {int x,y,width,height;} EpdRect;
typedef enum {UI_CLICK_NAV,UI_CLICK_BACK,UI_CLICK_BACK_SMALL} ui_click_kind_t;
enum {EPD_ROT_LANDSCAPE,EPD_ROT_PORTRAIT,EPD_ROT_INVERTED_LANDSCAPE,EPD_ROT_INVERTED_PORTRAIT};
static int rotation;
#define W 1216
#define H 684
#define BYTES (W*H/2)
static uint8_t frame[BYTES],saved[BYTES],again[BYTES];
static int live,allocs,max_bytes,fail_after=-1;
static size_t source_reads;
static size_t current_bytes,peak_bytes;
static struct {void *ptr;size_t bytes;} memory[16];
static bool oom,fast;
static int epd_width(void){return W;}
static int epd_height(void){return H;}
static int epd_get_rotation(void){return rotation;}
static int epd_rotated_display_width(void){return rotation&1?H:W;}
static int epd_rotated_display_height(void){return rotation&1?W:H;}
static uint8_t epd_get_pixel(int x,int y,int w,int h,const uint8_t *fb){++source_reads;assert(w==W&&h==H&&x>=0&&x<w&&y>=0&&y<h);unsigned value=fb[(size_t)y*w/2+x/2];return (uint8_t)((x&1?value>>4:value&15)<<4);}
static void epd_draw_pixel(int x,int y,uint8_t gray,uint8_t *fb){
 if(x<0||y<0||x>=epd_rotated_display_width()||y>=epd_rotated_display_height())return;
 int px=x,py=y;
 switch(rotation){case 1:px=W-y-1;py=x;break;case 2:px=W-x-1;py=H-y-1;break;case 3:px=y;py=H-x-1;break;}
 size_t i=(size_t)py*W/2+px/2;unsigned shift=(px&1)*4;
 fb[i]=(uint8_t)((fb[i]&~(15u<<shift))|((gray>>4)<<shift));
}
static void epd_fill_rect(EpdRect r,uint8_t gray,uint8_t *fb){for(int y=0;y<r.height;++y)for(int x=0;x<r.width;++x)epd_draw_pixel(r.x+x,r.y+y,gray,fb);}
static void epd_draw_circle(int cx,int cy,int r,uint8_t gray,uint8_t *fb){for(int y=-r;y<=r;++y)for(int x=-r;x<=r;++x)if(x*x+y*y<=r*r&&x*x+y*y>(r-1)*(r-1))epd_draw_pixel(cx+x,cy+y,gray,fb);}
static void epd_fill_circle(int cx,int cy,int r,uint8_t gray,uint8_t *fb){for(int y=-r;y<=r;++y)for(int x=-r;x<=r;++x)if(x*x+y*y<=r*r)epd_draw_pixel(cx+x,cy+y,gray,fb);}
static void ui_fill_round_rect(uint8_t *fb,EpdRect r,int radius,uint8_t gray){
 for(int y=0;y<r.height;++y)for(int x=0;x<r.width;++x){
  int dx=x<radius?radius-x:x>=r.width-radius?x-(r.width-radius-1):0;
  int dy=y<radius?radius-y:y>=r.height-radius?y-(r.height-radius-1):0;
  if(dx*dx+dy*dy<=radius*radius)epd_draw_pixel(r.x+x,r.y+y,gray,fb);
 }
}
static uint8_t ui_contrast_gray(uint8_t v){return v;}
static void ui_nav_status(uint8_t *fb){epd_fill_rect((EpdRect){36,29,52,16},0,fb);}
static bool app_settings_main_fast_refresh(void){return fast;}
static void *heap_caps_malloc(size_t n,unsigned caps){
 assert(caps==3&&n<=114228);++allocs;if((int)n>max_bytes)max_bytes=(int)n;
 if(oom||fail_after==0)return NULL;if(fail_after>0)--fail_after;
 void *p=malloc(n);assert(p);++live;current_bytes+=n;if(current_bytes>peak_bytes)peak_bytes=current_bytes;
 for(int i=0;i<16;++i)if(!memory[i].ptr){memory[i].ptr=p;memory[i].bytes=n;return p;}abort();
}
static void heap_caps_free(void *p){
 if(!p)return;for(int i=0;i<16;++i)if(memory[i].ptr==p){--live;current_bytes-=memory[i].bytes;memory[i].ptr=NULL;free(p);assert(live>=0);return;}abort();
}
'''
code+='static void ui_fill_rect_fast(EpdRect r,uint8_t gray,uint8_t *fb){epd_fill_rect(r,gray,fb);}\n'
header=(root/'main/ui/ui_quick_menu.h').read_text()
code+='\n'.join(re.findall(r'^#define UI_QUICK[^\n]*',header,re.M))+'\n'
for name in ('ui_read_pixel','ui_image_bw_rect','icon_nibble','ui_draw_icon'):
    code+=function('main/ui/ui_kit.c',name)
for name in ('clamp_radius','draw_arc','ui_draw_round_rect','ui_inset_rect','ui_draw_control_frame'):
    code+=function('main/ui/ui_kit.c',name)
code+='static uint8_t pixel(uint8_t *fb,int x,int y){return ui_read_pixel(fb,x,y)*17u;}\n'
code+=body('main/ui/ui_glass.c')+body('main/ui/ui_quick_menu.c')+body('main/ui/ui_click_feedback.c')
code+='#define UI_NAV_BACK_ICON_PX 48\n#define READER_SHEET_BACK_PX 52\n'
code+=function('main/ui/ui_nav.c','ui_nav_back')+function('main/apps/app_book.c','draw_sheet_back')
code+=r'''
static void patterned(void){for(size_t i=0;i<BYTES;++i)frame[i]=(uint8_t)(i*13u+7u);memcpy(saved,frame,BYTES);}
int main(int argc,char **argv){
 // 阅读背景只略增同一七抽头的模糊；普通路径像素/缓存和控件仍按原规则。
 // Slight reader frost reuses seven taps; ordinary pixels/caches and controls retain their rules.
 rotation=3;memset(frame,255,BYTES);
 EpdRect frost_area={100,300,200,50};
 for(int y=300;y<350;++y)for(int x=100;x<300;++x)epd_draw_pixel(x,y,(x%8<2)?0:255,frame);
 memcpy(saved,frame,BYTES);assert(ui_glass_panel(frame,frost_area,0,0,0));
 int old_variation=0,new_variation=0;
 for(int x=130;x<270;++x)old_variation+=abs((int)pixel(frame,x,325)-pixel(frame,x+1,325));
 memcpy(frame,saved,BYTES);int before_frost=allocs;
 assert(ui_glass_panel_frost(frame,frost_area,0,0,0,true)&&allocs==before_frost+1);
 for(int x=130;x<270;++x)new_variation+=abs((int)pixel(frame,x,325)-pixel(frame,x+1,325));
 fprintf(stderr,"reader frost variation %d -> %d\n",old_variation,new_variation);assert(new_variation<old_variation);
 for(int y=0;y<1216;++y)for(int x=0;x<684;++x)
  if(x<100||x>=300||y<300||y>=350)assert(pixel(frame,x,y)==pixel(saved,x,y));

 // 优化前的完整像素校验；短图、边界夹取和四旋转必须逐位一致，每个源像素只读取一次。
 // Pre-optimization whole-frame goldens cover short panels, clamped edges and four rotations; read each source pixel only once.
 const EpdRect cases[]={{18,88,648,168},{31,102,108,108},{3,9,3,1},{1,2,11,2},{0,0,1,1},{20,44,41,7},{20,44,41,8}};
 const int radius[]={56,54,1,8,1,10,10},white[]={25,25,0,100,-1,101,25};
 const uint32_t golden[]={2950348408u,3794443602u,1916862657u,3172911537u};
 for(rotation=0;rotation<4;++rotation){
  uint32_t hash=2166136261u;
  for(unsigned i=0;i<sizeof(cases)/sizeof(cases[0]);++i){
   patterned();size_t reads=source_reads;
   assert(ui_glass_panel(frame,cases[i],radius[i],white[i],160));
   assert(source_reads-reads==(size_t)cases[i].width*cases[i].height);
   for(size_t j=0;j<BYTES;++j){hash^=frame[j];hash*=16777619u;}
  }
  assert(hash==golden[rotation]);
 }
 for(rotation=0;rotation<4;++rotation){
  // 检查实际收藏图标像素，不能只验证选择了哪个枚举。
  // Check actual favorite pixels, not merely which enum was selected.
  memset(frame,255,BYTES);ui_draw_icon(frame,620,114,48,UI_ICON_BOOKMARK,0x38);
  assert(ui_read_pixel(frame,620,110)==15);
  memcpy(saved,frame,BYTES);ui_draw_icon(frame,620,114,48,UI_ICON_ANIMATE_BOOKMARK_PLUS,0x38);
  assert(ui_read_pixel(frame,620,110)<=4);
  assert(ui_read_pixel(frame,616,110)<=4&&ui_read_pixel(frame,624,110)<=4);
  assert(ui_read_pixel(frame,620,106)<=4&&ui_read_pixel(frame,620,114)<=4);
  assert(ui_read_pixel(frame,596,90)==15);
  for(int y=0;y<epd_rotated_display_height();++y)for(int x=0;x<epd_rotated_display_width();++x)
   if(x<584||x>=648||y<76||y>=146)assert(ui_read_pixel(frame,x,y)==ui_read_pixel(saved,x,y));
  for(int policy=0;policy<3;++policy){
   patterned();int before=allocs;ui_quick_menu_draw(frame,true,false,false);
   assert(allocs==before+7&&live==2&&current_bytes==113616&&peak_bytes<=203688);int grays=0;
   for(int y=0;y<epd_rotated_display_height();++y)for(int x=0;x<epd_rotated_display_width();++x){
    bool mask=y<76||(x>=18&&x<666&&y>=88&&y<256);
    uint8_t p=pixel(frame,x,y);
    if(!mask)assert(p==pixel(saved,x,y));
    else grays+=p!=0&&p!=255;
   }
   assert(grays>1000);
   assert(pixel(frame,18,88)==pixel(saved,18,88)&&pixel(frame,665,88)==pixel(saved,665,88));
   memcpy(again,frame,BYTES);memcpy(frame,saved,BYTES);ui_quick_menu_draw(frame,true,false,false);assert(!memcmp(frame,again,BYTES));
   // 每次触控只恢复圆键，底图/其他圆键不变；连按不分配、不重算玻璃。
   // Each contact restores only its key; rapid presses allocate nothing or regenerate glass.
   int input_allocs=allocs;
   for(int button=0;button<4;++button){
    EpdRect dirty;int cx=(button*2+1)*UI_LOCK_WIDTH/8;
    for(int tap=0;tap<20;++tap){
     assert(ui_quick_menu_draw_button(frame,button,button==0,true,&dirty));
     assert(dirty.x==cx-54&&dirty.y==102&&dirty.width==108&&dirty.height==108);
     assert(memcmp(frame,again,BYTES));
     if(tap==0)for(int y=0;y<epd_rotated_display_height();++y)for(int x=0;x<epd_rotated_display_width();++x)
      if(x<dirty.x||x>=dirty.x+dirty.width||y<dirty.y||y>=dirty.y+dirty.height)
       assert(ui_read_pixel(frame,x,y)==ui_read_pixel(again,x,y));
     assert(ui_quick_menu_draw_button(frame,button,button==0,false,&dirty));
     assert(!memcmp(frame,again,BYTES));
    }
    if(button<2){
     assert(ui_quick_menu_draw_button(frame,button,button!=0,false,&dirty));
     assert(memcmp(frame,again,BYTES));
     assert(ui_quick_menu_draw_button(frame,button,button==0,false,&dirty));
     assert(!memcmp(frame,again,BYTES));
    }
   }
   assert(allocs==input_allocs&&current_bytes==113616);
   // 仅顶部缓存即可精确收起，拒绝另一帧或旋转后的缓存；收起与点按都不分配。
   // A top-only snapshot restores exactly; reject other frames/rotations and allocate nothing on close or presses.
   assert(!ui_quick_menu_restore(again));
   int saved_rotation=rotation;rotation=(rotation+1)%4;assert(!ui_quick_menu_restore(frame));rotation=saved_rotation;
   assert(ui_quick_menu_restore(frame)&&allocs==input_allocs&&!memcmp(frame,saved,BYTES));
   ui_quick_menu_end();ui_quick_menu_end();assert(!live&&!current_bytes&&!ui_quick_menu_restore(frame));

  }
  for(int failure=0;failure<7;++failure){
   patterned();fail_after=failure;ui_quick_menu_draw(frame,true,false,false);fail_after=-1;
   int before=allocs;EpdRect dirty;
   assert(ui_quick_menu_draw_button(frame,2,false,true,&dirty));
   assert(ui_quick_menu_draw_button(frame,2,false,false,&dirty)&&allocs==before);
   ui_quick_menu_end();assert(!live&&!current_bytes);
   for(int y=UI_QUICK_HEIGHT;y<epd_rotated_display_height();++y)for(int x=0;x<epd_rotated_display_width();++x)
    assert(pixel(frame,x,y)==pixel(saved,x,y));
  }
  max_bytes=0;
  for(fast=false;;fast=true){
   patterned();ui_click_feedback_begin(frame);
   ui_click_feedback_register(again,(EpdRect){26,82,64,64},UI_CLICK_BACK,UI_ICON_ANIMATE_CIRCLE_CHEVRON_LEFT);
   EpdRect r;assert(!ui_click_feedback_press(frame,58,114,&r));
   ui_click_feedback_register(frame,(EpdRect){26,82,64,64},UI_CLICK_BACK,UI_ICON_ANIMATE_CIRCLE_CHEVRON_LEFT);
   assert(ui_click_feedback_press(frame,58,114,&r)&&live==1&&max_bytes==4096);
   assert(memcmp(frame,saved,BYTES));assert(!ui_click_feedback_press(frame,58,114,&r));
   for(int y=0;y<epd_rotated_display_height();++y)for(int x=0;x<epd_rotated_display_width();++x)
    if(x<26||x>=90||y<82||y>=146)assert(pixel(frame,x,y)==pixel(saved,x,y));
   assert(!ui_click_feedback_cancel_at(58,114)&&ui_click_feedback_cancel_at(100,114));
   assert(ui_click_feedback_release(frame,&r)&&!live&&!memcmp(frame,saved,BYTES));
   assert(!ui_click_feedback_release(frame,&r));
   oom=true;assert(!ui_click_feedback_press(frame,58,114,&r)&&!live&&!memcmp(frame,saved,BYTES));oom=false;
   ui_click_feedback_reset();assert(!ui_click_feedback_press(frame,58,114,&r));
   patterned();ui_click_feedback_begin(frame);
   ui_click_feedback_register(frame,(EpdRect){26,74,64,64},UI_CLICK_BACK_SMALL,UI_ICON_ANIMATE_CIRCLE_CHEVRON_LEFT);
   assert(ui_click_feedback_press(frame,58,106,&r));
   assert(r.x>=26&&r.y>=74&&r.x+r.width<=90&&r.y+r.height<=138);
   for(int yy=0;yy<epd_rotated_display_height();++yy)for(int xx=0;xx<epd_rotated_display_width();++xx)
    if(xx<26||xx>=90||yy<74||yy>=138)assert(pixel(frame,xx,yy)==pixel(saved,xx,yy));
   assert(ui_click_feedback_release(frame,&r)&&!live&&!memcmp(frame,saved,BYTES));
   if(fast)break;
  }
 }
 rotation=3;patterned();ui_click_feedback_begin(frame);
 ui_click_feedback_register(frame,(EpdRect){55,1111,60,60},UI_CLICK_NAV,UI_ICON_HOUSE);
 EpdRect r;assert(ui_click_feedback_press(frame,85,1188,&r));
 assert(!ui_click_feedback_cancel_at(90,1188)&&ui_click_feedback_cancel_at(258,1188));
 assert(ui_click_feedback_release(frame,&r)&&!memcmp(frame,saved,BYTES));
 assert(ui_click_feedback_press(frame,85,1188,&r));ui_click_feedback_begin(frame);assert(!live&&!ui_click_feedback_release(frame,&r));
 // 三种主页模式的底栏共用灰底：按压不能改外沿、文字、横条或相邻图标。
 // All three main modes share a gray footer: presses must preserve its perimeter, labels, marker and neighboring icons.
 static const ui_icon_t tabs[]={UI_ICON_HOUSE,UI_ICON_LIBRARY_BIG,UI_ICON_FOLDER_OPEN,UI_ICON_SLIDERS_HORIZONTAL};
 for(int tab=0;tab<4;++tab)for(int policy=0;policy<3;++policy)for(int edge=0;edge<2;++edge){
  fast=policy==1;memset(frame,0xee,BYTES);
  int cx=(tab*2+1)*684/8;
  epd_fill_rect((EpdRect){0,1097,684,5},0,frame);
  epd_fill_rect((EpdRect){0,1170,684,2},0x70,frame);
  ui_draw_icon(frame,cx,1141,44,tabs[tab],0);
  memcpy(saved,frame,BYTES);ui_click_feedback_begin(frame);
  ui_click_feedback_register(frame,(EpdRect){cx-30,1111,60,60},UI_CLICK_NAV,tabs[tab]);
  assert(!ui_click_feedback_press(frame,cx,UI_NAV_TOUCH_TOP-1,&r));
  assert(!ui_click_feedback_press(frame,cx,1216,&r));
  assert(!ui_click_feedback_press(frame,-1,UI_NAV_TOUCH_TOP,&r));
  assert(!ui_click_feedback_press(frame,684,UI_NAV_TOUCH_TOP,&r));
  assert(ui_click_feedback_press(frame,cx,edge?UI_NAV_TOUCH_TOP:1188,&r));
  assert(!ui_click_feedback_cancel_at(cx,edge?UI_NAV_TOUCH_TOP+12:1188));
  for(int y=0;y<1216;++y)for(int x=0;x<684;++x){
   if(x<cx-25||x>=cx+25||y<1118||y>=1168)assert(pixel(frame,x,y)==pixel(saved,x,y));
  }
  if(policy==0)memcpy(again,frame,BYTES);else assert(!memcmp(frame,again,BYTES));
  assert(r.y>=UI_NAV_MARKER_SCAN_END&&r.y+r.height<=1170);
  EpdRect pressed=r;
  assert(ui_click_feedback_release(frame,&r)&&!memcmp(&r,&pressed,sizeof(r))&&!live&&!memcmp(frame,saved,BYTES));
 }


 // 原来50px放大姿态立即显示，65ms后复位；不需要切换到另一个页面。
 // The original 50px enlarged pose appears immediately and settles after 65ms without navigation.
 for(int policy=0;policy<3;++policy)for(int tab=0;tab<4;++tab){
  fast=policy==1;memset(frame,0xee,BYTES);ui_click_feedback_begin(frame);
  for(int i=0;i<4;++i){int cx=(i*2+1)*684/8;ui_draw_icon(frame,cx,1141,44,tabs[i],0);
   ui_click_feedback_register(frame,(EpdRect){cx-30,1111,60,60},UI_CLICK_NAV,tabs[i]);}
  memcpy(saved,frame,BYTES);int before=allocs;
  ui_click_feedback_nav_request(tab);assert(allocs==before&&!live&&!memcmp(saved,frame,BYTES));
  assert(ui_click_feedback_nav_tick(frame,100,&r)&&live==1&&ui_click_feedback_active());
  assert(r.y>=UI_NAV_MARKER_SCAN_END&&r.y+r.height<=1170);
  assert(!ui_click_feedback_nav_tick(frame,164,&r));
  assert(ui_click_feedback_nav_tick(frame,165,&r)&&!live&&!ui_click_feedback_nav_pending()&&!memcmp(frame,saved,BYTES));
  assert(!ui_click_feedback_nav_tick(frame,1000,&r));
 }
 // 第一段效果没结束就再点同一图标，像素必须产生新变化，不能被相同差分吞掉。
 // A repeated tap before settling must change pixels rather than disappear as an identical differential.
 for(int tab=0;tab<4;++tab){
  ui_click_feedback_nav_request(tab);assert(ui_click_feedback_nav_tick(frame,0,&r));
  memcpy(again,frame,BYTES);
  for(int repeat=0;repeat<20;++repeat){
   ui_click_feedback_nav_request(tab);assert(!live);assert(ui_click_feedback_nav_tick(frame,repeat+1,&r));
   assert(memcmp(again,frame,BYTES));memcpy(again,frame,BYTES);
  }
  assert(ui_click_feedback_nav_tick(frame,200,&r)&&!live&&!memcmp(frame,saved,BYTES));
 }
 // 切页的固定底栏可能保留物理按下姿态；新页完成后必须从同一控件原像素复位，不能截留放大图标。
 // Frozen navigation can retain the physical pressed pose; settle from the same control baseline after the page paint.
 ui_click_feedback_nav_request(0);assert(ui_click_feedback_nav_tick(frame,0,&r));
 memcpy(again,frame,BYTES);ui_click_feedback_begin(frame);assert(live==1&&!memcmp(frame,saved,BYTES));
 epd_fill_rect((EpdRect){0,0,684,1000},0xa0,frame);
 for(int y=1111;y<1170;++y)for(int x=0;x<684;++x)epd_draw_pixel(x,y,pixel(again,x,y),frame);
 assert(ui_click_feedback_nav_tick(frame,200,&r)&&!live);
 for(int y=0;y<1216;++y)for(int x=0;x<684;++x)
  if(pixel(frame,x,y)!=(y<1000?0xaa:pixel(saved,x,y))) {fprintf(stderr,"frozen mismatch %d,%d actual %02x expected %02x\n",x,y,pixel(frame,x,y),y<1000?0xaa:pixel(saved,x,y));assert(false);}
 // 加载中底栏还没有完成注册，同样给到即时反馈；不等待下一页。
 // Loading before footer registration still gives immediate feedback without waiting for a new page.
 memset(frame,0xee,BYTES);ui_click_feedback_begin(frame);
 for(int i=0;i<4;++i)ui_draw_icon(frame,(i*2+1)*684/8,1141,44,tabs[i],0);
 memcpy(saved,frame,BYTES);
 ui_click_feedback_nav_request(2);assert(ui_click_feedback_nav_tick(frame,0,&r)&&live==1);
 assert(ui_click_feedback_nav_tick(frame,65,&r)&&!live&&!memcmp(frame,saved,BYTES));
 ui_click_feedback_nav_request(2);assert(ui_click_feedback_nav_tick(frame,0,&r));
 assert(ui_click_feedback_nav_finish(frame,&r)&&!live&&!ui_click_feedback_nav_pending()&&!memcmp(frame,saved,BYTES));
 assert(!ui_click_feedback_nav_finish(frame,&r));
 ui_click_feedback_nav_request(1);oom=true;assert(!ui_click_feedback_nav_tick(frame,0,&r)&&!live&&!ui_click_feedback_nav_pending());oom=false;
 ui_click_feedback_nav_request(0);assert(ui_click_feedback_nav_tick(frame,0,&r));ui_click_feedback_reset();assert(!live&&!ui_click_feedback_nav_tick(frame,500,&r));
 // 实际题头/阅读面板绘制均使用同一带圆环图标；按压不能在圆外增加方框。
 // Real headers/reader sheets share the complete icon; pressing must not add a rectangle outside the circle.
 for(int sheet=0;sheet<2;++sheet)for(int mode=0;mode<3;++mode){
  fast=mode==1;memset(frame,255,BYTES);ui_click_feedback_begin(frame);
  int cx=sheet?64:58,cy=sheet?257:114;
  if(sheet)draw_sheet_back(frame,200);else ui_nav_back(frame,36,79);
  memcpy(saved,frame,BYTES);
  assert(ui_click_feedback_press(frame,cx,cy,&r));
  for(int y=0;y<1216;++y)for(int x=0;x<684;++x){
   int dx=x-cx,dy=y-cy;
   if(dx*dx+dy*dy>29*29)assert(pixel(frame,x,y)==pixel(saved,x,y));
  }
  assert(ui_click_feedback_release(frame,&r)&&!live&&!memcmp(frame,saved,BYTES));
 }
 // 原尺寸保持精确位图；缩放产生中间覆盖率且不改变空白与满墨端点。
 // Native sizes remain exact; scaling produces intermediate coverage with preserved blank/full endpoints.
 uint8_t han[UI_HANZI_RECORD]={0};han[2]=han[3]=24;int intermediate=0;
 for(int y=0;y<24;++y)for(int x=0;x<24;++x)if(x==y){unsigned b=(unsigned)y*24+x;han[5+b/8]|=1u<<(b&7);}
 for(int y=0;y<24;++y)for(int x=0;x<24;++x)assert(ui_hanzi_coverage(han,24,24,x,y)==(x==y?255:0));
 for(int y=0;y<39;++y)for(int x=0;x<39;++x){unsigned v=ui_hanzi_coverage(han,39,39,x,y);intermediate+=v>0&&v<255;}
 assert(intermediate>100);
 memset(han+5,0,sizeof(han)-5);for(int y=0;y<72;++y)for(int x=0;x<72;++x)assert(!ui_hanzi_coverage(han,72,72,x,y));
 memset(han+5,255,sizeof(han)-5);assert(ui_hanzi_coverage(han,39,39,19,19)==255);
 // 可选原生帧输出：底图已有的状态栏替代测试桩，玻璃/图标全部来自实际绘制代码。
 // Optional native export: retain the source status strip instead of its test shim; glass/icons use actual drawing code.
 for(int arg=1;arg+1<argc;arg+=2){
  rotation=3;oom=false;fail_after=-1;
  FILE *in=fopen(argv[arg],"rb");assert(in);
  for(int y=0;y<1216;++y)for(int x=0;x<684;++x){int v=fgetc(in);assert(v>=0);epd_draw_pixel(x,y,v,frame);}fclose(in);
  memcpy(again,frame,BYTES);ui_quick_menu_draw(frame,true,false,false);
  for(int y=0;y<76;++y)for(int x=0;x<684;++x)epd_draw_pixel(x,y,pixel(again,x,y),frame);
  FILE *out=fopen(argv[arg+1],"wb");assert(out);fprintf(out,"P5\n684 1216\n255\n");
  for(int y=0;y<1216;++y)for(int x=0;x<684;++x)fputc(pixel(frame,x,y),out);fclose(out);
  ui_quick_menu_end();assert(!live&&!current_bytes);
 }
 ui_quick_menu_end();assert(!live&&!current_bytes);
 for(rotation=1;rotation<4;rotation+=2){
  patterned();ui_quick_menu_draw(frame,true,false,true);
  assert(ui_quick_menu_restore(frame)&&!memcmp(frame,saved,BYTES));
  ui_quick_menu_end();assert(!live);
 }
 puts("PASS: actual liquid-glass quick layer bounds temporary PSRAM to 203688 bytes; 113616-byte top/control cache frees on close, 80 local press/release and status changes allocate nothing and exactly restore artwork; grayscale in all modes, four rotations, deterministic rendering and every allocation failure; 12 navigation presses preserve gray backgrounds/labels/marker/neighbors in all modes; click cache <=4096 bytes restores exact pixels, ignores prefetch and tolerates OOM; Han scaling retains native bits and smooths enlarged edges");
 return 0;
}
'''
with tempfile.TemporaryDirectory() as tmp:
    src=Path(tmp)/'test.c';src.write_text(code)
    exe=Path(tmp)/'test'
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-g','-fsanitize=address,undefined',
                    '-I'+str(root/'main'),str(src),str(root/'main/ui/ui_image_dither.c'),'-lm','-o',str(exe)],check=True)
    subprocess.run([str(exe)]+sys.argv[1:],check=True)

loop=(root/'main/app/app_loop.c').read_text()
press=loop[loop.index('bool nav_press_owned='):loop.index('if (!power_input_owned && !quick_input_owned && !nav_press_owned')]
assert 'ui_click_feedback_nav_request(tab);' in press
assert 'ui_click_feedback_nav_tick' in press
assert '!ctx.request_app' not in press
assert loop.index('ui_click_feedback_nav_tick(ctx.fb,esp_timer_get_time()/1000,&click_area)') < loop.index('switch_to(&ctx, &current, next);')
for path,name in [('main/ui/ui_nav.c','ui_nav_back'),('main/apps/app_book.c','draw_sheet_back')]:
 draw=function(path,name)
 assert 'UI_ICON_ANIMATE_CIRCLE_CHEVRON_LEFT' in draw
 assert 'epd_draw_circle' not in draw and 'ui_draw_round_rect' not in draw
