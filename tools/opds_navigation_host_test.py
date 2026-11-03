from pathlib import Path
import subprocess,tempfile
from home_cover_cache_host_test import function
root=Path(__file__).resolve().parents[1];src=root/'main/apps/app_book.c'
s=src.read_text();a=s.index('    if (s_view == OPDS) {',s.index('static app_redraw_t action_at('));b=s.index('    if (s_view == TOC) {',a)
unit=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#define OPDS_ROWS 7
#define ESP_OK 0
#define APP_REDRAW_NONE 0
#define APP_REDRAW_PAGE 1
#define UI_GESTURE_TAP 1
enum{OPDS=1,IMPORT,SHELF};
typedef int app_redraw_t;
typedef struct{int x,y,width,height;}EpdRect;
typedef struct{int unused;}app_ctx_t;
typedef struct{int type;uint16_t x0,y0,x,y;}ui_gesture_event_t;
typedef struct{char href[256],title[128];bool is_book;}opds_entry_t;
typedef struct{size_t count;opds_entry_t entries[24];}opds_feed_t;
static opds_feed_t feed,*s_opds_feed=&feed;
static int s_view=OPDS,s_opds_page,s_opds_depth;static bool s_scan_pending;
static char s_opds_url[256]="https://test/opds",s_opds_history[6][256],s_opds_status[128];
static bool s_shelf_cache_valid=true;
static unsigned fetched,downloaded,closed;static char fetched_url[256];
static void copy_text(char*d,size_t n,const char*s){snprintf(d,n,"%s",s);}
static bool ui_rect_hit(EpdRect r,int x,int y){return x>=r.x&&x<r.x+r.width&&y>=r.y&&y<r.y+r.height;}
static int ui_nav_hit(int x,int y){return y>=1096?x/171:-1;}
static void ui_nav_request(app_ctx_t*c,int t){(void)c;(void)t;}
static int ui_pager_hit_at(int x,int y,int top){return y>=top&&y<top+70?(x<342?0:1):-1;}
static int ui_pager_target(int p,int current,int pages){return p==0?(current?current-1:0):(current+1<pages?current+1:current);}
static void opds_session_end(void){++closed;}
static int opds_fetch_feed(const char*u,opds_feed_t*f,char*m,size_t cap){(void)m;(void)cap;++fetched;copy_text(fetched_url,sizeof(fetched_url),u);memset(f,0,sizeof(*f));f->count=1;strcpy(f->entries[0].title,"Book");strcpy(f->entries[0].href,"https://test/book.epub");f->entries[0].is_book=true;return ESP_OK;}
typedef struct{app_ctx_t *ctx;const char *title;EpdRect modal;}opds_ui_progress_ctx_t;
static void opds_download_progress_cb(size_t a,size_t b,void*c){(void)a;(void)b;(void)c;}
static int opds_download_book(const opds_entry_t*e,char*p,size_t cap,void(*cb)(size_t,size_t,void*),void*c,char*m,size_t n){(void)p;(void)cap;(void)cb;(void)c;(void)m;(void)n;assert(e->is_book&&!strcmp(e->href,"https://test/book.epub"));++downloaded;return ESP_OK;}
static void book_store_notify_changed(void){}
static app_redraw_t action_at(app_ctx_t*ctx,uint16_t x,uint16_t y){
'''+s[a:b]+'''return APP_REDRAW_NONE;}\n'''+function('opds_control_at',src)+'\n'+function('opds_gesture',src)+r'''
int main(void){app_ctx_t ctx={0};feed.count=1;strcpy(feed.entries[0].title,"Recently Added");strcpy(feed.entries[0].href,"https://test/opds/new");
 ui_gesture_event_t e={UI_GESTURE_TAP,100,250,100,250};
 assert(opds_gesture(&ctx,&e)==APP_REDRAW_PAGE&&fetched==1&&!strcmp(fetched_url,"https://test/opds/new")&&s_opds_depth==1);
 assert(opds_gesture(&ctx,&e)==APP_REDRAW_PAGE&&downloaded==1&&!s_shelf_cache_valid&&s_view==OPDS);
 e.x=650;assert(opds_gesture(&ctx,&e)==APP_REDRAW_NONE&&downloaded==1);
 e.x=100;e.type=2;assert(opds_gesture(&ctx,&e)==APP_REDRAW_NONE);
 e.type=UI_GESTURE_TAP;e.y=e.y0=100;e.x=e.x0=50;assert(opds_gesture(&ctx,&e)==APP_REDRAW_PAGE&&fetched==2&&!strcmp(fetched_url,"https://test/opds"));
 assert(opds_gesture(&ctx,&e)==APP_REDRAW_PAGE&&s_view==IMPORT&&closed==1);
 puts("PASS actual OPDS hit/gesture/action: root -> Recently Added -> book download -> parent -> close; drag/non-tap rejection");}
'''
assert 'if(s_view==OPDS)return opds_gesture(ctx,ev);' in function('gesture_event',src)
with tempfile.TemporaryDirectory() as t:
 p=Path(t);(p/'test.c').write_text(unit);subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(p/'test.c'),'-o',str(p/'test')],check=True);subprocess.run([str(p/'test')],check=True)
