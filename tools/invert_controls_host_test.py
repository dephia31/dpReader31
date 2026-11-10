"""Execute real front/volume/AI dispatch and the shared persistent toggle."""
from pathlib import Path
from home_cover_cache_host_test import function
import re,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
header=(root/'main/settings.h').read_text()
reader=re.search(r'typedef enum \{\s*APP_READER_KEY_PREV[\s\S]*?app_reader_key_action_t;',header)[0]
ai=re.search(r'typedef enum \{\s*APP_AI_REFRESH[\s\S]*?app_ai_action_t;',header)[0]
unit=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#define DPREADER_BOARD_METALIO47 1
#define UI_KEY_1 1
#define UI_KEY_2 2
#define UI_KEY_3 3
typedef enum{APP_REDRAW_NONE,APP_REDRAW_PAGE,APP_REDRAW_FULL}app_redraw_t;
typedef struct{bool request_app,request_menu,request_return;}app_ctx_t;
typedef struct{bool owns_keys;app_redraw_t(*on_key)(app_ctx_t*,int);app_redraw_t(*on_key_long)(app_ctx_t*,int);}app_desc_t;
'''+reader+ai+r'''
static const app_desc_t app_book={0},other={0};
static bool inverted,fail,usb,busy,home;
static unsigned writes,refreshes,forwarded;
static unsigned reader_keys[3],holds[7],volumes[2];
static bool app_settings_screen_inverted(void){return inverted;}
static bool app_settings_set_screen_inverted(bool v){++writes;if(fail)return false;inverted=v;return true;}
static void display_request_full_refresh(void){++refreshes;}
static unsigned app_settings_control_hold(unsigned k){assert(k<7);return holds[k];}
static app_reader_key_action_t app_settings_reader_key_action(unsigned k){assert(k<3);return reader_keys[k];}
static app_reader_key_action_t app_settings_volume_key_action(unsigned k){assert(k<2);return volumes[k];}
static bool usb_storage_active(void){return usb;}
static bool pico_online_busy(void){return busy;}
static bool front_key_opens_action_menu(int k,bool h){(void)k;(void)h;return false;}
static bool front_key_locks(int k,bool h){(void)k;(void)h;return false;}
static void app_action_menu_request(void){}
static void app_lock_request(void){}
static bool main_page_ready(app_ctx_t*c,const app_desc_t*a){(void)c;(void)a;return home;}
static app_redraw_t main_key_action(app_ctx_t*c,const app_desc_t*a,int k,bool h){(void)c;(void)a;(void)k;(void)h;++forwarded;return APP_REDRAW_PAGE;}
static app_redraw_t control_hold_action(app_ctx_t*c,const app_desc_t*a,unsigned k){(void)c;(void)a;(void)k;++forwarded;return APP_REDRAW_NONE;}
static bool app_book_volume_key(app_ctx_t*c,int k,app_redraw_t*r){(void)c;(void)k;(void)r;++forwarded;return false;}
static app_redraw_t app_book_handle_key_action(app_ctx_t*c,int a){(void)c;(void)a;++forwarded;return APP_REDRAW_PAGE;}
static bool app_book_reader_body_visible(void){return true;}
static bool app_book_ai_bookmark(void){return true;}
static bool metalio_haptic_click(void){return true;}
static void metalio_frontlight_toggle(void){}
static void ui_nav_request(app_ctx_t*c,int k){(void)c;(void)k;}
static void app_book_request_kosync(app_ctx_t*c){(void)c;}
'''
unit+=function('display_toggle_inverted',root/'boards/metalio47/src/main/display.c')+'\n'
for name in ('front_key_dispatch','volume_key_action','ai_run_action'):unit+=function(name,root/'main/app/app_loop.c')+'\n'
unit+=r'''
int main(void){
 assert(APP_READER_KEY_LOCK==12&&APP_READER_KEY_INVERT==13&&APP_AI_LOCK==12&&APP_AI_INVERT==13);
 app_ctx_t ctx={0};
 for(int is_home=0;is_home<2;is_home++)for(int bad=0;bad<2;bad++){
  home=is_home;fail=bad;
  for(int key=1;key<=3;key++)for(int held=0;held<2;held++){
   reader_keys[key==1?0:key==2?1:2]=APP_READER_KEY_INVERT;
   holds[key==1?5:key==2?4:6]=APP_READER_KEY_INVERT;
   unsigned w=writes,r=refreshes;bool old=inverted;
   assert(front_key_dispatch(&ctx,&other,key,held)==(bad?APP_REDRAW_NONE:APP_REDRAW_FULL));
   assert(writes==w+1&&refreshes==r+!bad&&inverted==(bad?old:!old));
  }
  for(int v=-1;v<=1;v+=2){
   volumes[v<0?0:1]=APP_READER_KEY_INVERT;unsigned w=writes,r=refreshes;
   assert(volume_key_action(&ctx,&other,v)==(bad?APP_REDRAW_NONE:APP_REDRAW_FULL));
   assert(writes==w+1&&refreshes==r+!bad);
  }
  unsigned w=writes,r=refreshes;
  assert(ai_run_action(&ctx,&app_book,APP_AI_INVERT)==(bad?APP_REDRAW_NONE:APP_REDRAW_FULL));
  assert(writes==w+1&&refreshes==r+!bad);
 }
 fail=false;
 for(int blocker=0;blocker<2;blocker++){
  usb=blocker==0;busy=blocker==1;unsigned w=writes;
  assert(front_key_dispatch(&ctx,&other,1,false)==APP_REDRAW_NONE);
  assert(volume_key_action(&ctx,&other,1)==APP_REDRAW_NONE&&writes==w);
 }
 assert(forwarded==0);
 puts("PASS real invert dispatch: front/hold/volume/AI, Home/reader routing, one persistent toggle + full request, NVS failure and USB/OTA guards, stable stored IDs");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(unit)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
