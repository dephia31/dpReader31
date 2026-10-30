#!/usr/bin/env python3
"""Use actual bounded text drawing for Files cards and nested key/hold screens."""
from pathlib import Path
import subprocess,tempfile,runpy,re
from home_cover_cache_host_test import function
root=Path(__file__).resolve().parents[1]
fixture=runpy.run_path(str(root/'tools/settings_large_text_host_test.py'))
unit=fixture['unit'].split('int main(void)')[0]
src=root/'main/apps/app_device_settings.c';source=src.read_text()
unit+=r'''
#define APP_AI_ACTION_COUNT 10
#define APP_READER_KEY_COUNT 11
#define APP_READER_KEY_FULLSCREEN 3
#define APP_CONTROL_HOLD_DEFAULT 255
#define UI_ICON_CHEVRON_DOWN 2
static int s_control_key=-1,s_control_pick=-1;
static const char *const controls_key_names[]={"Nút AI","Âm lượng +","Âm lượng −","Nút nguồn","Phím giữa","Phím trái","Phím phải"};
static void ui_fill_round_rect(uint8_t*f,EpdRect r,int radius,int shade){}
static void epd_fill_rect(EpdRect r,int shade,uint8_t*f){}
static void ui_draw_control_frame(uint8_t*f,EpdRect r,int radius,int shade){}
static void epd_draw_circle(int x,int y,int radius,int shade,uint8_t*f){}
static const char *controls_choice_label(int target,int index){return "Đồng bộ KOSync / Chương trước";}
static int controls_choice_current(int target){return 6;}
static const char *ai_action_label(int action){return "Về Trang chủ";}
static int app_settings_ai_short(void){return 3;}
static const char *reader_key_action_label(int action){return "Đồng bộ KOSync";}
static bool app_settings_reader_power_turn(void){return true;}
static uint8_t app_settings_control_hold(int key){return APP_CONTROL_HOLD_DEFAULT;}
static bool app_settings_reader_swipe_up_tools(void){return true;}
static bool app_settings_reader_swipe_bottom_home(void){return true;}
'''
for name in ('controls_choice_count','controls_choice_action','controls_choice_rect','controls_row','draw_controls_choices'):
 unit+=function(name,src)+'\n'
a=source.index('    if(s_page==SETTINGS_CONTROLS) {',source.index('static void render_content'))
a=source.index('{',a)+1;b=a;depth=1
while depth:depth+=(source[b]=='{')-(source[b]=='}');b+=1
unit+='static void controls_screen(uint8_t*fb){'+source[a:b-1]+'}\n'
files=root/'main/apps/app_files.c';text=files.read_text()
unit+=function('home_transfer_rect',files)+'\n'
a=text.index('        static const char *const methods[]');b=text.index('        char summary[48]',a)
unit+='static void files_cards(uint8_t*fb){'+text[a:b]+'}\n'
transfer=root/'main/apps/app_transfer.c';text=transfer.read_text()
unit+=r"""
static bool s_qr_url;
static void ui_draw_round_rect(uint8_t*f,EpdRect r,int radius,int shade){}
static int transfer_footer_top(void){return 1000;}
"""
unit+=function('control_rect',transfer)+'\n'
a=text.index('        for (int i = 0; i < 2; ++i) {',text.index('    if (s_mode == READ_PICO_TRANSFER_MODE_AP) {',text.index('static void render_content')))
b=text.index('    } else {',a)
unit+='static void hotspot_tabs(uint8_t*fb){'+text[a:b]+'}\n'

unit+=r'''
int main(void){uint8_t fb=0;
 for(scale=100;scale<=200;scale+=10)for(wide=0;wide<2;wide++)for(tall=0;tall<2;tall++){
  used=0;files_cards(&fb);assert(used==6);check_distinct();
  for(int i=0;i<6;++i){EpdRect card=home_transfer_rect(i/2);assert(boxes[i].x>card.x&&boxes[i].x+boxes[i].width<card.x+card.width);}
  for(int tab=0;tab<2;++tab){s_qr_url=tab;used=0;hotspot_tabs(&fb);assert(used==2);check_distinct();}
  s_control_pick=-1;
  for(s_control_key=-1;s_control_key<7;++s_control_key){used=0;controls_screen(&fb);check_distinct();}
  for(s_control_pick=0;s_control_pick<16;++s_control_pick){used=0;draw_controls_choices(&fb);check_distinct();
   for(int i=0;i<used;++i)assert(boxes[i].y+boxes[i].height<=1088);}
 }
 puts("PASS actual Files cards and key/short/hold/action screens:11 scales,wide/tall glyphs,no overlapping text or card overflow");}
'''
with tempfile.TemporaryDirectory() as directory:
 p=Path(directory);(p/'test.c').write_text(unit)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-Wno-unused-function','-Wno-misleading-indentation',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
