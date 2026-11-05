"""中文：执行界面切换的保存/首页/清屏分支。/ English: Execute UI-mode save, Home routing and cleanup branch."""
from pathlib import Path
import subprocess,tempfile
from home_cover_cache_host_test import function
root=Path(__file__).resolve().parents[1]
s=function('settings_choice_gesture',root/'main/apps/app_device_settings.c')
a=s.index('    if(s_setting_choice.kind==SET_PICK_UI_MODE)');b=s.index('    else if(s_setting_choice.kind==SET_PICK_SIZE)',a)
u=r'''
#include <assert.h>
#include <stdbool.h>
#define ESP_OK 0
#define APP_REDRAW_NONE 0
#define SET_PICK_UI_MODE 7
typedef struct {void *request_app;} app_ctx_t;
static struct {int kind;bool open;} s_setting_choice;
static unsigned mode,cleans,releases,covers,clocks;static int error,home;
static int app_settings_set_ui_mode(unsigned v){mode=v;return error;}
static void app_book_release_shelf_cache(void){++releases;}
static void app_home_cover_mode_changed(void){++covers;}
static void app_home_release_clock_cache(void){++clocks;}
static void display_request_full_refresh(void){++cleans;}
static void *app_home_page(void){return &home;}
static int select_mode(app_ctx_t *ctx,int index){bool ok=false;
'''+s[a:b]+r'''
return ok?1:-1;}
int main(void){for(int i=0;i<2;++i){app_ctx_t c={0};s_setting_choice.kind=7;s_setting_choice.open=true;error=0;
 assert(select_mode(&c,i)==0&&mode==(i?2u:0u)&&!s_setting_choice.open&&c.request_app==&home);
 assert(cleans==(unsigned)i+1&&releases==cleans&&covers==cleans&&clocks==cleans);}
 app_ctx_t c={0};s_setting_choice.open=true;error=1;assert(select_mode(&c,1)==-1&&s_setting_choice.open&&!c.request_app&&cleans==2);
}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp)/'test.c';p.write_text(u)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined,bounds',str(p),'-o',str(p.with_suffix(''))],check=True)
 subprocess.run([str(p.with_suffix(''))],check=True)
print('PASS both modes route Home with one clean after save; failed save retains picker and screen')
