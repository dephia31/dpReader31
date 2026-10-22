#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
"""中文：运行真实目录按钮分支，检查目录验证、通知、错误与分页隔离。
English: Exercise actual root-button routing, validation, notifications, errors and pager separation.
"""
from pathlib import Path
import subprocess
import tempfile
from home_cover_cache_host_test import function

root = Path(__file__).resolve().parents[1]
path = root / 'main/apps/app_files.c'
source = path.read_text()
start = source.index('        if (!long_press && s_folder == 3 && (ui_rect_hit(file_action_rect(0)')
end = source.index('        int row = ', start)
assert start > source.index('    bool long_press = ev->type == UI_GESTURE_LONG_PRESS;', source.index('static app_redraw_t on_gesture'))
unit = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#define APP_REDRAW_PAGE 1
#define FILE_ROWS 9
enum {FILE_VIEW_LIST,FILE_VIEW_MOVE};
static int s_folder=3,s_view=FILE_VIEW_LIST,s_counts[3],book_saves,font_saves,changed,scans;
static char s_dir[288]="/sdcard/books",s_message[96],book_path[288],font_path[288];
static bool is_dir=true,exists=true,save_ok=true;
typedef struct{int x0,y0;} ui_gesture_event_t;
typedef struct{int x,y,width,height;} EpdRect;
static bool ui_rect_hit(EpdRect r,int x,int y){return x>=r.x&&x<r.x+r.width&&y>=r.y&&y<r.y+r.height;}
static int mock_stat(const char *p,struct stat *st){assert(!strcmp(p,s_dir));st->st_mode=is_dir?S_IFDIR:S_IFREG;return exists?0:-1;}
static bool app_settings_set_books_dir(const char *p){++book_saves;if(save_ok)snprintf(book_path,sizeof(book_path),"%s",p);return save_ok;}
static bool app_settings_set_fonts_dir(const char *p){++font_saves;if(save_ok)snprintf(font_path,sizeof(font_path),"%s",p);return save_ok;}
static void book_store_notify_changed(void){++changed;}
static void ttf_font_scan(void){++scans;}
static int count_root(int n){return n==0?13:7;}
'''
unit += function('file_pager_top', path)+'\n'+function('file_action_rect', path)+'\n'
unit += function('visible_rows', path) + '\n'
unit += 'static int tap(int x,int y,bool long_press){ui_gesture_event_t event={x,y};const ui_gesture_event_t *ev=&event;\n'
unit += source[start:end].replace('stat(s_dir,&selected)', 'mock_stat(s_dir,&selected)')
unit += 'return 0;}\n'
unit += r'''
int main(void){
 assert(visible_rows()==7);
 assert(tap(100,900,false)==1&&book_saves==1&&changed==1&&!scans);
 assert(!strcmp(book_path,s_dir)&&strstr(s_message,"已设为书籍目录"));
 assert(tap(500,900,false)==1&&font_saves==1&&scans==1&&!strcmp(font_path,s_dir));
 assert(s_counts[0]==13&&s_counts[2]==7);
 assert(!tap(100,874,false)&&!tap(100,951,false)&&!tap(100,1030,false)&&!tap(100,900,true));
 s_folder=0;assert(visible_rows()==8&&!tap(100,900,false));
 s_view=FILE_VIEW_MOVE;assert(visible_rows()==7);s_view=FILE_VIEW_LIST;s_folder=3;
 int saves=book_saves+font_saves;
 snprintf(s_dir,sizeof(s_dir),"/sdcard");assert(tap(100,900,false)==1&&saves==book_saves+font_saves);
 snprintf(s_dir,sizeof(s_dir),"/sdcard/library");exists=false;assert(tap(500,900,false)==1&&saves==book_saves+font_saves);
 exists=true;is_dir=false;assert(tap(100,900,false)==1&&saves==book_saves+font_saves);
 is_dir=true;save_ok=false;int notified=changed+scans;
 assert(tap(100,900,false)==1&&strstr(s_message,"保存失败")&&changed+scans==notified);
 assert(tap(500,900,false)==1&&strstr(s_message,"保存失败")&&changed+scans==notified);
 puts("PASS: actual custom-root buttons validate folders, notify books/fonts only after saving, preserve seven rows and do not overlap pagination");
}
'''
with tempfile.TemporaryDirectory() as directory:
    src = Path(directory) / 'test.c'
    src.write_text(unit)
    exe = Path(directory) / 'test'
    subprocess.run(['cc', '-std=gnu11', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', str(src), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
