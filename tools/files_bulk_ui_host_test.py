#!/usr/bin/env python3
"""Exercise actual confirmation handler; no live storage is touched."""
from pathlib import Path
import subprocess,tempfile
from home_cover_cache_host_test import function
root=Path(__file__).resolve().parents[1]
unit=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef int app_redraw_t;typedef int esp_err_t;
typedef struct{int x,y,width,height;}EpdRect;
typedef struct{int type,x0,y0;}ui_gesture_event_t;
enum{APP_REDRAW_NONE,APP_REDRAW_PAGE,UI_GESTURE_TAP=10,ESP_OK=0};
static bool s_bulk_delete_confirm;static char s_bulk_delete_dir[288]="/sdcard/test";static char s_message[96];
static int deletions,rescans,notifies,fonts,result;
static bool ui_rect_hit(EpdRect r,int x,int y){return x>=r.x&&y>=r.y&&x<r.x+r.width&&y<r.y+r.height;}
static void file_deleted(const char*p,bool d,void*c){(void)p;(void)d;(void)c;}
static int file_tree_delete_files(const char*p,void(*cb)(const char*,bool,void*),void*c,unsigned*n,unsigned*f){assert(!strcmp(p,"/sdcard/test")&&cb==file_deleted&&!c);++deletions;*n=140;*f=result?1:0;return result;}
static void rescan_current(void){++rescans;}static void book_store_notify_changed(void){++notifies;}static void ttf_font_scan(void){++fonts;}
'''+function('bulk_delete_gesture',root/'main/apps/app_files.c')+r'''
int main(void){
 for(int type=0;type<12;++type){s_bulk_delete_confirm=true;ui_gesture_event_t e={type,500,750};deletions=0;bulk_delete_gesture(&e);assert(deletions==(type==UI_GESTURE_TAP));}
 for(int y=0;y<1216;y+=7)for(int x=0;x<684;x+=7){
  s_bulk_delete_confirm=true;deletions=rescans=notifies=fonts=0;result=0;ui_gesture_event_t e={UI_GESTURE_TAP,x,y};
  bulk_delete_gesture(&e);bool accept=ui_rect_hit((EpdRect){356,710,280,80},x,y);
  assert(deletions==accept&&rescans==accept&&notifies==accept&&fonts==accept);
 }
 result=-1;s_bulk_delete_confirm=true;ui_gesture_event_t e={UI_GESTURE_TAP,500,750};bulk_delete_gesture(&e);
 assert(!s_bulk_delete_confirm&&strstr(s_message,"140")&&strstr(s_message,"lỗi"));
 puts("PASS actual bulk confirmation: only explicit accept deletes, all screen hit points, no hold/press activation, captured path, partial failure feedback");
}
'''
with tempfile.TemporaryDirectory() as folder:
 p=Path(folder);(p/'test.c').write_text(unit)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
