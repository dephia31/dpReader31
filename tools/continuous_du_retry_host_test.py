#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
"""中文：实际连续 DU 的失败重试与区域计数。/ English: Actual continuous-DU failure retry and region counters."""
from pathlib import Path
import subprocess,tempfile
from home_cover_cache_host_test import function
R=Path(__file__).resolve().parents[1]
unit=r'''
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
typedef struct{int x,y,width,height;}EpdRect;
typedef struct{uint8_t *difference_fb;bool*dirty_lines;uint8_t*dirty_columns;}EpdiyHighlevelState;
enum EpdDrawError{EPD_DRAW_SUCCESS,EPD_DRAW_EMPTY_LINE_QUEUE};
static int8_t states[8],*s_states=states;static int s_pending;
static int continuous_du_waveform,calls;static bool fail;
enum{READ_PICO_EPD_SCAN_FAST,MODE_PACKING_1PPB_DIFFERENCE=2,MODE_DU=4};
static int epd_width(void){return 4;}static int epd_height(void){return 2;}
static void read_pico_epd_use_scan(int s){(void)s;}
static EpdRect epd_full_screen(void){return(EpdRect){0,0,4,2};}
static enum EpdDrawError epd_draw_base(EpdRect a,uint8_t*d,EpdRect b,int m,int t,bool*l,uint8_t*c,const int*w){
 (void)a;(void)d;(void)b;(void)m;(void)t;(void)l;(void)c;(void)w;
 ++calls;return fail?EPD_DRAW_EMPTY_LINE_QUEUE:EPD_DRAW_SUCCESS;
}
'''+function('continuous_du_scan',R/'components/continuous_du/continuous_du.c')+r'''
int main(void){
 uint8_t diff[8],col[2];bool line[2];EpdiyHighlevelState h={diff,line,col};
 for(int phase=-16;phase<=16;phase++)for(int x=-1;x<4;x++)for(int width=1;width<=5;width++){
  for(int i=0;i<8;i++)states[i]=i%2?-phase:phase;
  int8_t before[8];memcpy(before,states,8);s_pending=phase?8:0;int pending=s_pending;
  fail=true;calls=0;EpdRect region={x,0,width,1};
  enum EpdDrawError err=continuous_du_scan(&h,region);
  assert(!memcmp(before,states,8)&&s_pending==pending);
  assert(err==(calls?EPD_DRAW_EMPTY_LINE_QUEUE:EPD_DRAW_SUCCESS));
  fail=false;continuous_du_scan(&h,region);
  for(int i=0;i<8;i++){
   bool inside=i<4&&i>=x&&i<x+width;
   int expected=before[i];if(inside&&expected)expected+=expected>0?-1:1;
   assert(states[i]==expected);
  }
  int count=0;for(int i=0;i<8;i++)if(states[i])count++;assert(s_pending==count);
 }
 puts("PASS: DU failed draws preserve phases/pending; retries deliver exactly once, both directions and clipped regions");
}
'''
with tempfile.TemporaryDirectory() as tmp:
    p=Path(tmp);(p/'test.c').write_text(unit)
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
