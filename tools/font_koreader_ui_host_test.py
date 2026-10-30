#!/usr/bin/env python3
"""Run actual S31 UI glyph/cache output with the firmware FreeType subset."""
from pathlib import Path
import subprocess, tempfile
from font_mono_host_test import FLAGS, SOURCES, COMP, ROOT
UNIT = r'''#include "ui_font.c"
#include <assert.h>
#include <stdio.h>
static uint8_t panel[684*1216];
static bool deny;
void *heap_caps_malloc(size_t n,int caps){(void)caps;return deny?NULL:malloc(n);}
void *heap_caps_calloc(size_t n,size_t size,int caps){(void)caps;return deny?NULL:calloc(n,size);}
void epd_draw_pixel(int x,int y,uint8_t color,uint8_t *fb){assert(x>=0&&x<684&&y>=0&&y<1216);fb[y*684+x]=color>>4;}
int main(void){
 assert(ready());for(int a=0;a<256;++a)assert(s_cover[a]==a);
 unsigned char sample[]={ 'A','g',0 }; (void)sample;
 unsigned cps[]={'A','g',0x110,0x1ebf,0x1ec7,0x1eaf,0x1ed9,0x1ef9};
 for(int px=18;px<=72;++px)for(unsigned i=0;i<sizeof(cps)/sizeof(*cps);++i){
  ui_glyph_t *g=glyph(cps[i],px);assert(g&&g->bitmap&&s_hint_face);
  ttf_mono_bitmap_t ref;assert(ttf_gray_render_stroke(s_hint_face,cps[i],scale_for(px),-1,0,&ref));
  assert(g->x0==ref.left&&g->y0==-ref.top&&g->width==ref.width&&g->height==ref.height);
  for(int y=0;y<ref.height;++y)assert(!memcmp(g->bitmap+y*g->width,ref.pixels+y*ref.pitch,g->width));
  int advance;stbtt_GetCodepointHMetrics(&s_font,cps[i],&advance,NULL);
  assert(g->advance==(int)lroundf(advance*scale_for(px)));
 }
 ui_glyph_t *g=glyph('A',30);assert(g);size_t bytes=s_bytes,engine=ttf_mono_bytes();
 assert(glyph('A',30)==g&&s_bytes==bytes&&ttf_mono_bytes()==engine);
 memset(panel,15,sizeof(panel));ui_font_draw_text_px(panel,20,100,30,"Trái tim vẫn đang đập.",0,0,15,false);
 unsigned used=0,tones=0;for(unsigned i=0;i<sizeof(panel);++i){assert(panel[i]<=15);used+=panel[i]!=15;tones|=1u<<panel[i];}
 assert(used>100&&__builtin_popcount(tones)>4);
 // All rows outside the glyph band remain exact white.
 for(int y=0;y<1216;++y)if(y<60||y>105)for(int x=0;x<684;++x)assert(panel[y*684+x]==15);
 memset(panel,15,sizeof(panel));ui_font_draw_text_px(panel,20,100,30,"Trái tim",0,0,15,true);
 for(unsigned i=0;i<sizeof(panel);++i)assert(panel[i]==0||panel[i]==15);
 deny=true;g=glyph('Z',71);assert(g&&!g->bitmap);deny=false;
 for(int i=0;i<UI_GLYPH_SLOTS;++i)evict(&s_glyphs[i]);assert(!s_bytes);
 ttf_mono_close(s_hint_face);s_hint_face=NULL;free(s_glyphs);s_glyphs=NULL;assert(!ttf_mono_bytes());
 puts("PASS actual S31 UI: Vietnamese18..72px light auto-hint, stable advances, continuous coverage, white background, BW, cache, OOM, cleanup");
}
'''
with tempfile.TemporaryDirectory() as directory:
 p=Path(directory)
 (p/'esp_heap_caps.h').write_text("#pragma once\n#include <stddef.h>\n#define MALLOC_CAP_SPIRAM 1\n#define MALLOC_CAP_8BIT 2\nvoid *heap_caps_malloc(size_t,int);\nvoid *heap_caps_calloc(size_t,size_t,int);\n")
 raw=(ROOT/'main/assets/builtin.ttf').read_bytes()
 assets='#include <stdint.h>\nconst uint8_t builtin[] __asm__("_binary_builtin_ttf_start")={'+','.join(map(str,raw))+'};\n'
 assets+='__asm__(".globl _binary_builtin_ttf_end\\n.set _binary_builtin_ttf_end, _binary_builtin_ttf_start + '+str(len(raw))+'");\n'
 (p/'assets.c').write_text(assets);(p/'test.c').write_text(UNIT)
 subprocess.run(['cc',*FLAGS,'-std=gnu11','-DKIIKO_BOARD_METALIO47=1','-I'+str(p),'-I'+str(ROOT/'tools/ui_gesture_stubs'),str(p/'test.c'),str(p/'assets.c'),str(ROOT/'main/font/ui_hanzi.c'),str(ROOT/'main/font/ttf_mono.c'),*[str(COMP/'vendor/src'/s) for s in SOURCES],'-lm','-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
