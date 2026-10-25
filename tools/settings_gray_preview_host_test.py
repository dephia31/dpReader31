#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
"""中文：真实设置预览保留16灰/恢复黑白作用域，拼贴选择不阻塞UI。
English: Actual settings previews retain16 grays/restore policy; collage selection never builds synchronously.
"""
from pathlib import Path
import subprocess,tempfile
from board_source_view import select_source
from home_cover_cache_host_test import function
r=Path(__file__).resolve().parents[1];p=r/'main/apps/app_device_settings.c'
def actual(name):return select_source(function(name,p),'metalio47')+'\n'
tick=actual('on_tick');assert 'book_lock_collage_draw' not in tick
source=select_source(p.read_text(),'metalio47')
start=source.index('if (s_page == SETTINGS_LOCK_STYLE)',source.index('static app_redraw_t on_gesture'))
brace=source.index('{',start);depth=1;end=brace+1
while depth:
    depth+=(source[end]=='{')-(source[end]=='}');end+=1
choose=source[start:end];assert 's_collage_prepare_pending' not in choose
unit=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
typedef struct{int x,y,width,height;}EpdRect;
typedef enum{APP_REDRAW_NONE,APP_REDRAW_PAGE,APP_REDRAW_FULL,APP_REDRAW_AREA,APP_REDRAW_DONE}app_redraw_t;
typedef struct{uint8_t *fb;void *hl;const void *request_app;}app_ctx_t;
typedef struct{int unused;}app_desc_t;
const app_desc_t app_image={0};
enum{SETTINGS_MAIN,SETTINGS_WALLPAPER_PREVIEW,SETTINGS_SHELF_STYLE,SETTINGS_ONLINE_FONTS,SETTINGS_PIN_ENTRY,
 SETTINGS_TEXT_EDIT,SETTINGS_BLUETOOTH,SETTINGS_BLE_SCAN,SETTINGS_SYSTEM_FONT,SETTINGS_WALLPAPER,SETTINGS_AVATAR,
 SETTINGS_UPGRADE,SETTINGS_LOCK_STYLE,MODE_GL16,MODE_DU};
#define UI_LOCK_WIDTH 684
#define UI_LOCK_HEIGHT 1216
#define UI_NAV_REFRESH_END 1088
static int s_page,s_online_fonts;
static char s_notice[96];static int picker_requests;static bool picker_transparent,picker_fail;
static bool app_image_request_lock_picker(const char*p,bool transparent){assert(!p);++picker_requests;picker_transparent=transparent;return !picker_fail;}static bool s_input_settle,s_input_layout,s_scroll_present_pending;
static EpdRect s_input_area;static int E0470_WAVEFORM,E0470_FOLLOW_WAVEFORM,E0470_FULL_WAVEFORM;
static int mono,pushes,paints,cancels,error,guards,style,scans,fulls;static bool binary;
static bool display_system_binary_set(bool b){bool old=binary;binary=b;return old;}
static void render_content(app_ctx_t *c,uint8_t *fb){(void)c;++paints;for(int i=0;i<256;++i)fb[i]=(i%16)*17;}
static void settings_navigation(uint8_t *fb){(void)fb;}
static void display_monochrome_content(uint8_t *fb){++mono;for(int i=0;i<256;++i)fb[i]=fb[i]>127?255:0;}
static bool ui_online_fonts_present(int *f,app_ctx_t *c,app_redraw_t d){(void)f;(void)c;(void)d;return false;}
static void display_main_transition_cancel(void){++cancels;}
static void guard_draw_result(void *h,int e){(void)h;assert(!binary&&e==error);++guards;}
static int update_display_area_full_with(void *h,const int *w,int m,EpdRect a){(void)h;assert(!binary&&w==&E0470_FULL_WAVEFORM&&m==MODE_GL16&&a.width==684&&a.height==1216);++pushes;return error;}
static int update_display_full(void *h){(void)h;assert(!binary);++pushes;++fulls;return error;}
static int update_display_area_diff_with(void *h,const int *w,int m,EpdRect a){(void)h;(void)w;(void)m;(void)a;assert(false);return 0;}
static const int *display_main_bw_waveform(void){return &E0470_WAVEFORM;}
static int settings_content_end(void){return 1216;}
static EpdRect upgrade_progress_area(void){return(EpdRect){0};}
static void wallpaper_scan(void){++scans;}
static void app_settings_set_lock_style(unsigned s){style=s;}
static const char *app_settings_wallpaper_path(void){return "";}
'''
unit+='static bool s_lock_style_return;\n'+actual('app_device_settings_resume_lock_style')+actual('settings_take_lock_style_return')+actual('settings_open_lock_images')+actual('settings_gray_preview')+actual('render')+actual('settings_present')
unit+='static app_redraw_t choose(app_ctx_t *ctx,int y){'+choose+'return APP_REDRAW_NONE;}\n'
unit+=r'''
int main(void){
 uint8_t fb[256];app_ctx_t ctx={.fb=fb};
 assert(!settings_take_lock_style_return());app_device_settings_resume_lock_style();
 assert(settings_take_lock_style_return()&&!settings_take_lock_style_return());
 for(int page=SETTINGS_WALLPAPER_PREVIEW;page<=SETTINGS_SHELF_STYLE;++page)
 for(int policy=0;policy<2;++policy)for(int redraw=APP_REDRAW_PAGE;redraw<=APP_REDRAW_AREA;++redraw)for(int fail=0;fail<2;++fail){
  s_page=page;binary=policy;error=fail;s_scroll_present_pending=true;
  int before=pushes,drawn=paints,was_mono=mono;
  assert(settings_present(&ctx,redraw)&&pushes==before+1&&paints==drawn+1&&mono==was_mono&&binary==!!policy&&!s_scroll_present_pending);
  unsigned levels=0;for(int i=0;i<256;++i)levels|=1u<<(fb[i]/17);assert(levels==65535);
 }
 assert(fulls==8);
 s_page=SETTINGS_WALLPAPER_PREVIEW;ctx.request_app=(void*)1;int before=pushes;
 assert(settings_present(&ctx,APP_REDRAW_PAGE)&&pushes==before);ctx.request_app=NULL;
 assert(!settings_present(&ctx,APP_REDRAW_NONE)&&pushes==before);
 s_page=SETTINGS_MAIN;render(&ctx,fb);assert(mono==1);for(int i=0;i<256;++i)assert(fb[i]==0||fb[i]==255);
 assert(!settings_present(&ctx,APP_REDRAW_PAGE));
 s_page=SETTINGS_LOCK_STYLE;assert(choose(&ctx,650)==APP_REDRAW_PAGE&&style==2&&!scans&&s_page==SETTINGS_LOCK_STYLE);
 for(int mode=0;mode<2;++mode){ctx.request_app=NULL;assert(choose(&ctx,mode?800:320)==APP_REDRAW_NONE&&ctx.request_app==&app_image&&picker_transparent==!!mode);}
 assert(picker_requests==2&&!scans);picker_fail=true;ctx.request_app=NULL;
 assert(choose(&ctx,320)==APP_REDRAW_PAGE&&!ctx.request_app&&s_notice[0]);
 puts("PASS: actual wallpaper/transparent/shelf previews retain16 tones with one full-gray output; restore BW even on failure; regular settings stay binary; cancelled navigation skips output; collage selection/ticks do not build or scan synchronously");
}
'''
with tempfile.TemporaryDirectory(prefix='s31-settings-preview-') as d:
    c=Path(d)/'test.c';c.write_text(unit);exe=Path(d)/'test'
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-g','-fsanitize=address,undefined',str(c),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
