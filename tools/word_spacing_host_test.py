"""Run production word-spaced/fitted font cursors, including Vietnamese and NBSP."""
from pathlib import Path
import subprocess,tempfile
from home_cover_cache_host_test import function
root=Path(__file__).resolve().parents[1]
u=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define EPD_DRAW_ALIGN_LEFT 0
typedef struct {int width,height,top,left,advance_x;const uint8_t *bitmap;} glyph_entry_t;
static bool font_ready=true,s_geometry_fn=true;
static struct {uint32_t cp;int x,advance;size_t byte;} records[128];static int count;
static uint32_t decode_utf8(const char **p){uint32_t c=(unsigned char)*(*p)++;if(c<128)return c;unsigned n=(c&224)==192?1:(c&240)==224?2:3;c&=(1u<<(6-n))-1;while(n--)c=(c<<6)|((unsigned char)*(*p)++&63);return c;}
static int advance(uint32_t cp){return cp==32||cp==0xa0||cp==0x202f?3:cp==9?4:cp==0x301?0:7;}
static const glyph_entry_t *get_glyph(uint32_t cp,int px){(void)px;static glyph_entry_t g;g.advance_x=advance(cp);return &g;}
static void geometry_glyph(const char *root,const char *a,const char *b,int x,int y,int px,int adv){(void)b;(void)y;(void)px;assert(count<128);records[count].x=x;records[count].advance=adv;records[count].byte=(size_t)(a-root);records[count++].cp=decode_utf8(&a);}
static void ttf_cover_lut_init(void){}
static int clamp_px(int n){return n;}
static void warm_runs(int px,const char *s){assert(px>0&&s);}
static void run_apply(size_t b){(void)b;}
static uint8_t glyph_alpha(const glyph_entry_t *g,int x,int y){(void)g;(void)x;(void)y;return 0;}
static unsigned mix_glyph_ink(const glyph_entry_t *g,unsigned a,unsigned fg,unsigned bg){(void)g;(void)a;(void)bg;return fg;}
static void epd_draw_pixel(int x,int y,unsigned ink,uint8_t *fb){(void)x;(void)y;(void)ink;(void)fb;assert(0);}
static int measure_width(int px,const char *text){(void)px;int n=0;while(*text)n+=advance(decode_utf8(&text));return n;}
static void ttf_draw_text_px(uint8_t *fb,int x,int y,int px,const char *text,int align,uint8_t fg,uint8_t bg){(void)fb;(void)align;(void)fg;(void)bg;const char *p=text;while(*p){const char *a=p;uint32_t cp=decode_utf8(&p);geometry_glyph(text,a,p,x,y,px,advance(cp));x+=advance(cp);}}
'''
for name in ['word_spacing_extra','cjk_spacing_point','ttf_draw_text_px_word_spaced','ttf_draw_text_px_word_fitted','ttf_vertical_text_height_word_px']:
 u+=function(name,root/'main/font/ttf_font.c')+'\n'
u+=r'''
static bool space(uint32_t cp){return cp==32||cp==9||cp==0xa0||cp==0x202f;}
int main(void){
 uint8_t fb=0;const char *cases[]={"cà phê Việt Nam","cafe\xcc\x81 et café","ab\xc2\xa0" "cd\xe2\x80\xaf" "ef","ab\tcd","chữliền"};
 for(unsigned t=0;t<sizeof(cases)/sizeof(cases[0]);t++){
  const char *text=cases[t];int positions[128];size_t bytes[128];
  count=0;ttf_draw_text_px_word_spaced(&fb,0,0,20,text,0,0,0,15);int n=count;
  for(int i=0;i<n;i++){positions[i]=records[i].x;bytes[i]=records[i].byte;}
  count=0;ttf_draw_text_px_word_spaced(&fb,0,0,20,text,0,5,0,15);assert(count==n);
  int gaps=0;for(int i=0;i<n;i++){assert(records[i].x==positions[i]+gaps*5&&records[i].byte==bytes[i]);if(space(records[i].cp))gaps++;}
  assert(ttf_vertical_text_height_word_px(20,text,0,5)==measure_width(20,text)+gaps*5);
  count=0;ttf_draw_text_px_word_spaced(&fb,0,0,20,text,2,3,0,15);
  gaps=0;for(int i=0;i<n;i++){assert(records[i].x==positions[i]+i*2+gaps*3);if(space(records[i].cp))gaps++;}
 }
 count=0;ttf_draw_text_px_word_spaced(&fb,0,0,20,"ab cd",0,-3,0,15);assert(records[3].x==records[2].x);
 const char *text="ab cd ef";int target=measure_width(20,text)+40;
 count=0;ttf_draw_text_px_word_fitted(&fb,0,0,20,text,0,5,target,0,15);
 assert(records[count-1].x+records[count-1].advance==target);
 for(int i=1;i<count;i++)if(!space(records[i-1].cp))assert(records[i].x-records[i-1].x==records[i-1].advance);
 count=0;ttf_draw_text_px_word_fitted(&fb,0,0,20,text,0,5,1,0,15);
 assert(records[count-1].x+records[count-1].advance==measure_width(20,text)+10);
 puts("PASS: actual font word-spacing cursor, Vietnamese/precomposed/combining/NBSP/narrow-NBSP/tab boundaries, unchanged inner-letter gaps+byte offsets, independent tracking, justified end extent and natural fallback, vertical advance");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(u)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
