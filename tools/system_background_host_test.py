#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
"""中文：真实页面合成出口保留二级灰底和底栏。/ English: Actual composition wrappers retain child grays and navigation."""
from pathlib import Path
import subprocess
import tempfile
from home_cover_cache_host_test import function

root = Path(__file__).resolve().parents[1]
settings = root / 'main/apps/app_device_settings.c'
files = root / 'main/apps/app_files.c'
text = settings.read_text()
assert '#if KIIKO_BOARD_METALIO47' not in text, 'Run in the Pico-selected host view'
enum = text[text.index('typedef enum { SETTINGS_MAIN,'):text.index('static settings_page_t s_page;')]
unit = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#define UI_NAV_REFRESH_END 1088
#define UI_NAV_TOP 1096
typedef struct {int x,y,width,height;} EpdRect;
typedef struct {int unused;} app_ctx_t;
static uint8_t frame[684*1216];
static int s_main_scroll,s_folder,normalizations;
static bool s_avatar_ready,s_render_secondary;
typedef enum {FILE_VIEW_LIST,FILE_VIEW_ACTIONS,FILE_VIEW_RENAME,FILE_VIEW_MOVE} file_view_t;
static file_view_t s_view;
''' + enum + r'''
static settings_page_t s_page;
static int settings_profile_height(void){return 180;}
static bool main_page_visible(app_ctx_t *ctx){(void)ctx;return s_view==FILE_VIEW_LIST&&s_folder<0;}
static void render_content(app_ctx_t *ctx,uint8_t *fb){
 (void)ctx;
 for(int y=0;y<1216;++y)for(int x=0;x<684;++x)
  fb[y*684+x]=y>=UI_NAV_TOP?14:(uint8_t)((x+y)%16);
}
static void settings_navigation(uint8_t *fb){
 for(int y=UI_NAV_TOP;y<1216;++y)for(int x=0;x<684;++x)fb[y*684+x]=14;
}
static void display_system_content(uint8_t *fb,bool navigation,EpdRect gray){
 assert(navigation);++normalizations;
 for(int y=0;y<UI_NAV_REFRESH_END;++y)for(int x=0;x<684;++x)
  if(!(x>=gray.x&&x<gray.x+gray.width&&y>=gray.y&&y<gray.y+gray.height))
   fb[y*684+x]=fb[y*684+x]>=8?15:0;
}
''' + function('render', settings).replace('static void render(', 'static void settings_render(') + '\n' + function('render', files).replace('static void render(', 'static void files_render(') + r'''
static void preserved(void){
 for(int y=0;y<UI_NAV_REFRESH_END;++y)for(int x=0;x<684;++x)
  assert(frame[y*684+x]==(x+y)%16);
 for(int y=UI_NAV_TOP;y<1216;++y)for(int x=0;x<684;++x)assert(frame[y*684+x]==14);
}
int main(void){
 app_ctx_t ctx={0};
 for(int page=SETTINGS_MAIN+1;page<=SETTINGS_TEXT_EDIT;++page){
  s_page=page;settings_render(&ctx,frame);preserved();assert(!normalizations);
 }
 for(int view=FILE_VIEW_LIST;view<=FILE_VIEW_MOVE;++view)for(int folder=-1;folder<=3;++folder){
  s_view=view;s_folder=folder;if(main_page_visible(&ctx))continue;
  normalizations=0;files_render(&ctx,frame);preserved();assert(!normalizations&&!s_render_secondary);
 }
 s_view=FILE_VIEW_LIST;s_folder=-1;files_render(&ctx,frame);assert(normalizations==1);
 s_page=SETTINGS_MAIN;settings_render(&ctx,frame);assert(normalizations==2);
 puts("PASS: actual Settings/Files wrappers preserve all16 child grays and fixed gray navigation; root lists retain BW conversion");
}
'''
with tempfile.TemporaryDirectory(prefix='system-background-') as directory:
    path = Path(directory)
    (path / 'test.c').write_text(unit)
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined',
                    str(path / 'test.c'), '-o', str(path / 'test')], check=True)
    subprocess.run([str(path / 'test')], check=True)
