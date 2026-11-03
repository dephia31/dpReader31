#!/usr/bin/env python3
"""Exercise production schedule UI fragments and bounded text at 100..200%."""
from pathlib import Path
import re,runpy,subprocess,tempfile
from home_cover_cache_host_test import function
root=Path(__file__).resolve().parents[1];src=root/'main/apps/app_device_settings.c'
fixture=runpy.run_path(str(root/'tools/settings_large_text_host_test.py'))
u=fixture['unit'].split('int main(void)')[0]
u+=r'''
#define SETTINGS_FRONTLIGHT 10
#define SETTINGS_LIGHT_SCHEDULE 11
#define APP_REDRAW_PAGE 1
#define APP_REDRAW_NONE 0
#define ESP_OK 0
typedef int app_redraw_t;
typedef struct{bool enabled;uint16_t on_minute,off_minute;uint8_t level;}metalio_light_schedule_t;
static metalio_light_schedule_t configs[2]={{false,420,1080,30},{false,1080,1380,30}};
static bool s_schedule_warm;static metalio_light_schedule_t s_schedule_draft;static char s_schedule_notice[96];
static int saves,restores,fail_save;
static metalio_light_schedule_t metalio_light_schedule_get(bool warm){return configs[warm];}
static int metalio_light_schedule_save(bool warm,const metalio_light_schedule_t*s){++saves;if(fail_save)return -1;configs[warm]=*s;return 0;}
static void metalio_frontlight_restore(void){++restores;}
static unsigned metalio_brightness(void){return 30;}
static unsigned metalio_warmth(void){return 50;}
static bool metalio_frontlight_enabled(void){return true;}
static bool metalio_haptic_enabled(void){return true;}
static void metalio_frontlight_toggle(void){}
static void metalio_haptic_set_enabled(bool b){}
static void ui_clear_page(uint8_t*f){}
static void ui_text_caption(uint8_t*f,EpdRect r,int px,const char*t,enum EpdFontFlags a,bool inv){ui_text_box(f,r,px,t,a,inv);}
static bool ui_rect_hit(EpdRect r,int x,int y){return x>=r.x&&x<r.x+r.width&&y>=r.y&&y<r.y+r.height;}
typedef struct{int x0,y0;}ui_gesture_event_t;
static struct{bool open;int kind,count,current;}s_setting_choice;
static void ui_choice_begin(void*p,int kind,const char*t,int count,int current){assert(current>=0&&current<count);s_setting_choice.open=true;s_setting_choice.kind=kind;s_setting_choice.count=count;s_setting_choice.current=current;}
'''
text=src.read_text();u+=re.search(r'enum \{SET_PICK_SIZE,[^\n]+',text)[0]+'\n'
for n in ['controls_row','schedule_row_rect','schedule_open','schedule_summary','frontlight_row_rect']:u+=function(n,src)+'\n'
def block(text,needle):
    a=text.index(needle);a=text.index('{',a)+1;b=a;depth=1
    while depth:depth+=(text[b]=='{')-(text[b]=='}');b+=1
    return text[a:b-1]
render=function('render_content',src)
for page in ['FRONTLIGHT','LIGHT_SCHEDULE']:
    u+='static void draw_'+page.lower()+'(uint8_t*fb){'+block(render,'if(s_page==SETTINGS_'+page+') {')+'}\n'
choice=block(function('settings_choice_open',src),'if(kind>=SET_PICK_LIGHT_ON_HOUR&&kind<=SET_PICK_LIGHT_LEVEL) {')
u+='static void settings_choice_open(int kind){int count=11,current=0;const char*title="";if(kind>=SET_PICK_LIGHT_ON_HOUR&&kind<=SET_PICK_LIGHT_LEVEL){'+choice+'}ui_choice_begin(&s_setting_choice,kind,title,count,current);}\n'
select=block(function('settings_choice_gesture',src),'if(s_setting_choice.kind>=SET_PICK_LIGHT_ON_HOUR&&s_setting_choice.kind<=SET_PICK_LIGHT_LEVEL) {')
u+='static int choose(int index){bool ok=false;{'+select+'}if(ok)s_setting_choice.open=false;return APP_REDRAW_PAGE;}\n'
gesture=function('on_gesture',src)
for page in ['FRONTLIGHT','LIGHT_SCHEDULE']:
    u+='static int tap_'+page.lower()+'(int x,int y){ui_gesture_event_t event={x,y};const ui_gesture_event_t*ev=&event;'+block(gesture,'if(s_page==SETTINGS_'+page+') {')+'}\n'
u+=r'''
int main(void){uint8_t fb=0;
 for(scale=100;scale<=200;scale+=10)for(wide=0;wide<2;wide++)for(tall=0;tall<2;tall++){
  for(int warm=0;warm<2;warm++){schedule_open(warm);used=0;draw_light_schedule(&fb);check_distinct();for(int i=0;i<used;i++)assert(boxes[i].y+boxes[i].height<=1088);}
  used=0;draw_frontlight(&fb);check_distinct();
 }
 s_page=SETTINGS_FRONTLIGHT;assert(tap_frontlight(100,570)==APP_REDRAW_PAGE&&s_page==SETTINGS_LIGHT_SCHEDULE&&!s_schedule_warm);
 assert(tap_light_schedule(100,280)==APP_REDRAW_PAGE&&s_schedule_draft.enabled&&!configs[0].enabled);
 tap_light_schedule(100,400);assert(s_setting_choice.kind==SET_PICK_LIGHT_ON_HOUR&&s_setting_choice.count==24);
 choose(23);assert(s_setting_choice.open&&s_setting_choice.kind==SET_PICK_LIGHT_ON_MINUTE&&s_setting_choice.count==60);choose(59);assert(!s_setting_choice.open&&s_schedule_draft.on_minute==1439);
 tap_light_schedule(100,520);choose(6);choose(0);assert(s_schedule_draft.off_minute==360);
 tap_light_schedule(100,640);assert(s_setting_choice.count==100);choose(74);assert(s_schedule_draft.level==75&&!restores&&!saves);
 tap_light_schedule(100,880);assert(s_page==SETTINGS_FRONTLIGHT&&configs[0].enabled&&configs[0].on_minute==1439&&configs[0].off_minute==360&&configs[0].level==75&&restores==1);
 tap_frontlight(100,670);assert(s_schedule_warm);tap_light_schedule(100,280);tap_light_schedule(100,980);assert(s_page==SETTINGS_FRONTLIGHT&&!configs[1].enabled); // cancel
 schedule_open(true);s_schedule_draft.on_minute=s_schedule_draft.off_minute;int before=saves;tap_light_schedule(100,880);assert(saves==before&&s_schedule_notice[0]&&s_page==SETTINGS_LIGHT_SCHEDULE);
 s_schedule_draft.on_minute=100;fail_save=1;tap_light_schedule(100,880);assert(!configs[1].enabled&&s_page==SETTINGS_LIGHT_SCHEDULE&&restores==1);
 tap_light_schedule(100,100);assert(s_page==SETTINGS_FRONTLIGHT); // Back discards draft
 puts("PASS actual schedule UI: independent channels, hour->minute pickers, levels, save/cancel/back, equal-time and storage failures; bounded text/controls at100..200%");
}
'''
with tempfile.TemporaryDirectory() as t:
    p=Path(t);(p/'test.c').write_text(u)
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-Wno-unused-function','-Wno-misleading-indentation','-fsanitize=undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
