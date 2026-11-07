#!/usr/bin/env python3
"""Run real shared ticket/card painters and Add Book routes; export layout previews."""
from pathlib import Path
import subprocess,tempfile
from home_cover_cache_host_test import function
ROOT=Path(__file__).resolve().parents[1]
HOME=ROOT/'main/apps/app_dashboard.c'
unit=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef struct {int x,y,width,height;} EpdRect;
typedef struct {int unused;} app_ctx_t;
typedef int app_redraw_t;
typedef int ui_icon_t;
enum EpdFontFlags {EPD_DRAW_ALIGN_LEFT,EPD_DRAW_ALIGN_CENTER,EPD_DRAW_ALIGN_RIGHT};
enum {UI_GRAY_WHITE=255,UI_GRAY_BLACK=0,UI_GRAY_LIGHT=192};
enum {UI_ICON_BOOK_OPEN,UI_ICON_FOLDER_OPEN,UI_ICON_DOWNLOAD,UI_ICON_COPY,UI_ICON_RADIO_TOWER,UI_ICON_PENCIL};
enum {APP_REDRAW_NONE,APP_REDRAW_PAGE,APP_HOME_UTIL_TRANSFER,APP_HOME_UTIL_OPDS};
#define UI_LOCK_WIDTH 684
static const char *app_settings_device_name(void){return "Huy";}
static void fit(char *text,int px,int width){(void)text;(void)px;(void)width;}
static bool labels=true,border=true;
static bool app_settings_simple_utility_labels(void){return labels;}
static bool app_settings_simple_utility_border(void){return border;}
static bool fast;
#define APP_HOME_UTIL_COUNT 12
static char s_utility_notice[96];
#define BOOK_EXCERPT_CAP 512
static char s_excerpt[BOOK_EXCERPT_CAP];
static bool s_excerpt_is_chapter;
static unsigned custom[2];
static bool app_settings_simple_ui(void){return true;}
static bool full_mode;
static bool app_settings_simple_full_ui(void){return full_mode;}
static unsigned app_settings_simple_utility(unsigned slot){return slot>=2&&slot<4?custom[slot-2]:0;}
static const char *utility_names[12]={"Để trống","Đọc tiếp","Thư viện","Duyệt OPDS","Đồng bộ KoSync","Cài đặt","Bật/tắt Wi-Fi","Bật/tắt Bluetooth","Cập nhật OTA","Tệp tin","Chuyển tệp","Làm mới màn hình"};
static const ui_icon_t utility_icons[12]={0};
static unsigned outlines,circles,rules,route_calls,last_action,title_draws;
static char title_tail[128];
static unsigned full_rows;static char full_tail[192];
static int ui_text_effective_px(int px){return px;}
static int ui_text_fixed_width_px(int px,const char *text){int width=0;for(;*text;++text)if(((unsigned char)*text&0xc0)!=0x80)width+=*text==' '?px/4:px/2;return width;}
static bool app_settings_main_fast_refresh(void){return fast;}
typedef struct {char path[288],title[128],author[128];bool has_progress,cover_fast;uint8_t *cover;unsigned cover_width,cover_height;struct{unsigned pct;}progress;}home_book_t;
static home_book_t book;
static home_book_t *featured(void){return &book;}
static void rect(const char *kind,EpdRect r,int radius,int gray){
 assert(r.width>=0&&r.height>=0&&r.x>=0&&r.y>=0&&r.x+r.width<=684&&r.y+r.height<=1216);
 printf("%s\t%d\t%d\t%d\t%d\t%d\t%d\n",kind,r.x,r.y,r.width,r.height,radius,gray);
}
static void ui_fill_round_rect(uint8_t*f,EpdRect r,int rad,int g){(void)f;rect("FILL",r,rad,g);}
static void home_frame(uint8_t*f,EpdRect r,int rad){(void)f;++outlines;rect("FRAME",r,rad,48);}
static void epd_fill_circle(int x,int y,int r,int g,uint8_t*f){(void)f;++circles;printf("CIRCLE\t%d\t%d\t%d\t%d\n",x,y,r,g);}
static void circle_frame(uint8_t*f,int x,int y,int r){(void)f;printf("RING\t%d\t%d\t%d\n",x,y,r);}
static void ui_fill_rect_fast(EpdRect r,int g,uint8_t*f){(void)f;rect("FILL",r,0,g);}
static void ui_draw_separator(uint8_t*f,int y,int x,int w,int g){(void)f;++rules;rect("FILL",(EpdRect){x,y,w,1},0,g);}
static void text(int x,int y,int px,const char *s,int align){printf("TEXT\t%d\t%d\t%d\t%d\t%s\n",x,y,px,align,s);}
static void ui_text(uint8_t*f,int x,int y,int px,const char*s,int a,bool inv){(void)f;(void)inv;text(x,y,px,s,a);}
static void label_on(uint8_t*f,int x,int y,int px,const char*s,int a,int g,int bg){(void)g;(void)bg;ui_text(f,x,y,px,s,a,false);}
static void home_bold_label(uint8_t*f,int x,int y,int px,const char*s,int a,int bg){label_on(f,x,y,px,s,a,0,bg);}
static void ui_text_caption(uint8_t*f,EpdRect r,int px,const char*s,int a,bool inv){(void)f;(void)inv;
 assert(r.x>=0&&r.x+r.width<=684&&r.y>=0&&r.y+r.height<=1096);

 printf("CAPTION\t%d\t%d\t%d\t%d\t%d\t%d\t%s\n",r.x,r.y,r.width,r.height,px,a,s);
}
static void ui_draw_icon(uint8_t*f,int x,int y,int size,int icon,int g){(void)f;(void)g;printf("ICON\t%d\t%d\t%d\t%d\n",x,y,size,icon);}
static void ui_draw_icon_normalized(uint8_t*f,int x,int y,int size,int icon,int g){ui_draw_icon(f,x,y,size,icon,g);}
static void ui_text_box(uint8_t*f,EpdRect r,int px,const char*s,int a,bool inv){
 if(r.x==60&&r.width==564){assert(a==EPD_DRAW_ALIGN_CENTER);assert(ui_text_fixed_width_px(px,s)<=564);++full_rows;snprintf(full_tail,sizeof(full_tail),"%s",s);}
 if(r.x==348&&r.width==274&&r.height==56){assert(r.y==278+(int)title_draws*56);assert(ui_text_fixed_width_px(px,s)<=274);++title_draws;snprintf(title_tail,sizeof(title_tail),"%s",s);}
 ui_text_caption(f,r,px,s,a,inv);
}
static void ui_draw_button(uint8_t*f,EpdRect r,const char*s,bool on){(void)on;home_frame(f,r,16);ui_text_caption(f,r,24,s,EPD_DRAW_ALIGN_CENTER,false);}
static void ui_nav_status(uint8_t*f){(void)f;puts("STATUS");}
static void ui_nav_draw(uint8_t*f,int active){(void)f;(void)active;puts("NAV");}
static void draw_cover(uint8_t*f,const home_book_t*b,EpdRect r){(void)b;ui_fill_round_rect(f,r,12,210);home_frame(f,r,12);}
static void draw_open_book(uint8_t*f,const home_book_t*b,EpdRect r){draw_cover(f,b,r);}
static void book_ticket_draw_shadow(uint8_t*f){(void)f;}
static int s_ticket_battery;
static int64_t ui_nav_clock_minute(void){return 13*60+30;}
static void *read_pico_pmu_get(void){return NULL;}
static int pmu_battery_percent(void *p){(void)p;return 80;}
static const char *app_settings_status_signature(void){return "dpReader";}
static void ui_text_fixed_vc(uint8_t *f,int x,int y,int px,const char *s,int a,bool inv){ui_text(f,x,y,px,s,a,inv);}
static unsigned cached_backgrounds,fallback_backgrounds;
static void book_ticket_draw_background(uint8_t *f,const uint8_t *p,unsigned w,unsigned h,bool packed){assert(p&&w&&h);(void)packed;++cached_backgrounds;ui_fill_rect_fast((EpdRect){0,0,684,1216},176,f);}
static void draw_simple_border(uint8_t *f){++fallback_backgrounds;ui_fill_rect_fast((EpdRect){0,0,684,1216},17,f);}
static void book_ticket_draw_paper(uint8_t *f){EpdRect paper={48,58,588,1100};ui_fill_rect_fast(paper,255,f);home_frame(f,paper,0);}
static void header(uint8_t*f,const char*s){ui_nav_status(f);ui_text(f,122,91,46,s,EPD_DRAW_ALIGN_LEFT,false);}
static app_redraw_t utility_run(app_ctx_t*c,unsigned action){(void)c;++route_calls;last_action=action;return APP_REDRAW_NONE;}
'''
for name in ['simple_slot_rect','utility_notice_rect','simple_resume_rect','simple_cover_rect','simple_open_rect','simple_add_rect','simple_import_rect',
             'home_tag_card','simple_utility','simple_title','full_resume_lines','simple_ticket_status_rect','draw_simple_ticket_status','render_simple_root','render_add_books','simple_add_action']:
 unit+=function(name,HOME)+'\n'
unit+=r'''
int main(void){
 uint8_t fb=0;app_ctx_t ctx={0};
 puts("PAGE\ttitle-wrap");
 assert(simple_title(&fb,"")==0);title_draws=0;
 assert(simple_title(&fb,"Sách ngắn")==1&&title_draws==1);title_draws=0;
 assert(simple_title(&fb,"Một cuốn sách có tiêu đề rất dài để kiểm tra cách xuống đủ ba dòng và dấu lược ở cuối")==3&&title_draws==3&&strstr(title_tail,"…"));title_draws=0;
 assert(simple_title(&fb,"ABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZABCDEFGHIJKLMNOPQRSTUVWXYZ")==3&&title_draws==3&&strstr(title_tail,"…"));title_draws=0;

 /* Exact same header shape at regular/expanded sizes. */
 puts("PAGE\tregular-tags");
 home_tag_card(&fb,(EpdRect){36,184,612,378},68,14,255);
 home_tag_card(&fb,(EpdRect){36,590,612,206},60,12,255);
 assert(circles==4&&rules==2&&outlines==2);
 for(unsigned state=0;state<7;++state){
  printf("PAGE\tsimple-%u\n",state);outlines=circles=rules=0;fast=state==2;
  if(state>=3){labels=((state-3)&1)!=0;border=((state-3)&2)!=0;}
  if(state){strcpy(book.path,"/sdcard/books/example.epub");strcpy(book.title,"Những ngày đọc sách bình yên");strcpy(book.author,"Nguyễn Nhật Ánh");book.has_progress=true;book.progress.pct=37;}
  if(state==2){custom[0]=5;custom[1]=6;}
  title_draws=0;render_simple_root(&fb);
  assert(circles==4&&rules==2&&route_calls==0);
  assert(outlines==(state?3:3)+(border?4:0));
 }
 full_rows=0;assert(full_resume_lines(&fb,"Một tựa sách rất dài với nhiều từ để kiểm tra việc xuống dòng cân đối và rút gọn đúng giới hạn hai dòng",766,36,2,false)==2&&full_rows==2);
 full_rows=0;assert(full_resume_lines(&fb,"“Một đoạn trích rất dài với nhiều từ để kiểm tra giới hạn dòng và dấu ngoặc kép kết thúc luôn được giữ lại khi phải rút gọn nội dung”",944,22,1,true)==1&&strstr(full_tail,"…”"));
 puts("PAGE\tfully-simple");strcpy(s_excerpt,"Những trang sách mở ra một khoảng trời bình yên cho mỗi ngày.");
 full_mode=true;title_draws=0;outlines=circles=rules=0;render_simple_root(&fb);
 assert(circles==0&&outlines==2&&simple_resume_rect().height==1100&&simple_resume_rect().y==58);
 assert(simple_cover_rect().x+simple_cover_rect().width/2==342&&simple_cover_rect().height==566);
 assert(fallback_backgrounds==1&&!cached_backgrounds);
 uint8_t cached=0;book.cover=&cached;book.cover_width=book.cover_height=1;
 render_simple_root(&fb);assert(fallback_backgrounds==1&&cached_backgrounds==1);
 full_mode=false;
 puts("PAGE\tadd-books");labels=border=false;unsigned before_menu=outlines;render_add_books(&fb);assert(!route_calls&&outlines==before_menu+2);
 for(unsigned i=0;i<2;++i){
  EpdRect r=simple_import_rect(i);unsigned before=route_calls;
  simple_add_action(&ctx,r.x-1,r.y+1);simple_add_action(&ctx,r.x+r.width,r.y+1);
  simple_add_action(&ctx,r.x,r.y-1);simple_add_action(&ctx,r.x,r.y+r.height);
  assert(route_calls==before);
  simple_add_action(&ctx,r.x,r.y);assert(route_calls==before+1&&last_action==(i?APP_HOME_UTIL_OPDS:APP_HOME_UTIL_TRANSFER));
 }
 puts("PASS\tshared ticket shape, four utilities, independent labels/borders in all combinations, safe bounds and Add Book routes; rendering starts no service");
}
'''
with tempfile.TemporaryDirectory() as tmp:
 c=Path(tmp)/'cards.c';exe=Path(tmp)/'cards';c.write_text(unit)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(c),'-o',str(exe)],check=True)
 result=subprocess.run([str(exe)],text=True,capture_output=True,check=True)
print(result.stdout.splitlines()[-1])
# Source contracts prevent an OPDS entry from falling back to a shelf scan in simple mode.
book=(ROOT/'main/apps/app_book.c').read_text()
entry=function('on_enter',ROOT/'main/apps/app_book.c')
assert '!s_requested_open[0]&&!s_requested_opds' in entry
assert 's_requested_opds=s_requested_manage=false' not in entry
assert 'if(app_settings_simple_ui()&&s_view==OPDS){s_scan_pending=false;return;}' in entry
out=ROOT/'build/simple-ui-RC6-preview';out.mkdir(parents=True,exist_ok=True)
(out/'drawing-trace.txt').write_text(result.stdout)
# Preview positions come from the actual painters above; host fonts and sample cover are illustrative.
try:
 from PIL import Image,ImageDraw,ImageFont
except ImportError:
 print('Pillow unavailable; drawing trace retained.')
 raise SystemExit(0)
fontpath='/System/Library/Fonts/Supplemental/Arial.ttf'
def font(px):return ImageFont.truetype(fontpath,px)
def draw_text(x,y,px,align,s):
 d.text((x,y),s,font=font(px),fill=0,anchor={0:'lt',1:'mt',2:'rt'}[align])
def caption(v):
 x,y,w,h,px,align=map(int,v[:6]);s=v[6];words=s.split();lines=[];line=''
 for word in words:
  trial=(line+' '+word).strip()
  if line and d.textlength(trial,font=font(px))>w:lines.append(line);line=word
  else:line=trial
 if line:lines.append(line)
 maxlines=max(1,h//(px+8));lines=lines[:maxlines]
 for i,line in enumerate(lines):draw_text(x+(w//2 if align==1 else w if align==2 else 0),y+i*(px+8),px,align,line)
name=None;im=None
for line in result.stdout.splitlines():
 parts=line.split('\t');kind=parts[0];v=parts[1:]
 if kind=='PAGE':
  if im:im.save(out/(name+'.png'))
  name=v[0];im=Image.new('RGB',(684,1216),'white');d=ImageDraw.Draw(im)
 elif kind in ('FILL','FRAME'):
  x,y,w,h,rad,g=map(int,v);box=(x,y,x+w-1,y+h-1)
  d.rounded_rectangle(box,radius=rad,fill=(g,g,g) if kind=='FILL' else None,outline=(48,48,48) if kind=='FRAME' else None,width=3)
 elif kind in ('CIRCLE','RING'):
  x,y,r=map(int,v[:3]);d.ellipse((x-r,y-r,x+r,y+r),fill='white' if kind=='CIRCLE' else None,outline=(48,48,48) if kind=='RING' else None,width=3)
 elif kind=='TEXT':draw_text(*map(int,v[:4]),v[4])
 elif kind=='CAPTION':caption(v)
 elif kind=='ICON':
  x,y,size,icon=map(int,v);d.rounded_rectangle((x-size//2,y-size//2,x+size//2,y+size//2),radius=4,outline='black',width=2)
  if icon in (0,1):d.line((x,y-size//2,x,y+size//2),fill='black',width=2)
  else:d.line((x-size//4,y,x+size//4,y),fill='black',width=2);d.line((x,y-size//4,x,y+size//4),fill='black',width=2)
 elif kind=='STATUS':draw_text(36,22,30,0,'20:30');draw_text(648,22,25,2,'80%')
 elif kind=='NAV':d.rectangle((0,1096,684,1216),fill=(224,224,224));draw_text(171,1158,20,1,'Đọc tiếp');draw_text(513,1158,20,1,'Cài đặt')
if im:im.save(out/(name+'.png'))
print('Layout previews:',out)
