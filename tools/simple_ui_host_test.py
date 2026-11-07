#!/usr/bin/env python3
"""Exercise the actual bounded picker and navigation mapping without firmware I/O."""
from pathlib import Path
import re, subprocess, tempfile
from home_cover_cache_host_test import function
ROOT=Path(__file__).resolve().parents[1]
source=(ROOT/'main/apps/app_book_picker.c').read_text()
source=re.sub(r'^#include ".*"\n','',source,flags=re.M)
preamble=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <errno.h>
typedef struct {int x,y,width,height;} EpdRect;
typedef int app_redraw_t;
typedef struct app_desc_s app_desc_t;
typedef struct {const app_desc_t *request_app;} app_ctx_t;
typedef struct {int type;uint16_t x0,y0;} ui_gesture_event_t;
struct app_desc_s {
 const char *title,*detail;bool owns_keys,enter_full,persistent_navigation;
 bool(*menu_handle_enabled)(app_ctx_t*);
 bool(*on_main_key)(app_ctx_t*,int,app_redraw_t*);
 void(*on_enter)(app_ctx_t*),(*on_exit)(app_ctx_t*),(*on_media_lost)(app_ctx_t*),(*on_media_ready)(app_ctx_t*),(*on_before_lock)(app_ctx_t*);
 void(*render)(app_ctx_t*,uint8_t*);app_redraw_t(*on_tick)(app_ctx_t*);
 app_redraw_t(*on_gesture)(app_ctx_t*,const ui_gesture_event_t*),(*on_key)(app_ctx_t*,int);
};
enum {APP_REDRAW_NONE,APP_REDRAW_PAGE,APP_REDRAW_FULL,UI_GESTURE_TAP,UI_GESTURE_SWIPE_L,UI_GESTURE_SWIPE_R,ESP_ERR_NOT_FINISHED};
#define UI_KEY_1 0
#define UI_KEY_2 1
#define UI_KEY_3 2
enum {UI_ICON_FOLDER_OPEN,UI_ICON_BOOK_OPEN,EPD_DRAW_ALIGN_LEFT,EPD_DRAW_ALIGN_CENTER};
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
#define UI_LOCK_WIDTH 684
#define UI_LOCK_HEIGHT 1216
#define UI_NAV_TOUCH_TOP 1080
static bool simple=true,media=true,alloc_fail,read_fail,open_fail,stat_fail,books_missing;
static int nav=-1,probes,visited,closed,opened,usb_requests;
static const app_desc_t app_transfer;
static void app_transfer_request_usb_start(void){++usb_requests;}
static size_t allocated;
static const app_desc_t apps[6];
static const app_desc_t *app_at(int i){assert(i>=0&&i<6);return &apps[i];}
static bool app_settings_simple_ui(void){return simple;}
static const char *app_settings_books_dir(void){return "/sdcard/books";}
static bool read_pico_sd_io_ready(void){return media;}
typedef struct {int unused;} read_pico_sd_info_t;
static int read_pico_sd_get_info(read_pico_sd_info_t *p){(void)p;return media?0:ESP_ERR_NOT_FINISHED;}
static void read_pico_sd_start_probe(void){++probes;}
static void *heap_caps_calloc(size_t n,size_t z,int caps){assert(caps==3);allocated=n*z;return alloc_fail?NULL:calloc(n,z);}
#include "book_file_path.h"
static char selected[288];
static bool app_book_request_open_from_home(const char *p){snprintf(selected,sizeof(selected),"%s",p);++opened;return !open_fail;}
#define ui_nav_status(...) ((void)0)
#define ui_nav_back(...) ((void)0)
#define ui_clear_page(...) ((void)0)
#define ui_text(...) ((void)0)
#define ui_text_caption(...) ((void)0)
#define ui_text_box(...) ((void)0)
#define ui_draw_icon(...) ((void)0)
#define ui_draw_separator(...) ((void)0)
#define ui_draw_button(...) ((void)0)
#define ui_nav_draw(...) ((void)0)
static const char *entries[100];static unsigned total,cursor;
static DIR *fake_open(const char *p){(void)p;cursor=0;return (DIR*)(uintptr_t)1;}
static struct dirent *fake_read(DIR *d){(void)d;static struct dirent e;
 if(read_fail){errno=EIO;return NULL;}
 if(cursor==total)return NULL;
 ++visited;snprintf(e.d_name,sizeof(e.d_name),"%s",entries[cursor++]);return &e;
}
static int fake_close(DIR *d){(void)d;++closed;return 0;}
static int fake_stat(const char *p,struct stat *s){memset(s,0,sizeof(*s));
 const char *name=strrchr(p,'/');name=name?name+1:p;
 if(stat_fail||(books_missing&&(!strcmp(name,"Books")||!strcmp(name,"books"))))return -1;
 s->st_size=2048;
 s->st_mode=(!strcmp(name,"Books")||!strcmp(name,"sdcard")||!strcmp(name,"books")||!strcmp(name,"folder"))?S_IFDIR:S_IFREG;return 0;
}
#if DPREADER_LOVE_BOOK
static int deletes, forgotten, cleared, changes;
static int fake_unlink(const char *path) { assert(!strcmp(path, "/sdcard/Books/Việt.EPUB")); ++deletes; return 0; }
static int book_progress_forget(const char *path) { assert(strstr(path,"Việt.EPUB")); ++forgotten; return 0; }
static int book_title_clear(const char *path) { assert(strstr(path,"Việt.EPUB")); ++cleared; return 0; }
static void book_store_notify_changed(void) { ++changes; }
#define unlink fake_unlink
#endif
#define opendir fake_open
#define readdir fake_read
#define closedir fake_close
#define stat fake_stat
'''
# Macro stat also rewrites struct stat: retain the real type tag.
source=source.replace('struct stat ', 'struct real_stat ')
preamble += '\n#define real_stat stat\n'
# Use function-like macro for stat instead, so struct declarations remain intact.
preamble=preamble.replace('#define stat fake_stat','#define stat(...) fake_stat(__VA_ARGS__)')
nav_source=ROOT/'main/ui/ui_nav.c'
unit=preamble+'\n'+function('ui_nav_hit',nav_source)+'\n'+function('ui_nav_request',nav_source)+'\n'+source+r'''
static bool front_key_opens_action_menu(int key,bool held){(void)key;(void)held;return false;}
static void app_action_menu_request(void){}
static int app_index_of(const app_desc_t *app){return (int)(app-apps);}
'''+function('main_key_action',ROOT/'main/app/app_loop.c')+r'''
static void tap(app_ctx_t *ctx,unsigned x,unsigned y){ui_gesture_event_t e={UI_GESTURE_TAP,x,y};gesture(ctx,&e);}
int main(void){
 app_ctx_t ctx={0};
 for(int i=0;i<2;++i)for(int key=UI_KEY_1;key<=UI_KEY_3;key+=2){
  int tab=i?3:0;ctx.request_app=NULL;
  assert(main_key_action(&ctx,app_at(tab),key,false)==APP_REDRAW_NONE);
  assert(ctx.request_app==app_at(i?0:3));
 }
 ctx.request_app=NULL;

 for(unsigned x=0;x<684;++x){assert(ui_nav_hit(x,1080)==(x<342?0:3));assert(ui_nav_hit(x,1079)==-1);}
 assert(ui_nav_hit(684,1100)==-1);
 simple=false;for(unsigned x=0;x<684;++x)assert(ui_nav_hit(x,1100)==(int)(x*4/684));simple=true;
 ui_nav_request(&ctx,2);assert(ctx.request_app==app_at(5));ctx.request_app=NULL;
 entries[0]=".hidden.epub";entries[1]="ignored.png";entries[2]="folder";entries[3]="Việt.EPUB";
 for(unsigned i=4;i<20;++i)entries[i]="book.txt";total=20;
 enter(&ctx);assert(probes==1&&s_pending);tick(&ctx);assert(!strcmp(s_directory,"/sdcard/Books"));
 assert(s_count==8&&s_more&&visited==11&&closed==1&&allocated==8*sizeof(picker_item_t));
 EpdRect left=page_button(0),right=page_button(1),label=page_label();
 assert(left.x+left.width<label.x&&label.x+label.width<right.x);
 assert(left.y+left.height<=UI_NAV_TOUCH_TOP&&right.y+right.height<=UI_NAV_TOUCH_TOP);
 tap(&ctx,342,1040);assert(s_page==0&&!s_pending);
 tap(&ctx,10,1040);assert(s_page==0&&!s_pending);
 assert(s_items[0].directory&&!strcmp(s_items[1].name,"Việt.EPUB"));
 tap(&ctx,160,PICK_TOP+PICK_ROW_H+10);assert(opened==1&&!strcmp(selected,"/sdcard/Books/Việt.EPUB")&&ctx.request_app==app_at(1));
 ctx.request_app=NULL;
 open_fail=true;tap(&ctx,160,PICK_TOP+PICK_ROW_H+10);assert(!ctx.request_app&&strstr(s_notice,"Không thể mở"));open_fail=false;
 stat_fail=true;tap(&ctx,160,PICK_TOP+PICK_ROW_H+10);assert(!ctx.request_app&&strstr(s_notice,"Tải lại"));stat_fail=false;
 tap(&ctx,550,260);assert(s_pending&&s_page==0);tick(&ctx);
 tap(&ctx,160,PICK_TOP+10);assert(s_pending&&!strcmp(s_directory,"/sdcard/Books/folder"));tick(&ctx);
 tap(&ctx,300,260);assert(s_pending&&!strcmp(s_directory,"/sdcard/Books"));tick(&ctx);
 ui_gesture_event_t swipe={UI_GESTURE_SWIPE_L,200,PICK_TOP+10};
 assert(gesture(&ctx,&swipe)==APP_REDRAW_PAGE&&s_page==1);tick(&ctx);
 swipe.type=UI_GESTURE_SWIPE_R;assert(gesture(&ctx,&swipe)==APP_REDRAW_PAGE&&s_page==0);tick(&ctx);
 tap(&ctx,550,1040);assert(s_page==1&&s_pending);tick(&ctx);assert(s_count==8&&s_more);
 tap(&ctx,550,1040);tick(&ctx);assert(s_count==2&&!s_more);tap(&ctx,100,1040);assert(s_page==1);tick(&ctx);
 back(&ctx);assert(!strcmp(s_directory,"/sdcard")&&s_pending);tick(&ctx);
 back(&ctx);assert(ctx.request_app==app_at(0));ctx.request_app=NULL;
 leave(&ctx);assert(!s_items&&!s_pending);
 alloc_fail=true;scan_page();assert(!s_items&&!s_count&&s_notice[0]);alloc_fail=false;
 total=0;scan_page();assert(!s_count&&!s_more&&s_notice[0]);
 read_fail=true;scan_page();assert(!s_count&&s_notice[0]);read_fail=false;
 lost(&ctx);assert(!s_items&&!s_pending&&!s_count&&!s_more);
 media=false;ready(&ctx);assert(tick(&ctx)==APP_REDRAW_NONE&&s_pending);
 media=true;tick(&ctx);assert(!s_pending&&!strcmp(s_directory,"/sdcard/Books"));leave(&ctx);
 books_missing=true;enter(&ctx);tick(&ctx);assert(!strcmp(s_directory,"/sdcard"));
 entries[0]="dprwifi";entries[1]="dpReader-settings.backup";entries[2]="dpreader_log.txt";
 entries[3]=".dpreader-progress";entries[4]="folder";entries[5]="Việt.EPUB";total=6;
 scan_page();assert(s_count==2&&s_items[0].directory&&!strcmp(s_items[0].name,"folder"));
 assert(!strcmp(s_items[1].name,"Việt.EPUB")&&!card_path("/sdcard//DPRWIFI"));
 entries[0]=".hidden.epub";entries[1]="ignored.png";entries[2]="folder";entries[3]="Việt.EPUB";
 for(unsigned i=4;i<20;++i)entries[i]="book.txt";total=20;
 tap(&ctx,300,260);assert(!s_pending&&!ctx.request_app);
 books_missing=false;tap(&ctx,100,260);tick(&ctx);assert(!strcmp(s_directory,"/sdcard/Books"));
 total=20;reload(false);tick(&ctx);
 tap(&ctx,160,PICK_TOP+10);tick(&ctx);assert(!strcmp(s_directory,"/sdcard/Books/folder"));
 tap(&ctx,550,110);assert(usb_requests==1&&ctx.request_app==&app_transfer&&s_return_from_usb);
 leave(&ctx);ctx.request_app=NULL;enter(&ctx);tick(&ctx);
 assert(!strcmp(s_directory,"/sdcard/Books/folder")&&!s_return_from_usb);
 leave(&ctx);enter(&ctx);tick(&ctx);assert(!strcmp(s_directory,"/sdcard/Books"));
 assert(!card_path("/sdcard/../other")&&!card_path("/sdcard/books/./bad")&&!card_path("/sdcard2"));
 for(unsigned i=0;i<3;++i){EpdRect r=tool_button(i);assert(r.y+r.height<=PICK_TOP&&r.x+r.width<=648);}
 assert(PICK_TOP+PICK_ROWS*PICK_ROW_H<=984);
#if DPREADER_LOVE_BOOK
 unsigned book_y=PICK_TOP+PICK_ROW_H+20;
 tap(&ctx,590,book_y);assert(s_delete_path[0]&&!deletes);
 tap(&ctx,100,730);assert(!s_delete_path[0]&&!deletes);
 tap(&ctx,590,book_y);app_book_picker.on_before_lock(&ctx);
 tap(&ctx,400,730);assert(!deletes);
 tap(&ctx,590,book_y);s_delete_stat.st_size++;
 tap(&ctx,400,730);assert(!deletes&&!s_delete_path[0]);
 tap(&ctx,590,book_y);tap(&ctx,400,730);
 assert(deletes==1&&forgotten==1&&cleared==1&&changes==1&&!s_delete_path[0]&&s_page==0);
#endif
 leave(&ctx);
 puts("PASS Love Book picker: Books default/fallback, folder navigation, refresh, buttons/swipes, Unicode/size, open errors, USB route/return/rescan, SD loss, OOM/read faults; both nav modes");
}
'''
with tempfile.TemporaryDirectory() as tmp:
 path=Path(tmp)/'test.c';path.write_text(unit)
 for edition in (0,1):
  subprocess.run(['cc','-std=gnu11',f'-DDPREADER_LOVE_BOOK={edition}','-fsanitize=undefined','-g',
                  '-I'+str(ROOT/'main/book'),str(path),'-o',str(Path(tmp)/'test')],check=True)
  subprocess.run([str(Path(tmp)/'test')],check=True)

# Full settings implementation with the S31 device-bound backup fixture.
from board_source_view import materialize
view=ROOT/'build/simple-ui-host'
materialize(ROOT,view,'metalio47',tests=True)
(view/'build/book-tests/settings-card').mkdir(parents=True,exist_ok=True)
subprocess.run(['cc','-std=gnu11','-Wall','-Wextra','-Werror','-fsanitize=undefined',
                '-Itools/settings_backup_stubs','-Icomponents/read_pico_transfer/include',
                'tools/simple_ui_backup_host_test.c','-o','build/book-tests/simple-ui-backup'],cwd=view,check=True)
subprocess.run(['build/book-tests/simple-ui-backup'],cwd=view,check=True)
