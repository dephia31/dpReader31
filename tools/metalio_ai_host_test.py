#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Execute actual AI debounce, chooser raster/input, persistent setting and action dispatch."""
from pathlib import Path
import re,subprocess,tempfile
from home_cover_cache_host_test import function
r=Path(__file__).resolve().parents[1]
common=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
'''
sampler=common+r'''
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(x) ((void)(x))
#define portEXIT_CRITICAL(x) ((void)(x))
typedef int portMUX_TYPE;
enum {METALIO_AI_NONE,METALIO_AI_SHORT,METALIO_AI_LONG};
'''+re.sub(r'^#include.*\n','',(r/'components/metalio_hw/metalio_ai_key.c').read_text(),flags=re.M)+r'''
static void reset(void){memset(&ai,0,sizeof(ai));}
static void press(int ms){metalio_ai_sample(true,ms);metalio_ai_sample(true,ms+25);}
static void release(int ms){metalio_ai_sample(false,ms);metalio_ai_sample(false,ms+25);}
int main(void){
 reset();press(0);metalio_ai_sample(true,1000);release(1100);assert(!metalio_take_ai()); // boot-held suppresses
 reset();release(0);metalio_ai_sample(true,50);release(60);assert(!metalio_take_ai()); // bounce
 press(100);release(200);assert(metalio_take_ai()==METALIO_AI_SHORT&&!metalio_take_ai());
 press(300);metalio_ai_sample(true,824);assert(!metalio_take_ai());metalio_ai_sample(true,825);
 assert(metalio_take_ai()==METALIO_AI_LONG);metalio_ai_sample(true,2000);release(2100);assert(!metalio_take_ai());
 press(2200);metalio_ai_clear();metalio_ai_sample(true,3500);release(3600);assert(!metalio_take_ai());
 press(3700);release(3800);assert(metalio_take_ai()==METALIO_AI_SHORT);
 for(int i=0;i<12;i++){press(4000+i*100);release(4050+i*100);}assert(ai.count==8);
 for(int i=0;i<8;i++)assert(metalio_take_ai()==METALIO_AI_SHORT);assert(!metalio_take_ai());
 puts("PASS: GPIO61 AI debounce,boot/sleep-held suppression,one500ms long event,no release tap,bounded8-event FIFO");
}
'''
enum='typedef enum {APP_AI_REFRESH,APP_AI_BOOKMARK,APP_AI_FRONTLIGHT,APP_AI_HOME,APP_AI_DISABLED,APP_AI_ACTION_COUNT}app_ai_action_t;\n'
ui=common+enum+r'''
#define W 1216
#define H 684
#define EPD_DRAW_ALIGN_CENTER 1
#define EPD_DRAW_ALIGN_LEFT 0
#define UI_GRAY_WHITE 255
#define UI_GRAY_BLACK 0
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
typedef struct{int x,y,width,height;}EpdRect;
typedef struct{bool touched;unsigned count;int x,y;}cst836u_touch_t;
typedef struct{const cst836u_touch_t*touch;bool pressed,released;}app_ctx_t;
typedef int ui_icon_t;
enum {UI_ICON_REFRESH_CW,UI_ICON_BOOKMARK,UI_ICON_LIGHTBULB,UI_ICON_HOUSE,UI_ICON_POWER};
static int rotation,allocs,live,draws,glass_calls;
static bool oom,glass_ok=true;
static uint8_t fb[W*H],original[W*H];
static int epd_rotated_display_width(void){return rotation%2?H:W;}
static int epd_rotated_display_height(void){return rotation%2?W:H;}
static int epd_get_rotation(void){return rotation;}
static size_t address(int x,int y){
 int px=x,py=y;
 if(rotation==1){px=W-y-1;py=x;}
 else if(rotation==2){px=W-x-1;py=H-y-1;}
 else if(rotation==3){px=y;py=H-x-1;}
 assert(px>=0&&px<W&&py>=0&&py<H);return (size_t)py*W+px;
}
static uint8_t ui_read_pixel(const uint8_t*f,int x,int y){return f[address(x,y)];}
static void epd_draw_pixel(int x,int y,uint8_t v,uint8_t*f){f[address(x,y)]=v>>4;}
static bool ui_rect_hit(EpdRect r,int x,int y){return x>=r.x&&x<r.x+r.width&&y>=r.y&&y<r.y+r.height;}
static void*heap_caps_calloc(size_t n,size_t sz,int flags){assert(flags==3&&n*sz==81648);++allocs;if(oom)return NULL;++live;return calloc(n,sz);}
static void heap_caps_free(void*p){if(p)--live;free(p);}
static void ui_fill_round_rect(uint8_t*f,EpdRect r,int rad,uint8_t v){(void)rad;draws++;for(int y=0;y<r.height;y++)for(int x=0;x<r.width;x++)epd_draw_pixel(r.x+x,r.y+y,v,f);}
static void ui_draw_round_rect(uint8_t*f,EpdRect r,int rad,uint8_t v){(void)rad;draws++;epd_draw_pixel(r.x,r.y,v,f);}
static bool ui_glass_panel_frost(uint8_t*f,EpdRect r,int rad,int white,int refract,bool reader){assert(rad==40&&white==52&&refract==160&&reader);glass_calls++;if(!glass_ok)return false;ui_fill_round_rect(f,r,rad,0xc0);return true;}
static void ui_text_fixed_vc(uint8_t*f,int x,int y,int px,const char*t,int align,bool inverted){(void)px;(void)t;(void)align;assert(!inverted);draws++;epd_draw_pixel(x,y,0,f);}
static void ui_draw_icon(uint8_t*f,int x,int y,int px,ui_icon_t icon,uint8_t v){(void)px;assert(icon>=0&&icon<=4);draws++;epd_draw_pixel(x,y,v,f);}
static void epd_draw_circle(int x,int y,int rad,uint8_t v,uint8_t*f){(void)rad;draws++;epd_draw_pixel(x,y,v,f);}
static void epd_fill_circle(int x,int y,int rad,uint8_t v,uint8_t*f){epd_draw_circle(x,y,rad,v,f);}
'''
ui+=re.sub(r'^#include.*\n|^#pragma once\n','',(r/'main/ui/ui_ai_menu.h').read_text(),flags=re.M)
ui+=re.sub(r'^#include.*\n','',(r/'main/ui/ui_ai_menu.c').read_text(),flags=re.M)
ui+=r'''
static int feed(ui_ai_menu_t*m,int x,int y,bool down,bool press,bool release,bool valid,unsigned count){
 cst836u_touch_t t={down,count,x,y};app_ctx_t c={&t,press,release};return ui_ai_menu_feed(m,&c,valid);
}
int main(void){
 for(rotation=0;rotation<4;rotation++)for(int fallback=0;fallback<2;fallback++){
  for(unsigned i=0;i<sizeof(fb);i++)fb[i]=i%16;memcpy(original,fb,sizeof(fb));
  ui_ai_menu_t m={0};oom=false;glass_ok=!fallback;int gc=glass_calls;
  assert(ui_ai_menu_begin(&m,fb,APP_AI_BOOKMARK,true)&&m.open&&live==1&&glass_calls==gc+1);
  assert(!ui_ai_menu_begin(&m,fb,APP_AI_REFRESH,false));
  assert(m.rect.y<200&&m.rect.width>600&&m.rect.height<300);
  assert(ui_read_pixel(fb,m.rect.x+m.rect.width/2,m.rect.y+32)==0); // title stays black
  for(int i=0;i<5;i++){
   EpdRect option=ui_ai_menu_option(&m,i);int cx=option.x+option.width/2;
   assert(option.y==ui_ai_menu_option(&m,0).y&&option.y>=m.rect.y&&option.y+option.height<=m.rect.y+m.rect.height);
   assert(ui_read_pixel(fb,cx,m.rect.y+174)==0); // labels remain below icons, black
   for(int y=m.rect.y+156;y<m.rect.y+190;y++)for(int x=option.x+2;x<option.x+option.width-2;x++)
    if(x!=cx||y!=m.rect.y+174)assert(ui_read_pixel(fb,x,y)==(fallback?14:12)); // no label backing
   if(i)assert(option.x>ui_ai_menu_option(&m,i-1).x+ui_ai_menu_option(&m,i-1).width);
   assert((ui_read_pixel(fb,cx+22,m.rect.y+106-22)==0)==(i==APP_AI_BOOKMARK));
  }
  int ac=allocs,dc=draws;assert(feed(&m,0,0,false,false,false,true,0)==UI_AI_NO_ACTION);
  for(int i=0;i<5;i++){
   EpdRect rr=ui_ai_menu_option(&m,i);int x=rr.x+80,y=rr.y+36;
   assert(feed(&m,x,y,true,true,false,true,1)==UI_AI_NO_ACTION);
   assert(feed(&m,x,y,false,false,true,true,0)==i);
   feed(&m,x,y,true,true,false,true,1);feed(&m,x+25,y,true,false,false,true,1);
   assert(feed(&m,x+25,y,false,false,true,true,0)==UI_AI_NO_ACTION);
   feed(&m,x,y,true,true,false,true,1);feed(&m,x,y,true,false,false,true,2);
   assert(feed(&m,x,y,false,false,true,true,0)==UI_AI_NO_ACTION);
  }
  EpdRect rr=ui_ai_menu_option(&m,0);
  feed(&m,rr.x+80,rr.y+36,true,true,false,true,1);feed(&m,0,0,false,false,false,false,0);
  assert(feed(&m,rr.x+80,rr.y+36,false,false,true,true,0)==UI_AI_NO_ACTION);
  feed(&m,4,4,true,true,false,true,1);assert(feed(&m,4,4,false,false,true,true,0)==UI_AI_CLOSE);
  assert(draws==dc&&allocs==ac&&glass_calls==gc+1); // zero work on input
  assert(ui_ai_menu_restore(&m,fb)&&!memcmp(fb,original,sizeof(fb)));ui_ai_menu_end(&m);assert(!m.open&&!live);
  oom=true;assert(!ui_ai_menu_begin(&m,fb,APP_AI_HOME,false)&&!memcmp(fb,original,sizeof(fb))&&!live);
 }
 puts("PASS: actual glass chooser black text without label backing,horizontal spaced options,inset selected dot,one blur,81648B bounded underlay,4 rotations,exact restore,zero input allocations/painting,row/gap/drag/multitouch/error/close/OOM boundaries");
}
'''
settings=common+enum+r'''
typedef int esp_err_t;typedef int nvs_handle_t;
#define ESP_OK 0
#define ESP_ERR_INVALID_ARG 99
#define NVS_NS "metalio_s31"
#define NVS_READWRITE 1
static app_ai_action_t s_ai_short=APP_AI_REFRESH;
static int open_error,set_error,commit_error,opens,closes,writes,stored;
static int nvs_open(const char*n,int flags,nvs_handle_t*h){assert(!strcmp(n,NVS_NS)&&flags==1);++opens;*h=1;return open_error;}
static int nvs_set_u8(nvs_handle_t h,const char*k,uint8_t a){assert(h==1&&!strcmp(k,"ai_short"));++writes;if(!set_error)stored=a;return set_error;}
static int nvs_commit(nvs_handle_t h){assert(h==1);return commit_error;}
static void nvs_close(nvs_handle_t h){assert(h==1);++closes;}
'''+function('app_settings_ai_short',r/'main/settings.c')+function('app_settings_set_ai_short',r/'main/settings.c')+r'''
int main(void){
 assert(app_settings_ai_short()==APP_AI_REFRESH);
 assert(app_settings_set_ai_short(APP_AI_REFRESH)==0&&!opens);
 assert(app_settings_set_ai_short((app_ai_action_t)9)==99&&!opens);
 open_error=1;assert(app_settings_set_ai_short(APP_AI_HOME)==1&&s_ai_short==0&&!closes);open_error=0;
 set_error=2;assert(app_settings_set_ai_short(APP_AI_HOME)==2&&s_ai_short==0&&closes==1);set_error=0;
 commit_error=3;assert(app_settings_set_ai_short(APP_AI_HOME)==3&&s_ai_short==0&&closes==2);commit_error=0;
 assert(app_settings_set_ai_short(APP_AI_HOME)==0&&s_ai_short==APP_AI_HOME&&stored==APP_AI_HOME&&closes==3);
 puts("PASS: S31 setting namespace,checked action IDs,no redundant writes,open/set/commit failures retain active mapping");
}
'''

dispatch=common+enum+r'''
typedef struct{int unused;}app_ctx_t;typedef struct{int unused;}app_desc_t;
typedef enum{APP_REDRAW_NONE,APP_REDRAW_PAGE,APP_REDRAW_FULL}app_redraw_t;
static app_desc_t app_book,other;
static app_ai_action_t chosen;
static bool reader,bookmark_ok=true;
static int marks,haptics,lamps,home;
static app_ai_action_t app_settings_ai_short(void){return chosen;}
static bool app_book_reader_body_visible(void){return reader;}
static bool bookmark_toggle(void){++marks;return bookmark_ok;}
static int metalio_haptic_click(void){++haptics;return 0;}
static void metalio_frontlight_toggle(void){++lamps;}
static void ui_nav_request(app_ctx_t*c,int tab){(void)c;assert(tab==0);++home;}
'''+function('app_book_ai_bookmark',r/'main/apps/app_book.c')+function('ai_short_action',r/'main/app/app_loop.c')+r'''
int main(void){app_ctx_t c={0};
 chosen=APP_AI_BOOKMARK;assert(ai_short_action(&c,&other)==APP_REDRAW_NONE&&!marks&&!haptics);
 assert(ai_short_action(&c,&app_book)==APP_REDRAW_NONE&&!marks);reader=true;
 assert(ai_short_action(&c,&app_book)==APP_REDRAW_PAGE&&marks==1&&haptics==1);
 bookmark_ok=false;assert(ai_short_action(&c,&app_book)==APP_REDRAW_PAGE&&marks==2&&haptics==1);
 chosen=APP_AI_REFRESH;assert(ai_short_action(&c,&other)==APP_REDRAW_FULL&&haptics==2);
 chosen=APP_AI_FRONTLIGHT;assert(ai_short_action(&c,&other)==APP_REDRAW_NONE&&lamps==1&&haptics==3);
 chosen=APP_AI_HOME;assert(ai_short_action(&c,&other)==APP_REDRAW_NONE&&home==1&&haptics==4);
 chosen=APP_AI_DISABLED;assert(ai_short_action(&c,&other)==APP_REDRAW_NONE&&home==1&&haptics==4&&lamps==1&&marks==2);
 puts("PASS: actual tap dispatcher and book wrapper share bookmark toggling,body/current-page guards,failure feedback,full/light/home/disabled actions");
}
'''
# Execute the exact load block too: old/missing/invalid records default safely; successful selections survive a fresh context.
load=(r/'main/settings.c').read_text()
load=load[load.index('    uint8_t ai_short=APP_AI_REFRESH;'):]
load=load[:load.index('\n',load.index('s_ai_short=nvs_get_u8'))]
load_fixture=r'''
static int read_error;
static int nvs_get_u8(nvs_handle_t h,const char*k,uint8_t*v){assert(h==1&&!strcmp(k,"ai_short"));*v=stored;return read_error;}
static void reload(void){nvs_handle_t h=1;
'''+load+'\n}\n'
settings=settings.replace('int main(void){',load_fixture+'int main(void){',1)
settings=settings.replace(' puts("PASS: S31 setting namespace',' s_ai_short=APP_AI_REFRESH;reload();assert(s_ai_short==APP_AI_HOME);stored=99;reload();assert(s_ai_short==APP_AI_REFRESH);stored=APP_AI_FRONTLIGHT;read_error=1;reload();assert(s_ai_short==APP_AI_REFRESH);\n puts("PASS: S31 setting namespace',1)
with tempfile.TemporaryDirectory() as d:
 for name,unit in [('sampler',sampler),('chooser',ui),('settings',settings),('dispatch',dispatch)]:
  p=Path(d)/name;p.with_suffix('.c').write_text(unit)
  subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-fsanitize=address,undefined',str(p.with_suffix('.c')),'-o',str(p)],check=True)
  subprocess.run([str(p)],check=True)
