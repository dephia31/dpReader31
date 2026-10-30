#!/usr/bin/env python3
"""Actual S31 fast-turn preparation must preserve AA on every subsequent page."""
from pathlib import Path
import subprocess,tempfile
from home_cover_cache_host_test import function
root=Path(__file__).resolve().parents[1]
source=root/'boards/metalio47/src/main/display_reader_waveform.c'
unit=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
typedef struct {int x,y,width,height;} EpdRect;
typedef struct {uint8_t *front_fb,*back_fb;} EpdiyHighlevelState;
enum EpdRotation {EPD_ROT_LANDSCAPE,EPD_ROT_PORTRAIT,EPD_ROT_INVERTED_LANDSCAPE,EPD_ROT_INVERTED_PORTRAIT};
static enum EpdRotation rotation;
static bool aa=true;
static int wave;
static int epd_width(void){return 64;}
static int epd_height(void){return 32;}
static enum EpdRotation epd_get_rotation(void){return rotation;}
static bool app_settings_book_antialias(void){return aa;}
static const int *display_reader_fast_text_waveform(void){return &wave;}
'''
for name in ('prepare_ui_byte','prepare_target','display_reader_fast_text_prepare'):
 unit+=function(name,source)+'\n'
unit+=r'''
int main(void){
 uint8_t front[1024],back[1024],original[1024],history[1024];
 EpdiyHighlevelState hl={front,back};
 for(int rot=0;rot<4;++rot){
  rotation=rot;EpdRect area={0,0,(rot&1)?32:64,(rot&1)?64:32};
  for(int page=0;page<12;++page){
   aa=page!=5&&page!=6;
   for(unsigned i=0;i<sizeof(front);++i){front[i]=original[i]=(uint8_t)(i*29+page*17);back[i]=history[i]=(uint8_t)(i*13+page);}
   bool binary=display_reader_fast_text_prepare(&hl,area);
   assert(binary==!aa);
   assert(!memcmp(back,history,sizeof(back)));
   for(unsigned i=0;i<sizeof(front);++i){
    uint8_t expected=aa?original[i]:(uint8_t)(((original[i]&8)?15:0)|((original[i]&128)?240:0));
    assert(front[i]==expected);
   }
  }
 }
 aa=true;assert(!display_reader_fast_text_prepare(NULL,(EpdRect){0,0,1,1}));
 puts("PASS actual S31 turn preparation: 12 pages x4 rotations, AA on/off/on, all16 target grays, unchanged history, no AA quantization");
}
'''
with tempfile.TemporaryDirectory() as directory:
 p=Path(directory);(p/'test.c').write_text(unit)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
