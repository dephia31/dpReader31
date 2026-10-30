#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Check actual native-pulse selective reader cleanup, without optical claims."""
from pathlib import Path
import subprocess,tempfile
from home_cover_cache_host_test import function
R=Path(__file__).resolve().parents[1]
unit=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "epdiy.h"
#include "e0470_epaper_waveform.h"
static bool aa;
static bool app_settings_book_antialias(void){return aa;}
'''+function('reader_native_text_waveform',R/'boards/metalio47/src/main/display_reader_waveform.c')+r'''
int main(void){
 e0470_waveform_init();
 const EpdWaveformPhases *gc=e0470_waveform_phases(&E0470_FULL_WAVEFORM,MODE_GC16);
 const EpdWaveformPhases *gl=e0470_waveform_phases(&E0470_TEXTTURN_WAVEFORM,MODE_GL16);
 assert(gc&&gl&&gc->phases==48&&gl->phases==48);
 uint8_t before_gc[48*64],before_gl[48*64];memcpy(before_gc,gc->luts,sizeof(before_gc));memcpy(before_gl,gl->luts,sizeof(before_gl));
 aa=false;assert(reader_native_text_waveform()==&E0470_TEXTTURN_WAVEFORM);
 aa=true;const EpdWaveform *wave=reader_native_text_waveform();assert(wave!=&E0470_TEXTTURN_WAVEFORM);
 const EpdWaveformPhases *p=e0470_waveform_phases(wave,MODE_GL16);assert(p&&p->phases==48&&!p->phase_times);
 unsigned maintenance=0;
 for(int f=0;f<48;++f)for(int to=0;to<16;++to)for(int from=0;from<16;++from){
  int action=e0470_phase_action(p,f,to,from);
  assert(action==e0470_phase_action(to==15&&from==15?gl:gc,f,to,from));
  if(to==15&&from==15){assert(action!=1&&action!=3);maintenance+=action==2;}
 }
 assert(maintenance>0);
 for(int i=0;i<100;++i){aa=false;assert(reader_native_text_waveform()==&E0470_TEXTTURN_WAVEFORM);aa=true;assert(reader_native_text_waveform()==wave);}
 assert(!memcmp(before_gc,gc->luts,sizeof(before_gc))&&!memcmp(before_gl,gl->luts,sizeof(before_gl)));
 puts("PASS actual selective reader: 255 ink transitions byte-exact nativeGC48, white-white exactexistingGL/no black pulse, fixed48phases, cached3KiB, AAofforiginal, sourcesunchanged");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(unit)
 cmd=['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined','-DKIIKO_BOARD_METALIO47=1']
 for d in ['main','tools/waveform_host_stubs','tools/shelf_waveform_host_stubs','components/epdiy/include','components/epdiy/src','components/e0470_epaper_waveform/include','components/e0470_epaper_waveform/waveforms','components/e0470_epaper_waveform/metalio']:cmd+=['-I'+str(R/d)]
 cmd += [str(p/'test.c'),str(R/'boards/metalio47/src/components/e0470_epaper_waveform/e0470_epaper_waveform.c'),str(R/'components/e0470_epaper_waveform/e0470_waveform_trim.c'),'-o',str(p/'test')]
 subprocess.run(cmd,check=True);subprocess.run([str(p/'test')],check=True)
