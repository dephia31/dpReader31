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
static bool staged=true;
static unsigned hours=1;
#define APP_SLEEP_OFF_HOURS_MIN 1
#define APP_SLEEP_OFF_HOURS_MAX 24
static unsigned app_settings_sleep_off_hours(void){return hours;}
static int app_settings_set_sleep_off_hours(uint8_t h){hours=h;return 0;}
static EpdRect boxes[2];
static int titles;
static bool app_settings_staged_shutdown(void){return staged;}
static void app_settings_set_staged_shutdown(bool v){staged=v;}
static bool ui_rect_hit(EpdRect r,int x,int y){return x>=r.x&&x<r.x+r.width&&y>=r.y&&y<r.y+r.height;}
static void back_header(uint8_t*f,const char*t){(void)f;assert(!strcmp(t,"Chế độ tắt nguồn / ngủ"));}
static void section(uint8_t*f,int y,const char*t){(void)f;(void)y;(void)t;}
static void settings_card(uint8_t*f,EpdRect r,int radius,int fill,int border){
 (void)f;(void)radius;(void)border;if(r.y==690){assert(r.width==80&&r.height==72);return;}assert(cards<2);boxes[cards]=r;
 assert(fill==((staged==(cards==0))?0xd0:255));++cards;
}
static void ui_text(uint8_t*f,int x,int y,int px,const char*t,int align,bool bold){
 (void)f;(void)x;(void)y;(void)px;(void)align;(void)bold;
 assert(!strstr(t,"锁屏省电方式")&&!strstr(t,"保留待机"));
 if(!strcmp(t,"Ngủ nhẹ rồi tắt nguồn")||!strcmp(t,"Ngắt nguồn hoàn toàn"))++titles;
}
static void ui_text_fixed_vc(uint8_t*f,int x,int y,int px,const char*t,int align,bool bold){(void)f;(void)x;(void)y;(void)px;(void)t;(void)align;(void)bold;}
#define EPD_DRAW_ALIGN_CENTER 1
static void epd_fill_circle(int x,int y,int r,int c,uint8_t*f){(void)x;(void)y;(void)r;(void)c;(void)f;++dots;}
static void settings_navigation(uint8_t*f){(void)f;}
'''
unit += 'static void paint(void){uint8_t *fb=NULL;' + paint + '}\n'
unit += 'static int tap(int x,int y){Event e={x,y};const Event *ev=&e;' + tap + 'return APP_REDRAW_NONE;}\n'
unit += r'''
int main(void){
 for(int initial=0;initial<2;initial++){
  staged=initial;cards=dots=titles=0;paint();assert(cards==2&&dots==1&&titles==2);
  // Every drawn pixel shares the same hit rectangle; gaps and the removed third entry do nothing.
  for(int y=0;y<1216;y+=3)for(int x=0;x<684;x+=3){
   staged=initial;hours=12;int target=-1;
   for(int i=0;i<2;i++)if(ui_rect_hit(boxes[i],x,y))target=i;
   int result=tap(x,y);
   int delta=ui_rect_hit((EpdRect){36,690,80,72},x,y)?-1:ui_rect_hit((EpdRect){568,690,80,72},x,y)?1:0;
   assert(result==((target>=0||delta)?APP_REDRAW_PAGE:APP_REDRAW_NONE));
   assert(hours==(unsigned)(12+delta));
   assert(staged==(target<0?initial:target==0));
  }
  hours=1;assert(tap(76,726)==APP_REDRAW_NONE&&hours==1);
  hours=24;assert(tap(608,726)==APP_REDRAW_NONE&&hours==24);
  staged=initial;assert(tap(300,800)==APP_REDRAW_NONE&&staged==initial);
 }
 puts("PASS: actual S31 render/touch configured-hour sleep or power-off, hour buttons1..24; identical card/hit bounds, gap/removed-entry isolation, one selection marker");
}
'''
with tempfile.TemporaryDirectory() as folder:
    path = Path(folder)
    (path / 'test.c').write_text(unit)
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', *(['-fsanitize=address,undefined'] if os.environ.get('HOST_TEST_SANITIZERS','1')!='0' else []),
                    str(path / 'test.c'), '-o', str(path / 'test')], check=True)
    subprocess.run([str(path / 'test')], check=True)
