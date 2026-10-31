"""Run actual paged dialog geometry, input cancellation, selected value and drawing bounds."""
from pathlib import Path
import re,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'main/ui/ui_choice.c').read_text();s=re.sub(r'^#include "[^"\n]+"\n','',s,flags=re.M)
h=(root/'main/ui/ui_choice.h').read_text();h=re.sub(r'^#(?:include|pragma).*\n','',h,flags=re.M)
prefix=r'''
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef struct {int x,y,width,height;} EpdRect;
#define UI_GRAY_WHITE 255
#define UI_GRAY_BLACK 0
#define EPD_DRAW_ALIGN_LEFT 0
#define EPD_DRAW_ALIGN_CENTER 1
static int w=684,h=1216,labels,selected;
static int epd_rotated_display_width(void){return w;}
static int epd_rotated_display_height(void){return h;}
static bool ui_rect_hit(EpdRect r,int x,int y){return x>=r.x&&x<r.x+r.width&&y>=r.y&&y<r.y+r.height;}
static void bounds(EpdRect r){assert(r.width>0&&r.height>0&&r.x>=0&&r.y>=0&&r.x+r.width<=w&&r.y+r.height<=h);}
static void ui_clear_page(uint8_t *p){assert(p);}
static void ui_text_set_system_scale(bool on){assert(on);}
static void ui_fill_round_rect(uint8_t *p,EpdRect r,int radius,int ink){(void)p;(void)radius;(void)ink;bounds(r);}
static void ui_draw_control_frame(uint8_t *p,EpdRect r,int radius,int ink){(void)p;(void)radius;(void)ink;bounds(r);}
static EpdRect ui_inset_rect(EpdRect r,int n){return (EpdRect){r.x+n,r.y+n,r.width-2*n,r.height-2*n};}
static void ui_text_box(uint8_t *p,EpdRect r,int px,const char *text,int align,bool inverse){(void)p;(void)px;(void)align;assert(text);bounds(r);if(inverse)++selected;}
'''
tests=r'''
static void label(int kind,int index,char *out,size_t size){assert(kind==7&&index>=0&&index<201);++labels;snprintf(out,size,"%d",index);}
static bool overlap(EpdRect a,EpdRect b){return a.x<b.x+b.width&&b.x<a.x+a.width&&a.y<b.y+b.height&&b.y<a.y+a.height;}
int main(void){
 const int sizes[][2]={{684,1216},{1216,684},{320,480}};uint8_t fb=0;
 for(unsigned mode=0;mode<3;mode++){
  w=sizes[mode][0];h=sizes[mode][1];ui_choice_t c;
  for(int n=1;n<=201;n+=10){
   ui_choice_begin(&c,7,"Chọn",n,n-1);assert(c.open&&c.current==n-1&&c.start<=c.current&&c.current<c.start+ui_choice_slots());
   labels=selected=0;ui_choice_draw(&c,&fb,label);assert(labels>0&&labels<=ui_choice_slots()&&selected==1);
   EpdRect r=ui_choice_item(c.current-c.start);assert(ui_choice_tap(&c,r.x+r.width/2,r.y+r.height/2)==n-1&&c.open);
   for(int i=0;i<ui_choice_slots();i++){bounds(ui_choice_item(i));assert(!overlap(ui_choice_item(i),choice_page_button(0)));assert(!overlap(ui_choice_item(i),(EpdRect){ui_choice_box().x+20,ui_choice_box().y+ui_choice_box().height-122,ui_choice_box().width-40,30}));}
   snprintf(c.error,sizeof(c.error),"Lỗi ghi");ui_choice_draw(&c,&fb,label);
   assert(ui_choice_tap(&c,0,0)==UI_CHOICE_CANCEL&&!c.open);
   ui_choice_begin(&c,7,"Chọn",n,999);assert(c.open&&c.current==-1&&c.start==0);
   int last=0;while(ui_choice_page(&c,1)){assert(c.start>last);last=c.start;}assert(c.start<n);
   while(ui_choice_page(&c,-1)){}assert(c.start==0);
   EpdRect close=choice_close();assert(ui_choice_tap(&c,close.x+1,close.y+1)==UI_CHOICE_CANCEL&&!c.open);
  }
 }
 w=h=160;ui_choice_t c;ui_choice_begin(&c,7,"Chọn",10,0);assert(!c.open);
 puts("PASS: actual choice dialog, portrait/landscape/small-screen bounds, up to201 options, current selection, pages, outside/close cancel, invalid selection, error/footer do not overlap options");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(prefix+h+s+tests)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
