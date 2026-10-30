#!/usr/bin/env python3
"""Check the actual AA37 LUT, native pulses, immutable sources and fallback."""
from pathlib import Path
import subprocess, tempfile
from home_cover_cache_host_test import function
R=Path(__file__).resolve().parents[1]
source=R/'boards/metalio47/src/main/display_reader_waveform.c'
unit=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "epdiy.h"
#include "e0470_epaper_waveform.h"
#include "e0470_waveform_trim.h"
extern const EpdWaveformPhases my_gl_25_0;
static bool aa,fail;
static unsigned calls;
static bool app_settings_book_antialias(void){return aa;}
static int checked_trim(const EpdWaveformPhases *p,const e0470_trim_t *t,uint8_t (*dst)[16][4]){
 ++calls;return fail?0:e0470_waveform_trim(p,t,dst);
}
#define e0470_waveform_trim checked_trim
'''
for name in ('reader_native_text_waveform','display_reader_text_waveform'):
 unit+=function(name,source)+'\n'
unit+=r'''
#undef e0470_waveform_trim
static int count(const EpdWaveformPhases *p,int to,int old,int action){int n=0;for(int f=0;f<p->phases;++f)n+=e0470_phase_action(p,f,to,old)==action;return n;}
int main(int argc,char **argv){
 assert(argc==2);fail=atoi(argv[1]);e0470_waveform_init();
 const EpdWaveformPhases *gc=e0470_waveform_phases(&E0470_FULL_WAVEFORM,MODE_GC16);
 const EpdWaveformPhases *white=e0470_waveform_phases(&E0470_TEXTTURN_WAVEFORM,MODE_GL16);
 uint8_t raw[48*64],full[48*64],previous[48*64];
 memcpy(raw,my_gl_25_0.luts,sizeof(raw));memcpy(full,gc->luts,sizeof(full));memcpy(previous,white->luts,sizeof(previous));
 aa=false;assert(display_reader_text_waveform()==&E0470_TEXTTURN_WAVEFORM&&!calls);
 aa=true;const EpdWaveform *wave=display_reader_text_waveform();
 const EpdWaveformPhases *p=e0470_waveform_phases(wave,MODE_GL16);
 if(fail){assert(wave==reader_native_text_waveform()&&p->phases==48);}
 else{
  assert(p&&p->phases==37&&!p->phase_times);
  assert(!e0470_waveform_phases(wave,MODE_GC16)&&!e0470_waveform_phases(wave,MODE_DU));
  for(int to=0;to<16;++to)for(int old=0;old<16;++old){
   int original[48],expected[48]={0},a=0,z=48;
   for(int f=0;f<48;++f)original[f]=e0470_phase_action(&my_gl_25_0,f,to,old);
   while(a<z&&!original[a]){++a;}
   while(z>a&&!original[z-1]){--z;}
   int erase=to==15?1:2,e=0;while(a+e<z&&original[a+e]==erase)++e;
   int sat=to==15?2:1,run=0;
   while(a+e+run<z&&original[a+e+run]==sat)++run;
   int cut=to==15?0:(run>5?5:run>0?run-1:0);
   int retained=e<11?e:11,n=retained+z-a-e-cut,start=34-n;
   assert(start>=0);
   for(int i=0;i<retained;++i)expected[start+i]=erase;
   for(int i=a+e+cut;i<z;++i)expected[start+retained+i-a-e-cut]=original[i];
   if(to==old)memset(expected,0,sizeof(expected));
   if(to==15&&old==15)for(int i=0;i<count(white,15,15,2);++i)expected[33-i]=2;
   for(int f=0;f<37;++f){int action=e0470_phase_action(p,f,to,old);assert(action==expected[f]);if(f>=34)assert(!action);}
   if(to!=old)assert(count(p,to,old,sat)==count(&my_gl_25_0,to,old,sat)-cut);
  }
  assert(count(p,0,15,1)==13&&count(p,15,0,2)==18);
  assert(count(p,15,15,1)==0&&count(p,15,15,2)==count(white,15,15,2));
 }
 for(int n=0;n<100;++n){aa=false;assert(display_reader_text_waveform()==&E0470_TEXTTURN_WAVEFORM);aa=true;assert(display_reader_text_waveform()==wave);}
 assert(calls==1);
 assert(!memcmp(raw,my_gl_25_0.luts,sizeof(raw))&&!memcmp(full,gc->luts,sizeof(full))&&!memcmp(previous,white->luts,sizeof(previous)));
 puts(fail?"PASS AA37 failed derivation: cached native48 fallback, no source mutation":"PASS actual AA37: all256pairs, gray tails, black13/white18, white maintenance,3finalholds, AAofforiginal, immutableGC48/GL48");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(unit)
 cmd=['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined','-DKIIKO_BOARD_METALIO47=1']
 for include in ['main','tools/waveform_host_stubs','tools/shelf_waveform_host_stubs','components/epdiy/include','components/epdiy/src','components/e0470_epaper_waveform/include','components/e0470_epaper_waveform/waveforms','components/e0470_epaper_waveform/metalio']:cmd+=['-I'+str(R/include)]
 cmd += [str(p/'test.c'),str(R/'boards/metalio47/src/components/e0470_epaper_waveform/e0470_epaper_waveform.c'),str(R/'components/e0470_epaper_waveform/e0470_waveform_trim.c'),'-o',str(p/'test')]
 subprocess.run(cmd,check=True)
 for failure in (0,1):subprocess.run([str(p/'test'),str(failure)],check=True)
