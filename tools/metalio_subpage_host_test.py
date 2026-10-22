#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
"""Exercise actual S31 child-page admission, back cleanup and navigation cleanup with ASan/UBSan."""
from pathlib import Path
import re
import subprocess
import tempfile
import json

ROOT = Path(__file__).resolve().parents[1]
view=ROOT/'.kiiko-source-view.json'
metalio=not view.exists() or json.loads(view.read_text())['board']=='metalio47'
board_option=['-DKIIKO_BOARD_METALIO47='+str(int(metalio))]


def function(path, name):
    text = (ROOT / path).read_text()
    match = re.search(r'^(?:static )?[A-Za-z_]\w*\s+\**' + name + r'\([^;{}]*\)\s*\{', text, re.M)
    assert match, name
    at, depth, quote, escape = match.end(), 1, None, False
    while depth:
        c = text[at]
        if quote:
            if escape: escape = False
            elif c == '\\': escape = True
            elif c == quote: quote = None
        elif c in "\"'": quote = c
        elif c == '{': depth += 1
        elif c == '}': depth -= 1
        at += 1
    return text[match.start():at]


def run(name, source, options=()):
    with tempfile.TemporaryDirectory(prefix='s31-subpage-') as directory:
        path = Path(directory) / 'test.c'
        path.write_text(source)
        binary = path.with_suffix('')
        subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-Wno-unused-function',
                        '-fsanitize=address,undefined', '-g', *options, str(path), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
    print('PASS: ' + name, flush=True)


settings_path = 'main/apps/app_device_settings.c'
text = (ROOT / settings_path).read_text()
enum = text[text.index('typedef enum { SETTINGS_MAIN,'):text.index('static settings_page_t s_page;')]
settings = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define UI_LOCK_HEIGHT 1216
#define UI_NAV_TOP 1096
#define UI_NAV_REFRESH_END 1088
#define UI_SYSTEM_KEYBOARD_TOP 560
typedef struct {uint16_t y0;} ui_gesture_event_t;
typedef struct {void *request_app;} app_ctx_t;
typedef enum {APP_REDRAW_NONE,APP_REDRAW_AREA,APP_REDRAW_PAGE,APP_REDRAW_FULL} app_redraw_t;
''' + enum + r'''
static settings_page_t s_page;
static bool s_reset_confirm,s_reset_pending,s_local_pending,s_upgrade_restart,s_pin_pending,s_font_sync_confirm,s_boot_pending;
static bool s_wallpaper_confirm,s_upgrade_confirm,s_editor_signature,s_main_wifi_saved;
static char s_notice[96],s_main_wifi[64];
static int s_upgrade_job,s_ble_scroll;
enum {UPGRADE_JOB_NONE};
static int nav_draws,keyboards,scans,pins,previews;
static void ui_nav_draw(uint8_t *fb,int tab){(void)fb;assert(tab==3);++nav_draws;}
static int ui_nav_hit(uint16_t x,uint16_t y){return x<684&&y>=1096&&y<1216?x/171:-1;}
static void ui_nav_request(app_ctx_t *ctx,int tab){assert(tab==0);ctx->request_app=(void*)1;}
static void app_cancel_download(void){}
static void ui_keyboard_end(void){++keyboards;}
static void pin_setup_close(void){++pins;}
static void pico_online_cancel_join(void){}
static void ble_pt_scan_stop(void){++scans;}
static void bookmark_preview_release(void){++previews;}
static int read_pico_transfer_get_saved_wifi(char *s,bool *saved){s[0]=0;*saved=false;return 0;}
static void settings_prepare_profile(void){}
''' + '\n'.join(function(settings_path, name) for name in [
    'settings_content_end', 'settings_viewport_end', 'settings_navigation', 'settings_nav_hit',
    'settings_edge_back', 'settings_back_swipe_area', 'on_key']) + r'''
int main(void) {
    uint8_t fb[1];app_ctx_t ctx={0};
    for(int page=SETTINGS_MAIN;page<=SETTINGS_FRONTLIGHT;++page) {
        s_page=page;nav_draws=0;settings_navigation(fb);
        assert(nav_draws==1);
        assert(settings_content_end()==1088);
        assert(settings_viewport_end()==1096);
        assert(settings_nav_hit(20,1140)==0);
        assert(settings_edge_back(&ctx)==(page!=SETTINGS_MAIN));
    }
    const int from[]={SETTINGS_TIME_EDIT,SETTINGS_WALLPAPER_PREVIEW,SETTINGS_WALLPAPER,
        SETTINGS_AVATAR,SETTINGS_TEXT_EDIT,SETTINGS_PROFILE,SETTINGS_BLE_SCAN,SETTINGS_PIN_ENTRY,SETTINGS_FRONTLIGHT};
    const int to[]={SETTINGS_TIME,SETTINGS_WALLPAPER,SETTINGS_LOCK_STYLE,
        SETTINGS_PROFILE,SETTINGS_PROFILE,SETTINGS_MAIN,SETTINGS_BLUETOOTH,SETTINGS_LOCK_PIN,SETTINGS_MAIN};
    for(unsigned i=0;i<sizeof(from)/sizeof(*from);++i) {
        s_page=from[i];assert(on_key(&ctx,0)==APP_REDRAW_PAGE);assert((int)s_page==to[i]);assert(!ctx.request_app);
    }
    assert(keyboards&&scans&&pins&&previews);
    const settings_page_t selectors[]={SETTINGS_MAIN_REFRESH,SETTINGS_SYSTEM_CONTRAST};
    for(unsigned i=0;i<sizeof(selectors)/sizeof(*selectors);++i){
        s_page=selectors[i];assert(settings_edge_back(&ctx));
        assert(settings_back_swipe_area(&ctx,&(ui_gesture_event_t){.y0=600}));
        assert(on_key(&ctx,0)==APP_REDRAW_PAGE&&s_page==SETTINGS_MAIN);
    }
    s_page=SETTINGS_TEXT_EDIT;
    assert(settings_back_swipe_area(&ctx,&(ui_gesture_event_t){.y0=559}));
    assert(!settings_back_swipe_area(&ctx,&(ui_gesture_event_t){.y0=560}));
    s_page=SETTINGS_UPGRADE;s_local_pending=true;
    assert(!settings_edge_back(&ctx));assert(on_key(&ctx,0)==APP_REDRAW_NONE&&s_page==SETTINGS_UPGRADE);
    s_local_pending=false;s_reset_pending=true;assert(!settings_edge_back(&ctx));
    s_reset_pending=false;s_font_sync_confirm=true;assert(!settings_edge_back(&ctx));
    return 0;
}
'''
if not metalio:
    settings=settings.replace('SETTINGS_FRONTLIGHT;++page','SETTINGS_TEXT_EDIT;++page').replace(',SETTINGS_PIN_ENTRY,SETTINGS_FRONTLIGHT}',',SETTINGS_PIN_ENTRY}').replace(',SETTINGS_LOCK_PIN,SETTINGS_MAIN}',',SETTINGS_LOCK_PIN}').replace(',s_boot_pending;', ';')
run('all settings child scopes, persistent hits, bounded content and actual nested cleanup',settings,board_option)

files_path='main/apps/app_files.c'
files = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
typedef struct {void *request_app;} app_ctx_t;
typedef struct {uint16_t y0;} ui_gesture_event_t;
#define UI_SYSTEM_KEYBOARD_TOP 560
typedef enum {APP_REDRAW_NONE,APP_REDRAW_AREA,APP_REDRAW_PAGE} app_redraw_t;
typedef enum {FILE_VIEW_LIST,FILE_VIEW_ACTIONS,FILE_VIEW_RENAME,FILE_VIEW_MOVE} file_view_t;
static int s_folder,s_page,s_move_origin_folder,s_move_origin_page;
static char s_dir[288],s_move_origin_dir[288];
static file_view_t s_view;
static bool s_delete_confirm,s_render_secondary,s_sd_format_confirm;
static int navs,keyboards,scans;
static void ui_nav_draw(uint8_t *fb,int tab){(void)fb;assert(tab==2);++navs;}
static int ui_nav_hit(uint16_t x,uint16_t y){return x<684&&y>=1096&&y<1216?x/171:-1;}
static void ui_nav_request(app_ctx_t *ctx,int index){assert(index==0);ctx->request_app=(void*)1;}
static void ui_keyboard_end(void){++keyboards;}
static void scan_folder(int folder){assert(folder>=0&&folder<=3);++scans;}
static void rescan_current(void){++scans;}
static void copy_utf8(char *out,size_t size,const char *value){snprintf(out,size,"%s",value);}
''' + '\n'.join(function(files_path,name) for name in ['main_page_visible','files_navigation','files_nav_hit','files_back_swipe_area','leave_move','on_key']) + r'''
int main(void) {
 app_ctx_t ctx={0};uint8_t fb[1];strcpy(s_dir,"/sdcard");
 for(int view=FILE_VIEW_LIST;view<=FILE_VIEW_MOVE;++view)for(int folder=-1;folder<=3;++folder) {
  s_view=view;s_folder=folder;navs=0;files_navigation(fb);
  bool root=view==FILE_VIEW_LIST&&folder<0;
  assert(navs==1&&main_page_visible(&ctx)==root);
  assert(files_nav_hit(20,1140)==0);
  assert(files_back_swipe_area(&ctx,&(ui_gesture_event_t){.y0=560})==(view!=FILE_VIEW_RENAME));
  assert(files_back_swipe_area(&ctx,&(ui_gesture_event_t){.y0=559}));
 }
 s_folder=-1;s_view=FILE_VIEW_LIST;s_render_secondary=true;navs=0;files_navigation(fb);assert(navs==1);
 s_render_secondary=false;s_sd_format_confirm=true;s_view=FILE_VIEW_RENAME;assert(on_key(&ctx,0)==APP_REDRAW_PAGE&&s_view==FILE_VIEW_ACTIONS&&keyboards==1&&!s_sd_format_confirm);
 s_delete_confirm=true;assert(on_key(&ctx,0)==APP_REDRAW_PAGE&&s_view==FILE_VIEW_LIST&&!s_delete_confirm);
 s_view=FILE_VIEW_MOVE;s_move_origin_folder=3;s_move_origin_page=2;strcpy(s_move_origin_dir,"/sdcard/a/b");
 assert(on_key(&ctx,0)==APP_REDRAW_PAGE&&s_folder==3&&s_page==2&&!strcmp(s_dir,"/sdcard/a/b"));
 assert(on_key(&ctx,0)==APP_REDRAW_PAGE&&!strcmp(s_dir,"/sdcard/a"));
 assert(on_key(&ctx,0)==APP_REDRAW_PAGE&&!strcmp(s_dir,"/sdcard"));
 assert(on_key(&ctx,0)==APP_REDRAW_PAGE&&s_folder==-1&&!ctx.request_app);
 return 0;
}
'''
run('file scopes, persistent bottom hits, nested modal underlay, rename/move/directory back cleanup',files,board_option)
