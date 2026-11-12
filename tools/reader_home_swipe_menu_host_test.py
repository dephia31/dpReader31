"""Exercise actual Home menu routing and reader dismissal without hardware."""
from pathlib import Path
import subprocess
import tempfile
from home_cover_cache_host_test import function
root=Path(__file__).resolve().parents[1]
home=root/'main/apps/app_dashboard.c'
popup=root/'firmware/love_book/ui/home.inc'
s=home.read_text();a=s.index('static app_redraw_t on_gesture(');b=s.index('    if(ev->type==UI_GESTURE_PRESS)',a)
prefix=s[a:b]
book=(root/'main/apps/app_book.c').read_text();a=book.index('    // A tap outside the tool strip');b=book.index('    if(s_view==READING&&(s_reader_panel==READER_PANEL_FONT_SETTINGS',a)
unit=r'''
#include <assert.h>
#include <stdbool.h>
#define UI_LOCK_WIDTH 684
#define UI_LOCK_HEIGHT 1216
typedef struct {int x,y,width,height;} EpdRect;
typedef struct {int request_app;bool user_activity,request_quick_menu,request_back_ripple;} app_ctx_t;
typedef struct {int type,x0,y0;bool swipe_leftward;} ui_gesture_event_t;

typedef int app_redraw_t;
enum {APP_REDRAW_NONE,APP_REDRAW_PAGE,APP_REDRAW_AREA,HOME_ROOT,HOME_POP_MENU,HOME_OTHER,UI_GESTURE_PRESS,UI_GESTURE_TAP,UI_GESTURE_SWIPE_U,UI_GESTURE_SWIPE_D,READING,READER_PANEL_TOOLS,READER_PANEL_NONE,APP_HOME_UTIL_TRANSFER,APP_HOME_UTIL_OPDS,APP_HOME_UTIL_SETTINGS};
static int s_view,last_action,routes,s_reader_panel,s_pressed_control;
static bool simple,s_press_pending,s_pop_menu_pending;
static EpdRect s_area;
static struct {bool pending;} s_reader_tap;
static bool app_settings_simple_ui(void){return simple;}
static bool full_mode;
enum { APP_MAIN_REFRESH_NORMAL, APP_MAIN_REFRESH_WATER };
static int refresh_mode = APP_MAIN_REFRESH_WATER;
static int app_settings_main_refresh_mode(void){return refresh_mode;}
static bool app_settings_simple_full_ui(void){return full_mode;}
static int ui_nav_hit(int x,int y){(void)x;return y>=1100?3:-1;}
static void ui_nav_request(app_ctx_t*c,int n){c->request_app=n;}
static int app_at(int n){return n;}
static bool ui_rect_hit(EpdRect r,int x,int y){return x>=r.x&&x<r.x+r.width&&y>=r.y&&y<r.y+r.height;}
static app_redraw_t utility_run(app_ctx_t*c,int a){(void)c;last_action=a;++routes;return APP_REDRAW_NONE;}
'''+function('pop_menu_rect',popup)+'\n'+function('pop_menu_row',popup)+'\n'+function('set_pop_menu_open',home)+'\n'+function('simple_pop_menu_action',popup)+'\n'+prefix+r'''
return APP_REDRAW_NONE;}
static int reader(const ui_gesture_event_t*ev){
'''+book[a:b]+r'''
return APP_REDRAW_NONE;}
int main(void){
 app_ctx_t c={0};
 for(int mode=0;mode<3;++mode){simple=mode!=0;full_mode=mode==2;
  s_view=HOME_ROOT;s_press_pending=true;
  ui_gesture_event_t press={UI_GESTURE_PRESS,300,600,false};
  assert(on_gesture(&c,&press)==APP_REDRAW_NONE&&!s_press_pending);
  if(mode){ui_gesture_event_t up={UI_GESTURE_SWIPE_U,300,600,false};assert(on_gesture(&c,&up)==APP_REDRAW_AREA&&s_view==HOME_POP_MENU);}
  s_view=HOME_ROOT;c.request_app=0;
  ui_gesture_event_t tab={UI_GESTURE_TAP,600,1150,false};
  assert(on_gesture(&c,&tab)==APP_REDRAW_NONE&&c.request_app==(mode==2?0:3));
  for(int y=100;y<=1150;y+=350){s_view=HOME_ROOT;s_press_pending=true;
   ui_gesture_event_t down={UI_GESTURE_SWIPE_D,300,y,false};c.request_quick_menu=false;
   assert(on_gesture(&c,&down)==APP_REDRAW_NONE&&c.request_quick_menu&&!s_press_pending);
  }
 }
 full_mode=false;ui_gesture_event_t e={UI_GESTURE_SWIPE_U,300,600,false};
 simple=true;s_view=HOME_ROOT;s_press_pending=true;assert(on_gesture(&c,&e)==APP_REDRAW_AREA&&s_view==HOME_POP_MENU&&!s_press_pending);
 for(int i=0;i<4;++i){s_view=HOME_POP_MENU;c.request_app=0;c.request_back_ripple=false;routes=0;EpdRect r=pop_menu_row(i);e=(ui_gesture_event_t){UI_GESTURE_TAP,r.x+1,r.y+1,false};assert(on_gesture(&c,&e)==APP_REDRAW_NONE);assert(i?routes==1&&last_action==(i==1?APP_HOME_UTIL_TRANSFER:i==2?APP_HOME_UTIL_OPDS:APP_HOME_UTIL_SETTINGS):c.request_app==5&&!routes);assert(c.request_back_ripple);}
 refresh_mode=APP_MAIN_REFRESH_NORMAL;
 c.request_back_ripple=false;
 assert(on_gesture(&c,&e)==APP_REDRAW_NONE&&!c.request_back_ripple);
 e=(ui_gesture_event_t){UI_GESTURE_TAP,0,0,false};assert(on_gesture(&c,&e)==APP_REDRAW_AREA&&s_view==HOME_ROOT);
 EpdRect panel=pop_menu_rect();
 assert(panel.x==0&&panel.width==UI_LOCK_WIDTH&&s_area.x==panel.x&&s_area.width==panel.width);
 assert(s_pop_menu_pending&&s_area.y==panel.y&&s_area.height==panel.height&&panel.y+panel.height==UI_LOCK_HEIGHT);
 e=(ui_gesture_event_t){UI_GESTURE_TAP,0,0,true};
 assert(on_gesture(&c,&e)==(DPREADER_LOVE_BOOK?APP_REDRAW_AREA:APP_REDRAW_NONE));
 s_view=HOME_ROOT;simple=false;e.swipe_leftward=false;e.type=UI_GESTURE_SWIPE_U;assert(on_gesture(&c,&e)==APP_REDRAW_NONE&&s_view==HOME_ROOT);
 simple=true;s_view=HOME_OTHER;assert(on_gesture(&c,&e)==APP_REDRAW_NONE&&s_view==HOME_OTHER);
 s_view=READING;s_reader_panel=READER_PANEL_TOOLS;s_reader_tap.pending=true;s_pressed_control=4;e=(ui_gesture_event_t){UI_GESTURE_TAP,300,600,false};assert(reader(&e)==APP_REDRAW_PAGE&&s_reader_panel==READER_PANEL_NONE&&!s_reader_tap.pending&&s_pressed_control==-1);
 s_reader_panel=READER_PANEL_TOOLS;e.y0=1096;assert(reader(&e)==APP_REDRAW_NONE&&s_reader_panel==READER_PANEL_TOOLS);
}
'''
with tempfile.TemporaryDirectory() as folder:
 p=Path(folder);(p/'test.c').write_text(unit)
 for love in (0,1):
  subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined','-DDPREADER_BOARD_METALIO47=1',f'-DDPREADER_LOVE_BOOK={love}',str(p/'test.c'),'-o',str(p/'test')],check=True)
  subprocess.run([str(p/'test')],check=True)
print('PASS: simple Home swipe ownership/menu routes/dismissal and reader tap closes tools without invoking page actions')
