#!/usr/bin/env python3
"""Exercise actual confirmation handler; no live storage is touched."""
from pathlib import Path
import subprocess,tempfile
from home_cover_cache_host_test import function
root=Path(__file__).resolve().parents[1]
source=root/'main/apps/app_files.c'
unit=r'''
#include <assert.h>
#include <dirent.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "book_file_path.h"
typedef int app_redraw_t;typedef int esp_err_t;
typedef struct{int x,y,width,height;}EpdRect;
typedef struct{int type,x0,y0;}ui_gesture_event_t;
enum{APP_REDRAW_NONE,APP_REDRAW_PAGE,UI_GESTURE_TAP=10,ESP_OK=0};
static bool s_bulk_delete_confirm;static char s_bulk_delete_dir[288]="/sdcard/test";static char s_message[96];
static int s_folder=3;
typedef struct { char path[288]; char name[128]; off_t size; bool is_dir; } file_item_t;
static file_item_t s_files[12];
static int s_count,s_page,s_counts[3],s_view;
enum { FILE_MAX=12, FILE_VIEW_MOVE=1 };
typedef struct { bool mounted; } sd_info_t;
static sd_info_t s_sd;
static const char *const entries[] = {"DprWifi", ".dpreader-progress", "dpreader_log.txt",
    "dpReader-settings.backup", "firmware.bin", "picture.png", "Books", "Novel.txt", "Việt.EPUB"};
static unsigned entry_index;
static const char *folder_root(int folder){assert(folder==3);return "/sdcard";}
static bool files_alloc(void){return true;}
static void read_pico_sd_get_info(sd_info_t *info){info->mounted=true;}
static DIR *fake_open(const char *path){assert(!strcmp(path,"/sdcard"));entry_index=0;return (DIR *)1;}
static int fake_close(DIR *dir){assert(dir);return 0;}
static struct dirent *fake_read(DIR *dir){
    assert(dir);static struct dirent result;
    if(entry_index==sizeof(entries)/sizeof(entries[0]))return NULL;
    strcpy(result.d_name,entries[entry_index++]);return &result;
}
static int fake_stat(const char *path,struct stat *info){
    info->st_mode=!strcmp(path,"/sdcard/Books") ? S_IFDIR : S_IFREG;info->st_size=100;return 0;
}
#define opendir fake_open
#define readdir fake_read
#define closedir fake_close
#define stat(...) fake_stat(__VA_ARGS__)
static int deletions,rescans,notifies,fonts,result;
static bool ui_rect_hit(EpdRect r,int x,int y){return x>=r.x&&y>=r.y&&x<r.x+r.width&&y<r.y+r.height;}
static void file_deleted(const char*p,bool d,void*c){(void)p;(void)d;(void)c;}
static int file_tree_delete_files(const char*p,bool(*match)(const char*,void*),void(*cb)(const char*,bool,void*),void*c,unsigned*n,unsigned*f){
 assert(!strcmp(p,"/sdcard/test")&&cb==file_deleted&&c==&s_folder);
 assert(match("/sdcard/Novel.EPUB",c)&&match("/sdcard/Novel.txt",c));
 assert(!match("/sdcard/.hidden.txt",c)&&!match("/sdcard/Dprwifi/credential.txt",c));
 assert(!match("/sdcard/dpreader_log.txt",c)&&!match("/sdcard/dpReader-settings.backup",c));
 assert(!match("/sdcard/firmware.bin",c)&&!match("/sdcard/picture.png",c));
 ++deletions;*n=140;*f=result?1:0;return result;
}
static void rescan_current(void){++rescans;}static void book_store_notify_changed(void){++notifies;}static void ttf_font_scan(void){++fonts;}
'''+ '\n'.join(function(name,source) for name in ('supported','compare_files','scan_folder',
                                                   'bulk_delete_matches','bulk_delete_gesture'))+r'''
int main(void){
 scan_folder(3);assert(s_count==3);
 assert(s_files[0].is_dir&&!strcmp(s_files[0].name,"Books"));
 assert(!s_files[1].is_dir&&!strcmp(s_files[1].name,"Novel.txt"));
 assert(!s_files[2].is_dir&&!strcmp(s_files[2].name,"Việt.EPUB"));
 s_view=FILE_VIEW_MOVE;scan_folder(3);assert(s_count==1&&s_files[0].is_dir);
 for(int type=0;type<12;++type){s_bulk_delete_confirm=true;ui_gesture_event_t e={type,500,750};deletions=0;bulk_delete_gesture(&e);assert(deletions==(type==UI_GESTURE_TAP));}
 for(int y=0;y<1216;y+=7)for(int x=0;x<684;x+=7){
  s_bulk_delete_confirm=true;deletions=rescans=notifies=fonts=0;result=0;ui_gesture_event_t e={UI_GESTURE_TAP,x,y};
  bulk_delete_gesture(&e);bool accept=ui_rect_hit((EpdRect){356,710,280,80},x,y);
  assert(deletions==accept&&rescans==accept&&notifies==accept&&fonts==accept);
 }
 result=-1;s_bulk_delete_confirm=true;ui_gesture_event_t e={UI_GESTURE_TAP,500,750};bulk_delete_gesture(&e);
 assert(!s_bulk_delete_confirm&&strstr(s_message,"140")&&strstr(s_message,"lỗi"));
 puts("PASS Full file list and bulk confirmation: TXT/EPUB, hidden data excluded, explicit accept and partial failure feedback");
}
'''
with tempfile.TemporaryDirectory() as folder:
 p=Path(folder);(p/'test.c').write_text(unit)
 subprocess.run(['cc','-std=gnu11','-Wall','-Wextra','-Werror','-Wno-sign-compare','-fsanitize=undefined','-I'+str(root/'main/book'),str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
