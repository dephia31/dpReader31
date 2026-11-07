#!/usr/bin/env python3
"""Exercise actual Home utility routes, editor geometry and four-book selection."""
from pathlib import Path
import subprocess,tempfile
from home_cover_cache_host_test import function
root=Path(__file__).resolve().parents[1]
source=root/'main/apps/app_dashboard.c'
text=source.read_text()
settings=(root/'main/settings.h').read_text()
a=settings.index('typedef enum {\n    APP_HOME_UTIL_NONE')
b=settings.index('} app_home_utility_t;',a)+len('} app_home_utility_t;')
names=text[text.index('static const char *const utility_names'):text.index('static const ui_icon_t utility_icons')]
unit=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef struct {int x,y,width,height;} EpdRect;
typedef struct {char path[288];bool has_progress;} home_book_t;
typedef struct {int id;} app_desc_t;
typedef struct {const app_desc_t *request_app;int64_t now_ms;} app_ctx_t;
typedef int esp_err_t;
typedef int app_redraw_t;
enum {ESP_OK,ESP_ERR_NOT_FOUND,ESP_FAIL};
enum {APP_REDRAW_NONE,APP_REDRAW_PAGE,APP_REDRAW_FULL};
enum {HOME_ROOT,HOME_UTILITIES,HOME_UTILITY_PICK,EPD_DRAW_ALIGN_LEFT};
static bool simple;static bool app_settings_simple_ui(void){return simple;}
static bool app_settings_simple_utility_labels(void){return true;}
static bool app_settings_simple_utility_border(void){return true;}
static uint8_t app_settings_simple_utility(unsigned slot){(void)slot;return 0;}
static home_book_t s_current,s_recent[6];
static int s_view;static bool s_save_error;
static uint8_t s_utility_draft[4],saved[4];
static unsigned s_utility_slot,s_utility_page;
static char s_utility_notice[96];
static unsigned buttons;static int tab,opds,ota,transfer,radio,resume,syncs;
static bool s_utility_labels_draft,s_utility_border_draft,labels=true;
static bool app_settings_home_utility_border(void){return true;}static int64_t s_utility_notice_until;static EpdRect s_area;
static int64_t fake_ms=1000;static int64_t esp_timer_get_time(void){return fake_ms*1000;}
static bool app_settings_home_utility_labels(void){return labels;}
static bool bt_enabled;static esp_err_t network_error;
const app_desc_t app_transfer={1};
static home_book_t *featured(void){return &s_current;}
static uint8_t app_settings_home_utility(unsigned i){return saved[i];}
static bool app_settings_ble_turner(void){return bt_enabled;}
static esp_err_t app_toggle_network(bool bt){radio=bt?2:1;bt_enabled=!bt_enabled;return network_error;}
static void ui_nav_request(app_ctx_t *ctx,int i){(void)ctx;tab=i;}
static bool app_book_request_home_sync(const char *p){assert(*p);++syncs;return true;}
static bool app_book_request_open_from_home(const char *p){assert(*p);++resume;return true;}
static void app_book_request_opds(void){++opds;}
static void app_device_settings_request_ota(void){++ota;}
static void app_transfer_request_method_picker(void){++transfer;}
static void header(uint8_t *fb,const char *s){(void)fb;assert(*s);}
static void ui_draw_button(uint8_t *fb,EpdRect r,const char *s,bool selected){(void)fb;(void)selected;assert(*s);assert(r.x>=36&&r.x+r.width<=648&&r.y>=190&&r.y+r.height<=1088);++buttons;}
static void ui_text_caption(uint8_t *fb,EpdRect r,int px,const char *s,int align,bool bold){(void)fb;(void)r;(void)px;(void)s;(void)align;(void)bold;}
'''+settings[a:b]+names
for name in ('utility_button','utility_icon_rect','utility_notice_rect','utility_notice_expire','utility_labels_button','utility_border_button','utility_save_button','recent_cover','recent_books','utilities_configured','utilities_edit','render_utilities_editor','utility_run'):
    unit+=function(name,source)+'\n'
unit+=r'''
int main(void){
 assert(!utilities_configured());saved[2]=APP_HOME_UTIL_WIFI;assert(utilities_configured());utilities_edit();assert(s_view==HOME_UTILITIES&&s_utility_draft[2]==APP_HOME_UTIL_WIFI);
 for(unsigned i=0;i<4;++i){EpdRect u=utility_button(i),r=recent_cover(i);assert(u.x>=36&&u.x+u.width<=648&&u.y>=654&&u.y+u.height<796);assert(r.x>=36&&r.x+r.width<=648&&r.y>796&&r.y+r.height<1018);if(i){EpdRect prev=utility_button(i-1);assert(prev.x+prev.width<u.x);}}
 buttons=0;render_utilities_editor(NULL);assert(buttons==7);
 EpdRect lb=utility_labels_button(),bb=utility_border_button(),sb=utility_save_button();assert(lb.y>=780&&lb.y+lb.height<bb.y&&bb.y+bb.height<sb.y&&sb.y+sb.height<=1088);
 s_view=HOME_UTILITY_PICK;for(s_utility_page=0;s_utility_page<2;++s_utility_page){buttons=0;render_utilities_editor(NULL);assert(buttons==8);}
 home_book_t *books[4];assert(!recent_books(books));
 strcpy(s_current.path,"current");s_current.has_progress=true;
 for(unsigned i=0;i<6;++i){snprintf(s_recent[i].path,288,"book%u",i);s_recent[i].has_progress=true;}
 strcpy(s_recent[0].path,"current");assert(recent_books(books)==4&&books[0]==&s_current&&books[1]==&s_recent[1]&&books[3]==&s_recent[3]);
 s_current.has_progress=false;s_recent[1].has_progress=false;assert(recent_books(books)==4&&books[0]==&s_recent[0]&&books[1]==&s_recent[2]);
 app_ctx_t ctx={0};
 for(unsigned action=1;action<APP_HOME_UTIL_COUNT;++action){tab=-1;radio=0;ctx.request_app=NULL;app_redraw_t result=utility_run(&ctx,action);
 switch(action){
 case APP_HOME_UTIL_RESUME:assert(tab==1&&resume);break;
 case APP_HOME_UTIL_LIBRARY:assert(tab==1);break;
 case APP_HOME_UTIL_OPDS:assert(tab==1&&opds);break;
 case APP_HOME_UTIL_KOSYNC:assert(tab==1&&syncs);break;
 case APP_HOME_UTIL_SETTINGS:assert(tab==3);break;
 case APP_HOME_UTIL_WIFI:assert(radio==1);break;
 case APP_HOME_UTIL_BLUETOOTH:assert(radio==2);break;
 case APP_HOME_UTIL_OTA:assert(tab==3&&ota);break;
 case APP_HOME_UTIL_FILES:assert(tab==2);break;
 case APP_HOME_UTIL_TRANSFER:assert(ctx.request_app==&app_transfer&&transfer);break;
 case APP_HOME_UTIL_REFRESH:assert(result==APP_REDRAW_FULL);break;
 default:assert(false);
 }}
 for(unsigned i=0;i<4;++i)for(int show=0;show<2;++show){EpdRect icon=utility_icon_rect(i,show),button=utility_button(i);assert(icon.width==icon.height&&icon.width==(show?32:64));assert(icon.x+icon.width/2==button.x+button.width/2);assert(icon.y+icon.height/2==button.y+(show?27:button.height/2));assert(icon.x>=button.x&&icon.y>=button.y&&icon.x+icon.width<=button.x+button.width&&icon.y+icon.height<=button.y+button.height);}
 utility_run(&ctx,APP_HOME_UTIL_WIFI);assert(s_utility_notice_until==fake_ms+2000);assert(!utility_notice_expire(fake_ms+1999)&&s_utility_notice[0]);assert(utility_notice_expire(fake_ms+2000)&&!s_utility_notice[0]&&!s_utility_notice_until);assert(!utility_notice_expire(fake_ms+3000)&&s_area.width==484&&s_area.height==100);
 fake_ms=5000;utility_run(&ctx,APP_HOME_UTIL_BLUETOOTH);assert(!utility_notice_expire(6000));fake_ms=6500;utility_run(&ctx,APP_HOME_UTIL_WIFI);assert(!utility_notice_expire(7000));assert(utility_notice_expire(8500));
 s_current.path[0]=0;tab=-1;assert(utility_run(&ctx,APP_HOME_UTIL_RESUME)==APP_REDRAW_PAGE&&tab==-1&&s_utility_notice[0]);
 network_error=ESP_ERR_NOT_FOUND;utility_run(&ctx,APP_HOME_UTIL_WIFI);assert(strstr(s_utility_notice,"Wi-Fi"));
 puts("PASS Home: four non-overlapping slots/covers, editor pages, recent-book deduplication and all shortcut routes");
}
'''
with tempfile.TemporaryDirectory() as d:
    p=Path(d);(p/'test.c').write_text(unit)
    subprocess.run(['cc','-std=c11','-DDPREADER_LOVE_BOOK=0','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
