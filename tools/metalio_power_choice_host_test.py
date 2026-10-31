"""Execute the actual S31 two-choice power render and touch blocks together."""
from pathlib import Path
import subprocess
import tempfile
import os

root = Path(__file__).resolve().parents[1]
source = (root / 'main/apps/app_device_settings.c').read_text()
# The maintained source still includes the Pico alternative; generated views omit it.
start = source.index('    if (s_page == SETTINGS_POWER_SLEEP) {')
end = source.find('\n#else', start)
if end < 0:
    end = source.index('    if (s_page == SETTINGS_BOOT)', start)
paint = source[start:end]
start = source.index('    if (s_page == SETTINGS_POWER_SLEEP) {', source.index('static app_redraw_t on_gesture'))
end = source.find('\n#else', start)
if end < 0:
    end = source.index('    if (s_page == SETTINGS_CONFIG)', start)
tap = source[start:end]
assert 'SETTINGS_LOCK_SLEEP' not in source
assert 'Sleep sau 5 phút' not in paint and 'Sleep sau 10 phút' not in paint
assert 'app_settings_set_auto_lock_minutes' not in tap
unit = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef struct {int x,y,width,height;} EpdRect;
typedef struct {int x0,y0;} Event;
enum {SETTINGS_POWER_SLEEP=1,APP_REDRAW_NONE=0,APP_REDRAW_PAGE=2};
#define UI_GRAY_WHITE 255
#define EPD_DRAW_ALIGN_LEFT 0
#define UI_GRAY_BLACK 0
static int s_page=SETTINGS_POWER_SLEEP,cards,dots;

static unsigned hours=1;
static int save_error;
#define APP_SLEEP_OFF_HOURS_MIN 0
#define ESP_OK 0
#define APP_SLEEP_OFF_HOURS_MAX 24
static unsigned app_settings_sleep_off_hours(void){return hours;}
static int app_settings_set_sleep_off_hours(uint8_t h){if(save_error)return save_error;hours=h;return 0;}
static EpdRect boxes[2];
static int titles;

static bool ui_rect_hit(EpdRect r,int x,int y){return x>=r.x&&x<r.x+r.width&&y>=r.y&&y<r.y+r.height;}
static void back_header(uint8_t*f,const char*t){(void)f;assert(!strcmp(t,"Chế độ tắt nguồn / ngủ"));}
static void section(uint8_t*f,int y,const char*t){(void)f;(void)y;(void)t;}
static void settings_card(uint8_t*f,EpdRect r,int radius,int fill,int border){
 (void)f;(void)radius;(void)border;if(r.y==690){assert(r.width==80&&r.height==72);return;}assert(cards<2);boxes[cards]=r;
 assert(fill==(((hours>0)==(cards==1))?0xd0:255));++cards;
}
static void ui_text(uint8_t*f,int x,int y,int px,const char*t,int align,bool bold){
 (void)f;(void)x;(void)y;(void)px;(void)align;(void)bold;
 assert(!strstr(t,"锁屏省电方式")&&!strstr(t,"保留待机"));
 if(!strcmp(t,"Chỉ Sleep")||!strcmp(t,"Sleep và tắt nguồn"))++titles;
}
static void ui_text_fixed_vc(uint8_t*f,int x,int y,int px,const char*t,int align,bool bold){(void)f;(void)x;(void)y;(void)px;(void)t;(void)align;(void)bold;}
#define EPD_DRAW_ALIGN_CENTER 1
static void epd_fill_circle(int x,int y,int r,int c,uint8_t*f){(void)x;(void)y;(void)r;(void)c;(void)f;++dots;}
static void settings_navigation(uint8_t*f){(void)f;}
'''
unit+=r"""
static void ui_text_box(uint8_t *fb,EpdRect r,int px,const char *text,int align,bool active){assert(r.x>=0&&r.y>=0&&r.x+r.width<=684&&r.y+r.height<=1132);ui_text(fb,r.x,r.y,px,text,align,active);}
static void settings_line(uint8_t *fb,int x,int y,int px,const char *text,int align,bool active){ui_text(fb,x,y,px,text,align,active);}
"""
unit += 'static void paint(void){uint8_t *fb=NULL;' + paint + '}\n'
unit += 'static int tap(int x,int y){Event e={x,y};const Event *ev=&e;' + tap + 'return APP_REDRAW_NONE;}\n'
unit += r'''
int main(void){
 for(unsigned initial=0;initial<=24;initial+=initial?23:1){
  hours=initial;cards=dots=titles=0;paint();assert(cards==2&&dots==1&&titles==2);
  for(int y=0;y<1216;y+=3)for(int x=0;x<684;x+=3){
   hours=initial;int target=-1;
   for(int i=0;i<2;i++)if(ui_rect_hit(boxes[i],x,y))target=i;
   int delta=initial?(ui_rect_hit((EpdRect){36,690,80,72},x,y)?-1:ui_rect_hit((EpdRect){568,690,80,72},x,y)?1:0):0;
   bool allowed=delta&&((int)initial+delta>=1)&&((int)initial+delta<=24);
   int result=tap(x,y);
   assert(result==((target>=0||allowed)?APP_REDRAW_PAGE:APP_REDRAW_NONE));
   unsigned expected=target==0?0:target==1?(initial?initial:1):allowed?initial+delta:initial;
   assert(hours==expected);
  }
 }
 hours=0;assert(tap(76,726)==APP_REDRAW_NONE&&hours==0);
 hours=1;assert(tap(76,726)==APP_REDRAW_NONE&&hours==1);
 hours=24;assert(tap(608,726)==APP_REDRAW_NONE&&hours==24);
 hours=0;assert(tap(100,500)==APP_REDRAW_PAGE&&hours==1);
 hours=7;assert(tap(100,500)==APP_REDRAW_PAGE&&hours==7);
 save_error=9;assert(tap(100,350)==APP_REDRAW_NONE&&hours==7);
 assert(tap(608,726)==APP_REDRAW_NONE&&hours==7);
 puts("PASS: actual two sleep policies,0/1/24 selection,1..24 hour bounds/hidden controls, failed saves, no5/10minute timers or auto-lock mutation");
}

'''
with tempfile.TemporaryDirectory() as folder:
    path = Path(folder)
    (path / 'test.c').write_text(unit)
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', *(['-fsanitize=address,undefined'] if os.environ.get('HOST_TEST_SANITIZERS','1')!='0' else []),
                    str(path / 'test.c'), '-o', str(path / 'test')], check=True)
    subprocess.run([str(path / 'test')], check=True)
