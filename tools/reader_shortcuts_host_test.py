"""中文：真实快捷菜单的选项、保存、失败和布局。/ English: Actual shortcut menu choices, persistence routing, failures and bounds."""
from pathlib import Path
import re,subprocess,tempfile
from home_cover_cache_host_test import function
root=Path(__file__).resolve().parents[1];file=root/'main/apps/app_device_settings.c'
header=(root/'main/settings.h').read_text()
enums='\n'.join(re.search(r'typedef enum \{\s*'+first+r',[\s\S]*?\} '+name+r';',header)[0] for first,name in [('APP_AI_REFRESH','app_ai_action_t'),('APP_READER_KEY_PREV','app_reader_key_action_t')])
unit=r'''
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#define ESP_OK 0
typedef struct {int x,y,width,height;} EpdRect;
'''+enums+r'''
static int ai,volume[2],keys[3];
static bool power,up,bottom,fail;
static int app_settings_ai_short(void){return ai;}
static int app_settings_set_ai_short(int n){if(fail)return -1;ai=n;return 0;}
static int app_settings_volume_key_action(int k){return volume[k];}
static bool app_settings_set_volume_key_action(int k,int n){if(fail)return false;volume[k]=n;return true;}
static int app_settings_reader_key_action(int k){return keys[k];}
static bool app_settings_set_reader_key_action(int k,int n){if(fail)return false;keys[k]=n;return true;}
static bool app_settings_reader_power_turn(void){return power;}
static void app_settings_set_reader_power_turn(bool on){power=on;}
static bool app_settings_reader_swipe_up_tools(void){return up;}
static void app_settings_set_reader_swipe_up_tools(bool on){up=on;}
static bool app_settings_reader_swipe_bottom_home(void){return bottom;}
static void app_settings_set_reader_swipe_bottom_home(bool on){bottom=on;}
'''
unit+=r'''
#define UI_GRAY_WHITE 255
#define UI_ICON_CHEVRON_DOWN 1
enum EpdFontFlags {EPD_DRAW_ALIGN_LEFT,EPD_DRAW_ALIGN_CENTER,EPD_DRAW_ALIGN_RIGHT};
static EpdRect text_boxes[2],choice_box;
static int box_count;
static int test_percent=100;
static bool s_system_ttf;
static EpdRect fitting_box;
static bool ttf_font_is_builtin(void){return false;}
static bool ui_font_has_text(const char *s){(void)s;return true;}
static int ui_text_effective_px(int px){return px*test_percent/100;}
static void ui_font_measure_line_px(int px,const char *s,int *above,int *below){(void)s;*above=(px*4+4)/5;*below=px/4;}
static void ttf_measure_line_px(int px,const char *s,int *above,int *below){ui_font_measure_line_px(px,s,above,below);}
static int ui_text_fixed_width_px(int px,const char *s){return (int)strlen(s)*px/2;}
static void ui_text_fixed_vc(uint8_t *fb,int x,int cy,int px,const char *s,enum EpdFontFlags align,bool invert){
 (void)fb;(void)invert;int above,below;ui_font_measure_line_px(px,s,&above,&below);
 assert(align==EPD_DRAW_ALIGN_LEFT&&x==fitting_box.x);
 assert(cy-(above+below)/2>=fitting_box.y&&cy+(above+below+1)/2<=fitting_box.y+fitting_box.height);
 assert(ui_text_fixed_width_px(px,s)<=fitting_box.width);
}
'''
unit+=function('ui_text_box',root/'main/ui/ui_kit.c').replace('void ui_text_box(', 'static void fit_text_box(')+'\n'
unit+=r'''
static void ui_text_box(void *fb,EpdRect r,int px,const char *text,int align,bool invert){
 assert(box_count<2);text_boxes[box_count++]=r;fitting_box=r;
 fit_text_box(fb,r,px,text,align,invert);
}
static void ui_fill_round_rect(void *fb,EpdRect r,int radius,int gray){(void)fb;(void)radius;(void)gray;choice_box=r;}
static void ui_draw_control_frame(void *fb,EpdRect r,int radius,int gray){(void)fb;(void)r;(void)radius;(void)gray;}
static void ui_draw_icon(void *fb,int x,int y,int px,int icon,int gray){
 (void)fb;(void)icon;(void)gray;assert(x-px/2>=choice_box.x&&x+px/2<=choice_box.x+choice_box.width);
 assert(y-px/2>=choice_box.y&&y+px/2<=choice_box.y+choice_box.height);
}
'''
for name in ('ai_action_label','reader_key_action_label','controls_choice_count','controls_choice_action','controls_choice_current','controls_choice_label','controls_choice_save','controls_choice_rect','controls_row'):
 unit+=function(name,file)+'\n'
unit+=r'''
int main(void){
 const int rows[]={230,310,390,470,550,630,710,835,915};
 for(test_percent=100;test_percent<=200;test_percent+=10)for(int face=0;face<2;++face)for(unsigned i=0;i<sizeof(rows)/sizeof(rows[0]);++i){
  s_system_ttf=face;uint8_t fb=0;box_count=0;controls_row(&fb,rows[i],"Nút Âm lượng (+) bên hông","Đồng bộ KOSync");
  assert(box_count==2&&text_boxes[0].y+text_boxes[0].height<=choice_box.y);
  assert(text_boxes[1].y>=choice_box.y&&text_boxes[1].y+text_boxes[1].height<=choice_box.y+choice_box.height);
  assert(text_boxes[1].x+text_boxes[1].width<=choice_box.x+choice_box.width-32);
  assert(choice_box.y+choice_box.height<=rows[i]+80);
 }

 for(int target=0;target<=8;++target){
  int count=controls_choice_count(target);assert(count>=2&&count<=10);
  for(int i=0;i<count;++i){
   EpdRect r=controls_choice_rect(i);assert(r.y>=268&&r.y+r.height<944);
   assert(controls_choice_label(target,i)[0]);assert(controls_choice_save(target,i));
   int expected=target==0||target==3||target>=7?i:controls_choice_action(i);
   assert(controls_choice_current(target)==expected);
   if(target!=0&&target!=3&&target<7)assert(expected!=APP_READER_KEY_FULLSCREEN);
  }
  assert(!controls_choice_save(target,-1)&&!controls_choice_save(target,count));
 }
 fail=true;int previous=volume[1];assert(!controls_choice_save(1,0)&&volume[1]==previous);
 assert(!controls_choice_save(-1,0)&&!controls_choice_save(9,0));
 bool light=false,home=false;
 for(int i=0;i<controls_choice_count(1);++i){light|=controls_choice_action(i)==APP_READER_KEY_LIGHT;home|=controls_choice_action(i)==APP_READER_KEY_HOME;}
 assert(light&&home);
 puts("PASS: actual menu exposes Home/light and all allowed shortcuts, saves each target, rejects invalid/retired choices and preserves failed settings");
}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp)/'test.c';p.write_text(unit);binary=p.with_suffix('')
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror',str(p),'-o',str(binary)],check=True)
 subprocess.run([str(binary)],check=True)
