#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Actual S31 panel overlays and body-cache fallback; no physical display or serial access."""
from pathlib import Path
import subprocess,tempfile
from home_cover_cache_host_test import function
root=Path(__file__).resolve().parents[1];book=root/'main/apps/app_book.c';source=book.read_text()
if 'static bool reader_panel_fast_present(' not in source:
 print('SKIP: S31 panel overlay is not used on this board');raise SystemExit(0)
unit=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#define UI_LOCK_WIDTH 684
#define UI_LOCK_HEIGHT 1216
#define UI_GRAY_WHITE 255
#define BYTES (684*1216/2)
#define ESP_LOGI(...) ((void)0)
#define TAG "test"
typedef struct{int x,y,width,height;}EpdRect;
typedef enum{APP_REDRAW_NONE,APP_REDRAW_AREA,APP_REDRAW_PAGE,APP_REDRAW_FULL}app_redraw_t;
enum EpdDrawError{EPD_DRAW_SUCCESS,EPD_DRAW_OTHER_ERROR};
enum{READING,SHELF,MODE_GL16};
typedef enum{READER_PANEL_NONE,READER_PANEL_TOOLS,READER_PANEL_FONT_SETTINGS,READER_PANEL_REFRESH_SETTINGS,READER_PANEL_STATS,READER_PANEL_BACKGROUND,READER_PANEL_BACKGROUND_PREVIEW}reader_panel_t;
typedef struct{uint8_t*back_fb;}hl_t;
typedef struct{uint8_t*fb;hl_t*hl;bool request_app;}app_ctx_t;
static uint8_t front[BYTES],back[BYTES],original[BYTES],cached_body[BYTES];
static hl_t hl={back};static app_ctx_t ctx={front,&hl,false};
static int s_view=READING,s_mode,s_du_count,s_presented_view;
static const char*s_text="book";static size_t s_page=2,s_chapter=3;
static bool s_clear_confirm,s_reader_fullscreen=true,s_reader_text_frame,s_fonts_reload_pending;
static bool s_reader_cleanup,s_reader_turn_pending,s_water_turn_pending,s_reader_footer_pending;
static char s_reader_notice[20];static reader_panel_t s_reader_panel;
static int images,rotation=3,draws,pushes,errors,prepared,ended,full_draws,outer_draws,stores,cancels,invalidates;
static uint32_t generation=9;static bool background,draw_lock,failed,cache_ready,cache_oom;
static int font_top=580,settings_top=580;
static EpdRect area;
static size_t fb_bytes(void){return BYTES;}
static unsigned book_layout_page_image_count(size_t page){assert(page==s_page);return images;}
static uint32_t book_layout_generation(void){return generation;}
static int epd_get_rotation(void){return rotation;}
static const char*app_settings_book_background_path(void){return background?"wallpaper":"";}
static int reader_font_top(void){return font_top;}
static int reading_settings_top(void){return settings_top;}
static bool display_page_prepare_begin(void){++prepared;return false;}
static void display_page_prepare_end(bool held){assert(!held);++ended;}
static int64_t esp_timer_get_time(void){return 123456;}
static void lock_draw(void){assert(!draw_lock);draw_lock=true;}
static void unlock_draw(void){assert(draw_lock);draw_lock=false;}
static void draw_reader_panel(uint8_t*fb){
 assert(draw_lock);++draws;int top=s_reader_panel==READER_PANEL_TOOLS?1096:s_reader_panel==READER_PANEL_FONT_SETTINGS?font_top:s_reader_panel==READER_PANEL_REFRESH_SETTINGS?settings_top:768;
 memset(fb+top*684/2,0,(1216-top)*684/2);
}
static enum EpdDrawError update_display_reader_panel(hl_t*h,EpdRect a){
 assert(h==&hl&&!draw_lock);area=a;++pushes;
 if(!failed)memcpy(back,front,BYTES);return failed?EPD_DRAW_OTHER_ERROR:EPD_DRAW_SUCCESS;
}
static void guard_draw_result(hl_t*h,enum EpdDrawError err){assert(h==&hl);errors+=err!=EPD_DRAW_SUCCESS;}
static size_t cached_page;static uint32_t cached_generation;
static const uint8_t*reader_page_cache_find(size_t page,uint32_t gen,size_t bytes){assert(bytes==BYTES);return cache_ready&&page==cached_page&&gen==cached_generation?cached_body:NULL;}
static bool reader_page_cache_store(size_t page,uint32_t gen,const uint8_t*fb,size_t bytes,size_t focus){
 assert(bytes==BYTES&&page==focus);++stores;if(cache_oom)return false;
 memcpy(cached_body,fb,bytes);cached_page=page;cached_generation=gen;cache_ready=true;return true;
}
static void draw_reader(uint8_t*fb,size_t page){assert(page==s_page);++full_draws;memset(fb,0x76,BYTES);}
static EpdRect body_rect(void){return(EpdRect){12,80,660,1110};}
static void ui_fill_rect_fast(EpdRect r,uint8_t v,uint8_t*fb){assert(r.x==0&&r.width==684);memset(fb+r.y*684/2,v,r.height*684/2);}
static void draw_reader_outer(uint8_t*fb,size_t page){assert(page==s_page);++outer_draws;fb[BYTES-1]=0x43;}
static int s_next_page,s_prep_page;static uint32_t s_next_generation;static uint8_t*s_next_fb;
static struct{int value;}s_prep_cursor;
static void reader_page_cache_cancel(void){++cancels;}
static void invalidate_prep(void){++invalidates;}
'''
a=source.rindex('static struct {',0,source.index('static void reader_panel_record('));b=source.index('} s_reader_panel_frame;',a)+len('} s_reader_panel_frame;')
unit+=source[a:b]+'\n'
for name in ['reader_panel_record','reader_panel_short_ready','reader_panel_fast_present','draw_reader_cached','pause_reader_prep']:unit+=function(name,book)+'\n'
unit+=r'''
static void reset(reader_panel_t prior){
 s_view=READING;s_text="book";s_page=2;s_chapter=3;generation=9;rotation=3;s_reader_fullscreen=true;
 images=0;background=false;s_clear_confirm=false;s_reader_notice[0]=0;s_reader_panel=prior;
 s_reader_cleanup=s_reader_turn_pending=s_water_turn_pending=s_reader_footer_pending=false;
 draws=pushes=errors=prepared=ended=0;failed=false;draw_lock=false;
 for(unsigned i=0;i<BYTES;i++)front[i]=(uint8_t)(i*13+7);
 memcpy(back,front,BYTES);memcpy(original,front,BYTES);ctx=(app_ctx_t){front,&hl,false};hl.back_fb=back;
 reader_panel_record(EPD_DRAW_SUCCESS);assert(s_reader_panel_frame.valid);
}
int main(void){
 (void)invalidate_prep;
 for(int large=0;large<2;large++){
  font_top=large?468:580;settings_top=large?376:580;
  const reader_panel_t targets[]={READER_PANEL_TOOLS,READER_PANEL_FONT_SETTINGS,READER_PANEL_REFRESH_SETTINGS,READER_PANEL_STATS};
  for(unsigned i=0;i<4;i++){
   reset(i?READER_PANEL_TOOLS:READER_PANEL_NONE);s_reader_panel=targets[i];
   assert(reader_panel_fast_present(&ctx,APP_REDRAW_PAGE)&&draws==1&&pushes==1&&prepared==1&&ended==1&&!errors);
   assert(area.x==0&&area.width==684&&area.height==1216-area.y);
   assert(area.y==(i==0?1096:i==1?font_top:i==2?settings_top:768));
   assert(!memcmp(front,original,(size_t)area.y*684/2)&&!memcmp(front,back,BYTES));
   assert(s_reader_panel_frame.valid&&s_reader_panel_frame.panel==targets[i]);
   assert(!reader_panel_fast_present(&ctx,APP_REDRAW_PAGE)&&pushes==1); // same-panel edits fall back
  }
 }
 for(int risk=0;risk<27;risk++){
  reset(READER_PANEL_NONE);s_reader_panel=READER_PANEL_TOOLS;app_redraw_t redraw=APP_REDRAW_PAGE;
  switch(risk){
   case 0:ctx.request_app=true;break;case 1:generation++;break;case 2:s_page++;break;case 3:s_chapter++;break;
   case 4:rotation=1;break;case 5:s_reader_fullscreen=false;break;case 6:s_text="different";break;
   case 7:s_clear_confirm=true;break;case 8:s_reader_notice[0]='x';break;case 9:images=1;break;
   case 10:background=true;break;case 11:s_reader_cleanup=true;break;case 12:s_reader_turn_pending=true;break;
   case 13:s_water_turn_pending=true;break;case 14:s_reader_footer_pending=true;break;case 15:s_reader_panel_frame.valid=false;break;
   case 16:front[0]^=1;break;case 17:redraw=APP_REDRAW_FULL;break;case 18:s_reader_panel=READER_PANEL_BACKGROUND;break;
   case 19:s_reader_panel=READER_PANEL_NONE;break;case 20:s_reader_panel_frame.panel=READER_PANEL_FONT_SETTINGS;break;
   case 21:ctx.hl=NULL;break;case 22:ctx.fb=NULL;break;case 23:hl.back_fb=NULL;break;case 24:s_view=SHELF;break;
   case 25:redraw=APP_REDRAW_AREA;break;case 26:s_text=NULL;break;
  }
  assert(!reader_panel_fast_present(&ctx,redraw)&&!draws&&!pushes&&!prepared&&!ended);
 }
 reset(READER_PANEL_NONE);s_reader_panel=READER_PANEL_TOOLS;failed=true;
 assert(reader_panel_fast_present(&ctx,APP_REDRAW_PAGE)&&pushes==1&&errors==1&&!s_reader_panel_frame.valid);
 // Full-rendered font adjustments may use short grays; images/background/turns/other books stay isolated.
 reset(READER_PANEL_TOOLS);s_reader_panel=READER_PANEL_FONT_SETTINGS;
 assert(reader_panel_short_ready(APP_REDRAW_PAGE));generation++;s_page++;
 assert(reader_panel_short_ready(APP_REDRAW_PAGE)); // final reflowed text+UI is deliberately admitted
 assert(!reader_panel_short_ready(APP_REDRAW_FULL)&&!reader_panel_short_ready(APP_REDRAW_AREA));
 images=1;assert(!reader_panel_short_ready(APP_REDRAW_PAGE));images=0;
 background=true;assert(!reader_panel_short_ready(APP_REDRAW_PAGE));background=false;
 s_fonts_reload_pending=true;assert(!reader_panel_short_ready(APP_REDRAW_PAGE));s_fonts_reload_pending=false;
 s_reader_turn_pending=true;assert(!reader_panel_short_ready(APP_REDRAW_PAGE));s_reader_turn_pending=false;
 s_reader_panel=READER_PANEL_BACKGROUND_PREVIEW;assert(!reader_panel_short_ready(APP_REDRAW_PAGE));
 reset(READER_PANEL_NONE);s_reader_panel=READER_PANEL_NONE;assert(!reader_panel_short_ready(APP_REDRAW_PAGE));
 // Reuse completed body slots; overlay frames and notices never become cached body art.
 reset(READER_PANEL_NONE);cache_ready=false;stores=full_draws=outer_draws=0;
 draw_reader_cached(front,s_page);assert(full_draws==1&&stores==1&&cache_ready);
 s_reader_panel=READER_PANEL_FONT_SETTINGS;memset(front,0,BYTES);draw_reader_cached(front,s_page);
 assert(full_draws==1&&outer_draws==1&&stores==1&&front[0]==255&&front[100*684/2]==0x76&&front[BYTES-1]==0x43);
 pause_reader_prep();assert(cancels==1&&!invalidates&&cache_ready);
 generation++;draw_reader_cached(front,s_page);assert(full_draws==2&&stores==1); // new font/layout generation
 s_reader_panel=READER_PANEL_NONE;cache_oom=true;cache_ready=false;draw_reader_cached(front,s_page);assert(full_draws==3&&!cache_ready);
 cache_oom=false;strcpy(s_reader_notice,"notice");draw_reader_cached(front,s_page);assert(full_draws==4&&!cache_ready);
 s_reader_notice[0]=0;images=1;draw_reader_cached(front,s_page);assert(full_draws==5&&!cache_ready);
 images=0;background=true;draw_reader_cached(front,s_page);assert(full_draws==6&&!cache_ready);
 puts("PASS: actual S31 overlay one regional gray push,untouched body,100/200% panel bounds,27 stale/state/error fallbacks;existing body slots reused,no overlay/notice cache,OOM and generation guards");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test';p.with_suffix('.c').write_text(unit)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-fsanitize=address,undefined',str(p.with_suffix('.c')),'-o',str(p)],check=True)
 subprocess.run([str(p)],check=True)
