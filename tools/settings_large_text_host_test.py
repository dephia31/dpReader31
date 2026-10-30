"""中文：100–200%及宽高字形验证真实设置布局和文字框。/ English: Verify actual settings layout and text fitting at100–200% with wide/tall glyphs."""
from pathlib import Path
import subprocess,tempfile
from home_cover_cache_host_test import function
from board_source_view import select_source
root=Path(__file__).resolve().parents[1];src=root/'main/apps/app_device_settings.c';kit=root/'main/ui/ui_kit.c'
unit=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef struct{int x,y,width,height;}EpdRect;
enum EpdFontFlags{EPD_DRAW_ALIGN_LEFT,EPD_DRAW_ALIGN_CENTER,EPD_DRAW_ALIGN_RIGHT};
enum{SETTINGS_MAIN,SETTINGS_TEXT_EDIT,SETTINGS_SYSTEM_FONT};
#define UI_GRAY_BLACK 0
#define UI_GRAY_WHITE 255
#define SETTINGS_UNLOCK_ICON 18
#define UI_ICON_CHEVRON_RIGHT 1
static int scale=100,wide,tall,s_page,s_main_scroll,s_settings_group;
static bool s_system_ttf;
static char s_notice[96];
static unsigned app_settings_system_font_size(void){return scale;}
static unsigned app_settings_wake_presses(void){return 2;}
static int ui_text_effective_px(int px){return px*scale/100;}
static bool ttf_font_is_builtin(void){return !s_system_ttf;}
static bool ui_font_has_text(const char*t){return true;}
static void metrics(int px,const char*t,int*a,int*b){*a=px*(tall?10:8)/10;*b=px*(tall?4:2)/10;}
#define ui_font_measure_line_px metrics
#define ttf_measure_line_px metrics
static int ui_text_fixed_width_px(int px,const char*t){int n=0;for(;*t;t++)if(((unsigned char)*t&0xc0)!=0x80)n++;return n*px*(wide?9:6)/10;}
static EpdRect boxes[128],expected;static int used,painted;
static void ui_text_fixed_vc(uint8_t*f,int x,int cy,int px,const char*t,enum EpdFontFlags align,bool inv){
 int a,b;metrics(px,t,&a,&b);int w=ui_text_fixed_width_px(px,t);int left=x-(align==EPD_DRAW_ALIGN_CENTER?w/2:align==EPD_DRAW_ALIGN_RIGHT?w:0);int baseline=cy+(a-b)/2;
 assert(left>=expected.x&&left+w<=expected.x+expected.width);
 assert(baseline-a>=expected.y&&baseline+b<=expected.y+expected.height);painted++;
}
'''
unit+=function('ui_text_box',kit).replace('void ui_text_box(', 'static void actual_text_box(')+'\n'
unit+=r'''
static void ui_text_box(uint8_t*f,EpdRect r,int px,const char*t,enum EpdFontFlags align,bool inv){
 if(!t||!*t)return;assert(r.width>0&&r.height>0);assert(used<128);boxes[used++]=r;expected=r;actual_text_box(f,r,px,t,align,inv);
}
static EpdRect ui_inset_rect(EpdRect r,int n){r.x+=n;r.y+=n;r.width-=2*n;r.height-=2*n;return r;}
static void ui_draw_choice_round_rect(uint8_t*f,EpdRect r,int radius,bool on){}
#define UI_BTN_RADIUS 12
#define UI_PX_BTN 24
static int settings_viewport_end(void){return 1088;}
static void settings_card(uint8_t*f,EpdRect r,int rad,int fill,int edge){}
static void settings_divider(uint8_t*f,int y,int x,int width){}
static void setting_icon(uint8_t*f,int i,int x,int y){}
static void settings_switch(uint8_t*f,int y,bool on){}
static void ui_draw_icon(uint8_t*f,int x,int y,int size,int icon,int gray){}
static void ui_nav_back(uint8_t*f,int x,int y){}
static bool overlap(EpdRect a,EpdRect b){return a.x<b.x+b.width&&b.x<a.x+a.width&&a.y<b.y+b.height&&b.y<a.y+a.height;}
static void check_distinct(void){for(int i=0;i<used;i++)for(int j=0;j<i;j++)assert(!overlap(boxes[i],boxes[j]));}
'''
for name in ['settings_stacked','settings_detail_row_height','settings_main_row_height','settings_line','settings_center_line','back_header','row','pin_grace_row','section','setting_group','setting_toggle']:
 if name=='setting_group':unit+='#define SETTINGS_ROW_H settings_main_row_height()\n'
 unit+=select_source(function(name,src),'metalio47')+'\n'
unit+=function('ui_draw_button',kit)+'\n'
# Execute the root parent cards from the maintained renderer as well as child rows.
source_text=src.read_text()
start=source_text.index('    if (s_settings_group < 0) {',source_text.index('static void render_content('))
at=source_text.index('{',start)+1;depth=1;end=at
while depth:
 depth+=(source_text[end]=='{')-(source_text[end]=='}');end+=1
unit+=function('settings_profile_height',src)+'\n'
unit+='#define SETTINGS_PARENT_Y (180+settings_profile_height()+28)\n#define SETTINGS_PARENT_ROW_H 136\n'
import re
unit+=re.search(r'static const char \*const settings_group_titles\[\] = \{[\s\S]*?\};',source_text)[0]+'\n'
unit+='static void parent_cards(uint8_t*fb){'+source_text[start:end]+'}\n'

unit+=r'''
static char s_kosync_notice[96],s_opds_notice[96];
static const char*app_settings_kosync_url(void){return "https://sync.example.test/path";}
static const char*app_settings_kosync_user(void){return "Tài khoản rất dài để kiểm tra bố cục";}
static const char*app_settings_kosync_key(void){return "key";}
static bool app_settings_kosync_auto(void){return true;}
static const char*app_settings_opds_url(void){return "https://opds.example.test/path";}
static const char*app_settings_opds_user(void){return "Tài khoản rất dài để kiểm tra bố cục";}
static const char*app_settings_opds_pass(void){return "key";}
static void epd_fill_circle(int x,int y,int r,int c,uint8_t*f){}
static void ui_hairline(uint8_t*f,int y,int x,int w,int gray){}
#define UI_GRAY_LIGHT 200
static void settings_navigation(uint8_t*f){}
static void ui_nav_status(uint8_t*f){}
'''
render=function('render_content',src)
for page in ['KOSYNC','OPDS']:
 start=render.index('    if (s_page == SETTINGS_'+page+') {');at=render.index('{',start)+1;depth=1;end=at
 while depth:
  depth+=(render[end]=='{')-(render[end]=='}');end+=1
 body=render[at:end-1]
 unit+='static void draw_'+page.lower()+'(uint8_t*fb){'+body+'}\n'
unit+=function('transfer_header',root/'main/apps/app_transfer.c')+'\n'

unit+=r'''
int main(void){uint8_t fb=0;
 const char*labels[]={"Điều khiển lật trang BLE","Tự tắt Bluetooth khi rảnh","Chế độ tắt nguồn / ngủ"};
 const char*values[]={"Đã bật","Tắt (Không tự tắt)","Ngủ nhẹ rồi tắt nguồn"};int icons[]={1,2,3};
 for(scale=100;scale<=200;scale+=10)for(wide=0;wide<2;wide++)for(tall=0;tall<2;tall++)for(int face=0;face<2;face++){
  s_system_ttf=face;
  for(s_page=0;s_page<3;s_page++){
   used=0;back_header(&fb,"Sao lưu & Khôi phục cấu hình");assert(used==1);assert(boxes[0].x>=108&&boxes[0].y>=70&&boxes[0].y+boxes[0].height<=158);
   if(s_page==SETTINGS_SYSTEM_FONT)assert(boxes[0].x+boxes[0].width<488);
   if(s_page==SETTINGS_TEXT_EDIT)assert(boxes[0].x+boxes[0].width<542);
  }
  s_settings_group=-1;used=0;parent_cards(&fb);assert(used==10);check_distinct();
  for(int i=0;i<used;++i)assert(boxes[i].y>=SETTINGS_PARENT_Y&&boxes[i].y+boxes[i].height<=1088);
  s_settings_group=1;used=0;setting_group(&fb,0,200,"Kết nối",260,icons,labels,values,3);assert(used==0);
  s_settings_group=0;used=0;setting_group(&fb,0,200,"Kết nối",260,icons,labels,values,3);check_distinct();
  assert(settings_main_row_height()==(scale>=150?128:82));
  for(int i=1;i<used;i++)assert(boxes[i].x>=100&&boxes[i].x+boxes[i].width<=598);
  used=0;row(&fb,296,"Kết nối WiFi, truyền sách & cấu hình KOSync/OPDS","Mở Truyền dữ liệu  ›");row(&fb,408,"Chỉnh giờ thủ công","Chỉnh sửa  ›");check_distinct();
  used=0;row(&fb,1010,"Hình nền hiện tại","ten-anh-rat-dai.png");for(int i=0;i<used;i++)assert(boxes[i].y+boxes[i].height<1088);
  if(scale>=150)assert(pin_grace_row().y>=512+settings_detail_row_height());
  used=0;setting_toggle(&fb,300,"Bật Bluetooth","Bật để kết nối điều khiển lật trang BLE; tắt sẽ giải phóng RAM",true);check_distinct();
  EpdRect toggle={555,343,69,39};for(int i=0;i<used;i++)assert(!overlap(boxes[i],toggle));
  used=0;ui_draw_button(&fb,(EpdRect){36,577,612,72},"Kiểm tra kết nối máy chủ KOSync",false);assert(boxes[0].x==48&&boxes[0].width==588&&boxes[0].height==48);
  used=0;draw_kosync(&fb);check_distinct();used=0;draw_opds(&fb);check_distinct();
  used=0;transfer_header(&fb,"Truyền WiFi / Điểm phát","Mật khẩu chỉ được lưu cục bộ trên máy");check_distinct();assert(boxes[0].x>=108);
  used=0;settings_line(&fb,48,770,21,"Tự động ngắt kết nối và tắt Wi-Fi khi rảnh rỗi",EPD_DRAW_ALIGN_LEFT,false);settings_line(&fb,48,804,21,"để tiết kiệm pin và giải phóng RAM hệ thống.",EPD_DRAW_ALIGN_LEFT,false);check_distinct();
 }
 assert(painted>1000);puts("PASS: actual header/main rows/toggles/detail rows/buttons/captions fit at11 scales with8 glyph-face cases; Back/right actions and neighboring controls remain clear");
}
'''
text=src.read_text();assert 'ui_text(fb,' not in text and 'ui_text_vc(fb,' not in text
assert '408+settings_detail_row_height()' in text and '512,612,settings_detail_row_height()' in text
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(unit)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-Wno-unused-function','-Wno-misleading-indentation',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
