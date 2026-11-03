"""Production reader quick gesture routing and confirmed-wake restore retry."""
from pathlib import Path
import subprocess, tempfile
from home_cover_cache_host_test import function
r=Path(__file__).resolve().parents[1]
sleep=r/'boards/metalio47/src/main/sleep.c'
if not sleep.exists(): sleep=r/'main/sleep.c'
u=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#define UI_LOCK_WIDTH 540
#define UI_LOCK_HEIGHT 960
enum {READING=1,READER_PANEL_NONE=0,UI_GESTURE_SWIPE_D=1,UI_GESTURE_SWIPE_R=2,APP_REDRAW_NONE=0,APP_REDRAW_DONE=4,APP_REDRAW_PAGE=2,APP_REDRAW_FULL=3,UI_KEY_1=1};
typedef int app_redraw_t;
typedef struct {int type,x0,y0;} ui_gesture_event_t;
typedef struct {bool consumed,user_activity,request_quick_menu,request_app,request_return,request_back_ripple;int64_t now_ms;} app_ctx_t;
typedef struct {bool render,holds_pmu;app_redraw_t (*on_gesture)(app_ctx_t*,const ui_gesture_event_t*);app_redraw_t (*on_key)(app_ctx_t*,int);} app_desc_t;
static int s_view=READING,s_reader_panel,s_pressed_control,s_reader_slider;
static bool s_text=true,s_toolbar,s_clear_confirm,s_scan_pending,s_zoom_active;
static int64_t s_stats_activity_ms;
static struct {bool pending;} s_reader_tap;
'''
u+=function('reader_quick_swipe',r/'main/apps/app_book.c')
u+=r'''
static bool busy,usb;static unsigned presents;
static struct {bool open;} s_quick;
static bool pico_online_busy(void){return busy;}
static bool usb_storage_active(void){return usb;}
static void ui_quick_menu_reset(void *q){(void)q;s_quick.open=false;}
static void quick_present(app_ctx_t *c,const app_desc_t *a,bool full){(void)c;(void)a;assert(!full&&s_quick.open);++presents;}
static void ui_click_feedback_reset(void){}
static bool edge_back_contact(app_ctx_t *c,const app_desc_t *a,const ui_gesture_event_t *e){(void)c;(void)a;(void)e;return false;}
static app_redraw_t gesture(app_ctx_t *c,const ui_gesture_event_t *e){reader_quick_swipe(c,e);return APP_REDRAW_NONE;}
'''
u+=function('dispatch_page_gesture',r/'main/app/app_loop.c')
u+=r'''
#define ESP_OK 0
#define ESP_ERR_INVALID_STATE -1
#define ESP_SLEEP_WAKEUP_ALL 99
#define pdMS_TO_TICKS(x) (x)
#define ESP_LOGE(...) ((void)0)
typedef int esp_err_t;
static bool s_lock_keys_restore_pending,s_lock_power_wake_pending;
static unsigned failures,resumes,latches;
static int esp_sleep_disable_wakeup_source(int x){assert(x==99);return 0;}
static int metalio_restore_key_io(void){if(failures){--failures;return -1;}return 0;}
static void read_pico_pmu_keys_resume(void){++resumes;}
static void read_pico_pmu_latch_power_wakeup(void){++latches;}
static void vTaskDelay(int x){assert(x==20);}
'''
u+=function('lock_restore_key_wake',sleep)
u+=r'''
int main(void){
 app_ctx_t c={.now_ms=123};ui_gesture_event_t e={.type=UI_GESTURE_SWIPE_D,.x0=250,.y0=400};
 app_desc_t a={.render=true,.on_gesture=gesture};
 assert(dispatch_page_gesture(&c,&a,&e)==APP_REDRAW_DONE&&presents==1&&c.consumed&&c.user_activity&&!c.request_quick_menu);
 for(int i=0;i<9;++i){
  c=(app_ctx_t){0};s_view=READING;s_text=true;s_reader_panel=0;s_toolbar=s_clear_confirm=s_scan_pending=s_zoom_active=false;
  switch(i){case 0:s_view=0;break;case 1:s_text=false;break;case 2:s_reader_panel=1;break;case 3:s_toolbar=true;break;case 4:s_clear_confirm=true;break;case 5:s_scan_pending=true;break;case 6:s_zoom_active=true;break;case 7:c.consumed=true;break;case 8:e.type=UI_GESTURE_SWIPE_R;break;}
  assert(!reader_quick_swipe(&c,&e)&&!c.request_quick_menu);
 }
 s_view=READING;s_text=true;s_reader_panel=0;s_toolbar=s_clear_confirm=s_scan_pending=s_zoom_active=false;e.type=UI_GESTURE_SWIPE_D;
 for(int i=0;i<3;++i){c=(app_ctx_t){0};busy=i==0;usb=i==1;c.request_app=i==2;assert(dispatch_page_gesture(&c,&a,&e)==APP_REDRAW_NONE&&!c.request_quick_menu&&presents==1);}
 failures=3;s_lock_power_wake_pending=true;lock_restore_key_wake();
 assert(s_lock_keys_restore_pending&&s_lock_power_wake_pending&&!resumes&&!latches);
 lock_restore_key_wake();assert(!s_lock_keys_restore_pending&&!s_lock_power_wake_pending&&resumes==1&&latches==1);
 lock_restore_key_wake();assert(latches==1); // no duplicate click, including double-press mode
 s_lock_power_wake_pending=false;failures=3;lock_restore_key_wake();lock_restore_key_wake();assert(latches==1); // no shared-IRQ authorization
 puts("PASS: actual reader gesture/quick routing guards and confirmed wake retained across input restore failure, once only");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'t.c').write_text(u)
 subprocess.run(['cc','-std=gnu11','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(p/'t.c'),'-o',str(p/'t')],check=True)
 subprocess.run([str(p/'t')],check=True)
