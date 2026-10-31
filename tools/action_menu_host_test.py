#!/usr/bin/env python3
"""Real configurable action menu, NVS and AI reader dispatch regressions.
SPDX-License-Identifier: Apache-2.0
"""
from pathlib import Path
import re,subprocess,tempfile
from home_cover_cache_host_test import function
ROOT=Path(__file__).resolve().parents[1]
settings=(ROOT/'main/settings.h').read_text()
enums='\n'.join(re.search(r'typedef enum \{[^{}]+\} '+name+r';',settings).group() for name in ['app_ai_action_t','app_reader_key_action_t'])
macros='\n'.join(line for line in settings.splitlines() if line.startswith(('#define APP_AI_MENU_COUNT','#define APP_ACTION_MENU_','#define APP_SLEEP_OFF_HOURS_DEFAULT')))
common=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
'''+enums+'\n'+macros+'\n'
menu=common+r'''
typedef struct{int x,y,width,height;}EpdRect;
typedef struct{bool touched;int count,x,y;}cst836u_touch_t;
typedef struct{const cst836u_touch_t *touch;bool pressed,released;}app_ctx_t;
'''+re.search(r'typedef struct \{[^{}]+\} ui_ai_menu_t;', (ROOT/'main/ui/ui_ai_menu.h').read_text()).group()+r'''
enum{UI_AI_CLOSE=-1,UI_AI_NO_ACTION=-2,EPD_DRAW_ALIGN_CENTER,UI_GRAY_WHITE=255,UI_GRAY_BLACK=0};
enum{UI_ICON_REFRESH_CW,UI_ICON_BOOKMARK,UI_ICON_LIGHTBULB,UI_ICON_HOUSE,UI_ICON_X,UI_ICON_ROTATE_CW,UI_ICON_CHEVRON_LEFT,UI_ICON_CHEVRON_RIGHT,UI_ICON_LIST};
typedef int ui_icon_t;
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
static unsigned width=684,height=1216,rotation,live,allocations;static bool oom;
static uint32_t mask=APP_ACTION_MENU_DEFAULT;
static uint32_t app_settings_action_menu(void){return mask;}
static unsigned epd_rotated_display_width(void){return width;}
static unsigned epd_rotated_display_height(void){return height;}
static int epd_get_rotation(void){return rotation;}
static void *heap_caps_calloc(size_t n,size_t bytes,int caps){assert(caps==3);if(oom)return NULL;++live;++allocations;return calloc(n,bytes);}
static void heap_caps_free(void *p){if(p){--live;free(p);}}
static bool ui_rect_hit(EpdRect r,int x,int y){return x>=r.x&&y>=r.y&&x<r.x+r.width&&y<r.y+r.height;}
static uint8_t ui_read_pixel(const uint8_t *fb,int x,int y){assert(x>=0&&y>=0&&(unsigned)x<width&&(unsigned)y<height);return fb[(size_t)y*width+x];}
static void epd_draw_pixel(int x,int y,int shade,uint8_t *fb){assert(x>=0&&y>=0&&(unsigned)x<width&&(unsigned)y<height);fb[(size_t)y*width+x]=shade>>4;}
static void bounds(EpdRect r){assert(r.x>=0&&r.y>=0&&r.width>0&&r.height>0&&(unsigned)(r.x+r.width)<=width&&(unsigned)(r.y+r.height)<=height);}
static void ui_fill_round_rect(uint8_t *fb,EpdRect r,int rad,int shade){(void)rad;bounds(r);for(int y=r.y;y<r.y+r.height;++y)for(int x=r.x;x<r.x+r.width;++x)epd_draw_pixel(x,y,shade,fb);}
static void ui_draw_control_frame(uint8_t *fb,EpdRect r,int rad,int shade){(void)rad;bounds(r);epd_draw_pixel(r.x,r.y,shade,fb);}
static void epd_draw_circle(int x,int y,int radius,int shade,uint8_t *fb){(void)radius;epd_draw_pixel(x,y,shade,fb);}
static void ui_draw_icon(uint8_t *fb,int x,int y,int size,int icon,int shade){(void)size;(void)icon;epd_draw_pixel(x,y,shade,fb);}
static void ui_text_fixed_vc(uint8_t *fb,int x,int y,int px,const char *t,int a,bool inv){(void)px;(void)t;(void)a;(void)inv;epd_draw_pixel(x,y,0,fb);}
'''
menu+='\n'.join(line for line in (ROOT/'main/ui/ui_ai_menu.c').read_text().splitlines() if not line.startswith('#include'))
menu+=r'''
int main(void){
 for(unsigned orientation=0;orientation<2;++orientation){width=orientation?1216:684;height=orientation?684:1216;rotation=orientation;
  size_t bytes=(size_t)width*height;uint8_t *fb=malloc(bytes),*original=malloc(bytes);assert(fb&&original);
  for(size_t i=0;i<bytes;++i)fb[i]=original[i]=i%16;
  uint32_t masks[]={0,1u<<APP_AI_KOSYNC,APP_ACTION_MENU_DEFAULT,APP_ACTION_MENU_ALLOWED};
  for(unsigned set=0;set<4;++set){mask=masks[set];ui_ai_menu_t m={0};
   assert(ui_ai_menu_begin(&m,fb,APP_AI_NEXT_PAGE,true));bounds(m.rect);assert(live==1);
   assert(!ui_ai_menu_begin(&m,fb,0,false));unsigned count=0;for(unsigned a=0;a<APP_AI_ACTION_COUNT;++a)if(mask&(1u<<a))++count;assert((unsigned)m.count==count);
   cst836u_touch_t touch={0};app_ctx_t ctx={.touch=&touch};assert(ui_ai_menu_feed(&m,&ctx,true)==UI_AI_NO_ACTION);
   unsigned before=allocations;
   for(int i=0;i<m.count;++i){EpdRect r=ui_ai_menu_option(&m,i);bounds(r);assert(r.y+r.height<m.rect.y+m.rect.height);
    touch=(cst836u_touch_t){true,1,r.x+r.width/2,r.y+r.height/2};ctx.pressed=true;ctx.released=false;assert(ui_ai_menu_feed(&m,&ctx,true)==UI_AI_NO_ACTION);
    touch.touched=false;ctx.pressed=false;ctx.released=true;assert(ui_ai_menu_feed(&m,&ctx,true)==(int)m.actions[i]);
   }
   touch=(cst836u_touch_t){true,1,0,0};ctx.pressed=true;ctx.released=false;ui_ai_menu_feed(&m,&ctx,true);
   touch.touched=false;ctx.pressed=false;ctx.released=true;assert(ui_ai_menu_feed(&m,&ctx,true)==UI_AI_CLOSE&&before==allocations);
   assert(ui_ai_menu_restore(&m,fb));ui_ai_menu_end(&m);assert(!live&&!memcmp(fb,original,bytes));
  }
  ui_ai_menu_t m={0};oom=true;assert(!ui_ai_menu_begin(&m,fb,0,false)&&!live);oom=false;free(fb);free(original);
 }
 puts("PASS actual action menu:0/1/default/all10 actions, portrait/landscape bounds, execute IDs, outside close, no input allocations, exact restore and OOM");
}
'''
persist=common+r'''
typedef int esp_err_t;typedef int nvs_handle_t;
#define ESP_OK 0
#define ESP_ERR_INVALID_ARG 1
#define NVS_NS "test"
#define NVS_READWRITE 1
static uint32_t s_action_menu=APP_ACTION_MENU_DEFAULT,stored;
static bool s_power_action_menu;
static app_ai_action_t s_ai_short=APP_AI_REFRESH;
static int error_at,calls,closed;
static int op(void){return ++calls==error_at?9:0;}
static int nvs_open(const char *ns,int mode,int *h){(void)ns;(void)mode;*h=1;return op();}
static int nvs_set_u32(int h,const char *key,uint32_t v){(void)h;assert(!strcmp(key,"action_menu"));stored=v;return op();}
static int nvs_set_u8(int h,const char *key,uint8_t v){(void)h;assert(!strcmp(key,"power_menu")||!strcmp(key,"ai_short"));stored=v;return op();}
static int nvs_commit(int h){assert(h==1);return op();}
static void nvs_close(int h){assert(h==1);++closed;}
'''
for name in ['app_settings_action_menu','app_settings_set_action_menu','app_settings_power_action_menu','app_settings_set_power_action_menu','app_settings_ai_short','app_settings_set_ai_short']:persist+=function(name,ROOT/'main/settings.c')+'\n'
persist+=r'''
int main(void){
 _Static_assert(APP_SLEEP_OFF_HOURS_DEFAULT==0,"Fresh settings must default sleep-only");
 uint32_t old=app_settings_action_menu();assert(old&(1u<<APP_AI_KOSYNC));
 for(int fail=1;fail<=3;++fail){calls=0;error_at=fail;assert(app_settings_set_action_menu(0)!=0&&app_settings_action_menu()==old);}
 calls=0;error_at=0;assert(app_settings_set_action_menu(0)==0&&stored==0&&app_settings_action_menu()==0);
 assert(app_settings_set_action_menu(1u<<APP_AI_ACTION_MENU)==ESP_ERR_INVALID_ARG);
 assert(app_settings_set_action_menu(1u<<APP_AI_NONE)==ESP_ERR_INVALID_ARG);
 assert(app_settings_set_action_menu(APP_ACTION_MENU_ALLOWED)==0);
 calls=0;error_at=3;assert(app_settings_set_power_action_menu(true)!=0&&!app_settings_power_action_menu());
 calls=0;error_at=0;assert(app_settings_set_power_action_menu(true)==0&&app_settings_power_action_menu());
 assert(app_settings_set_ai_short(APP_AI_NEXT_PAGE)==0&&app_settings_ai_short()==APP_AI_NEXT_PAGE&&stored==APP_AI_NEXT_PAGE);
 calls=0;error_at=3;assert(app_settings_set_ai_short(APP_AI_ACTION_MENU)!=0&&app_settings_ai_short()==APP_AI_NEXT_PAGE);
 calls=0;error_at=0;assert(app_settings_set_ai_short(APP_AI_ACTION_MENU)==0&&app_settings_ai_short()==APP_AI_ACTION_MENU);
 puts("PASS actual NVS setters: defaultSleep-only, KoSync default, empty/full masks, invalid recursion/none, save failures preserve settings, power button mapping");
}
'''
dispatch=common+r'''
typedef int app_redraw_t;
enum{APP_REDRAW_NONE,APP_REDRAW_PAGE,APP_REDRAW_FULL};
typedef struct{int unused;}app_ctx_t;
typedef struct{int unused;}app_desc_t;
static const app_desc_t app_book={1},other={2};
static bool visible,reading=true;static unsigned requests,turns,chapters,tools,syncs,marks,lights,home,haptics;
static int last;static app_ai_action_t chosen;
static void app_action_menu_request(void){++requests;}
static bool app_book_reader_body_visible(void){return visible;}
static bool app_book_ai_bookmark(void){++marks;return true;}
static void metalio_haptic_click(void){++haptics;}
static void metalio_frontlight_toggle(void){++lights;}
static void ui_nav_request(app_ctx_t *c,int tab){(void)c;assert(!tab);++home;}
static void app_book_request_kosync(app_ctx_t *c){(void)c;++syncs;}
static app_ai_action_t app_settings_ai_short(void){return chosen;}
static int app_book_handle_key_action(app_ctx_t *c,int a){(void)c;if(!reading)return 0;last=a;if(a==APP_READER_KEY_PREV||a==APP_READER_KEY_NEXT)++turns;else if(a==APP_READER_KEY_TOOLS)++tools;else ++chapters;return APP_REDRAW_PAGE;}
'''
for name in ['ai_run_action','ai_short_action']:dispatch+=function(name,ROOT/'main/app/app_loop.c')+'\n'
dispatch+=r'''
int main(void){app_ctx_t c={0};
 chosen=APP_AI_NEXT_PAGE;assert(ai_short_action(&c,&app_book)==APP_REDRAW_PAGE&&turns==1&&last==APP_READER_KEY_NEXT);
 chosen=APP_AI_PREV_PAGE;assert(ai_short_action(&c,&app_book)==APP_REDRAW_PAGE&&turns==2&&last==APP_READER_KEY_PREV);
 chosen=APP_AI_NEXT_CHAPTER;assert(ai_short_action(&c,&app_book)==APP_REDRAW_PAGE&&chapters==1);
 assert(ai_short_action(&c,&other)==APP_REDRAW_NONE&&chapters==1);
 reading=false;assert(ai_short_action(&c,&app_book)==APP_REDRAW_NONE&&chapters==1);reading=true;
 chosen=APP_AI_ACTION_MENU;assert(ai_short_action(&c,&other)==APP_REDRAW_NONE&&requests==1);
 assert(ai_run_action(&c,&app_book,APP_AI_KOSYNC)==APP_REDRAW_PAGE&&syncs==1);
 assert(ai_run_action(&c,&app_book,APP_AI_TOOLS)==APP_REDRAW_PAGE&&tools==1);
 puts("PASS actual AI short/menu dispatch: page/chapter actions work with reader toolbar open, skip non-reader/shelf, KoSync/tools/menu execute without changing AI mapping");
}
'''
bindings=common+r"""
#define KIIKO_BOARD_METALIO47 1
#define APP_CONTROL_HOLD_DEFAULT 255
#define UI_KEY_1 1
#define UI_KEY_2 2
#define UI_KEY_3 3
#define READ_PICO_TRANSFER_UPLOADING 9
enum{APP_REDRAW_NONE,APP_REDRAW_PAGE,APP_REDRAW_FULL};
typedef int app_redraw_t;
typedef struct{void *request_app;bool request_menu,request_return;}app_ctx_t;
typedef struct{bool holds_pmu,owns_keys;bool(*navigation_enabled)(app_ctx_t*);int(*on_key)(app_ctx_t*,int);int(*on_key_long)(app_ctx_t*,int);}app_desc_t;
const app_desc_t app_book={0};
static uint8_t holds[7],shorts[3],vols[2];static unsigned requests,fallbacks;
static bool blocked,root_page,ready=true;
static uint8_t app_settings_control_hold(unsigned key){return holds[key];}
static app_reader_key_action_t app_settings_reader_key_action(unsigned key){return shorts[key];}
static app_reader_key_action_t app_settings_volume_key_action(unsigned key){return vols[key];}
static void app_action_menu_request(void){++requests;}
static bool usb_storage_active(void){return blocked;}
static bool pico_online_busy(void){return false;}
static bool app_download_active(void){return false;}
typedef struct{int state;}read_pico_transfer_status_t;
static void read_pico_transfer_get_status(read_pico_transfer_status_t *s){s->state=0;}
static void ui_nav_request(app_ctx_t *c,int tab){(void)c;(void)tab;}
static void metalio_frontlight_toggle(void){}
static bool app_book_reader_body_visible(void){return ready;}
static int app_book_handle_key_action(app_ctx_t *c,int action){(void)c;(void)action;return 0;}
static bool main_page_ready(app_ctx_t *c,const app_desc_t *app){(void)c;(void)app;return root_page;}
static int main_key_action(app_ctx_t *c,const app_desc_t *app,int key,bool held){(void)c;(void)app;(void)key;(void)held;++fallbacks;return 0;}
static bool app_book_volume_key(app_ctx_t *c,int dir,int *out){(void)c;(void)dir;(void)out;return false;}
"""
for name in ['front_key_opens_action_menu','front_key_dispatch','control_hold_action','volume_key_action']:bindings+=function(name,ROOT/'main/app/app_loop.c')+'\n'
bindings+=r"""
int main(void){app_ctx_t ctx={0};app_desc_t other={0};
 for(unsigned key=0;key<7;++key){holds[key]=APP_READER_KEY_ACTION_MENU;assert(control_hold_action(&ctx,&other,key)==APP_REDRAW_NONE);}
 assert(requests==7);blocked=true;control_hold_action(&ctx,&other,0);assert(requests==7);blocked=false;
 for(unsigned i=0;i<3;++i)shorts[i]=APP_READER_KEY_ACTION_MENU;
 for(unsigned root=0;root<2;++root){root_page=root;for(int key=1;key<=3;++key){front_key_dispatch(&ctx,&other,key,false);front_key_dispatch(&ctx,&other,key,true);}}
 assert(requests==19&&!fallbacks);
 vols[0]=vols[1]=APP_READER_KEY_ACTION_MENU;volume_key_action(&ctx,&other,-1);volume_key_action(&ctx,&other,1);assert(requests==21);
 puts("PASS actual routing: all7 holds, front3 short/hold on roots/subpages, volume short menu and busy guards");
}
"""
# Render the actual settings branch over arbitrary old page pixels. This catches
# the missing clear even when every new row is individually drawn correctly.
settings_source=(ROOT/'main/apps/app_device_settings.c').read_text()
a=settings_source.index('    if(s_page==SETTINGS_ACTION_MENU) {')
b=settings_source.index('{',a)+1;depth=1
while depth:
 depth+=(settings_source[b]=='{')-(settings_source[b]=='}');b+=1
screen=common+r"""
typedef struct{int x,y,width,height;}EpdRect;
enum{SETTINGS_ACTION_MENU=1,UI_GRAY_WHITE=255,UI_GRAY_BLACK=0,EPD_DRAW_ALIGN_LEFT=0};
static int s_page=SETTINGS_ACTION_MENU,clears;
static uint32_t chosen_mask=APP_ACTION_MENU_DEFAULT;
static char s_notice[96];
static uint8_t fresh[684*1216],dirty[684*1216];
static uint32_t app_settings_action_menu(void){return chosen_mask;}
static void paint(uint8_t *fb,EpdRect r,int value){assert(r.x>=0&&r.y>=0&&r.width>0&&r.height>0&&r.x+r.width<=684&&r.y+r.height<=1216);for(int y=r.y;y<r.y+r.height;++y)memset(fb+y*684+r.x,value,r.width);}
static void ui_clear_page(uint8_t *fb){++clears;memset(fb,255,sizeof(fresh));}
static void ui_nav_status(uint8_t *fb){paint(fb,(EpdRect){24,12,636,30},100);}
static void back_header(uint8_t *fb,const char *title){assert(!strcmp(title,"Menu thao tác nhanh"));paint(fb,(EpdRect){36,70,612,88},50);}
static void ui_text_box(uint8_t *fb,EpdRect r,int px,const char *text,int align,bool inverse){(void)px;(void)text;(void)align;(void)inverse;paint(fb,r,180);}
static void settings_card(uint8_t *fb,EpdRect r,int radius,int fill,int border){(void)radius;(void)border;paint(fb,r,fill);}
static void epd_fill_circle(int x,int y,int radius,int color,uint8_t *fb){paint(fb,(EpdRect){x-radius,y-radius,2*radius,2*radius},color);}
static void settings_line(uint8_t *fb,int x,int y,int px,const char *text,int align,bool inverse){ui_text_box(fb,(EpdRect){x,y,560,32},px,text,align,inverse);}
static void settings_navigation(uint8_t *fb){paint(fb,(EpdRect){0,1132,684,84},160);}
"""
screen+=function('ai_action_label',ROOT/'main/apps/app_device_settings.c')+'\n'
screen+='static void screen_draw(uint8_t *fb){'+settings_source[a:b]+'}\n'
screen+=r"""
int main(void){uint32_t masks[]={APP_ACTION_MENU_DEFAULT,APP_ACTION_MENU_ALLOWED,0,1u<<APP_AI_KOSYNC};
 for(unsigned m=0;m<4;++m){chosen_mask=masks[m];
  memset(fresh,255,sizeof(fresh));screen_draw(fresh);
  for(size_t i=0;i<sizeof(dirty);++i)dirty[i]=(uint8_t)(i*13u+7u);
  screen_draw(dirty);assert(!memcmp(fresh,dirty,sizeof(fresh)));
  chosen_mask=APP_ACTION_MENU_ALLOWED;screen_draw(dirty);
  chosen_mask=masks[m];screen_draw(dirty);assert(!memcmp(fresh,dirty,sizeof(fresh)));
 }
 assert(clears==16);
 puts("PASS actual settings renderer: old header/rows/gap pixels cleared, selection toggles match fresh render, all/empty/KoSync masks, status/footer retained");
}
"""
with tempfile.TemporaryDirectory() as temp:
 p=Path(temp)
 for name,unit in [('menu',menu),('persist',persist),('dispatch',dispatch),('bindings',bindings),('settings-screen',screen)]:
  c=p/(name+'.c');exe=p/name;c.write_text(unit)
  subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(c),'-o',str(exe)],check=True)
  subprocess.run([str(exe)],check=True)
