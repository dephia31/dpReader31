#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
"""中文：真实书架37/39相逐迁移、官方独立对照、OOM和原厂表隔离。
English: Actual shelf37/39 transitions, independent vendor oracle, OOM and vendor-table isolation.
"""
from pathlib import Path
import os,subprocess,tempfile
r=Path(__file__).resolve().parents[1]
unit=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "display_main_waveform.h"
#include "e0470_epaper_waveform.h"
#include "e0470_waveform_trim.h"
#ifdef HAVE_VENDOR_ORACLE
#include "my_waveform_trim.h"
#endif
static bool oom;static unsigned allocations;
void *heap_caps_malloc(size_t n,int caps){assert(caps==3&&n<=6144);++allocations;return oom?NULL:malloc(n);}
int main(int argc,char **argv){
 assert(argc==2);oom=atoi(argv[1]);e0470_waveform_init();
 const EpdWaveformPhases *gl=e0470_waveform_phases(&E0470_FULL_WAVEFORM,MODE_GL16);
 const EpdWaveformPhases *bw=e0470_waveform_phases(&E0470_FULL_WAVEFORM,MODE_DU);
 uint8_t saved[48*64];memcpy(saved,gl->luts,sizeof(saved));
#if KIIKO_S31_SHELF37
 bool requested_oom=oom;oom=false;
 const EpdWaveform *prior[]={display_main_normal_waveform(),display_main_gray_waveform(),display_main_bw_waveform(),
  display_main_shelf_waveform(MODE_GL16),display_main_navigation_waveform(),display_main_bw_turn_waveform()};
 for(unsigned i=0;i<6;++i){int m=i==2?MODE_DU:MODE_GL16;assert(display_main_switch_waveform(prior[i],m)!=prior[i]);}
 oom=requested_oom;
 if(oom){unsigned was=allocations;assert(!display_main_shelf37_waveform(false)&&allocations==was+1);oom=false;}
 unsigned before=allocations;
 const EpdWaveform *soft=display_main_shelf37_waveform(false),*art=display_main_shelf37_waveform(true);
 assert(soft&&art&&soft!=art&&allocations==before+1);
 uint8_t expected[48][16][4];
#ifdef HAVE_VENDOR_ORACLE
 const my_waveform_trim_t trim={11,5,0,3};
 assert(my_waveform_trim(gl,&trim,expected)==37);
#else
 const e0470_trim_t trim={11,5,0,3};assert(e0470_waveform_trim(gl,&trim,expected)==37);
#endif
 const EpdWaveformPhases oracle={37,(const uint8_t*)expected,NULL};
 const EpdWaveform *waves[]={soft,art};
 for(int kind=0;kind<2;++kind){
  const EpdWaveformPhases *p=e0470_waveform_phases(waves[kind],MODE_GL16);
  assert(p&&p->phases==37&&!p->phase_times&&waves[kind]->temp_intervals==E0470_FULL_WAVEFORM.temp_intervals);
  assert(!e0470_waveform_phases(waves[kind],MODE_GC16));
  for(int to=0;to<16;++to)for(int from=0;from<16;++from)for(int f=0;f<37;++f){
   int want=e0470_phase_action(&oracle,f,to,from);
   if(!kind&&to==from)want=0;
   else if((!kind&&(to==0||to==15))||(kind&&to==15&&from!=15))
    want=f<20?e0470_phase_action(bw,f,to,from):0;
   assert(e0470_phase_action(p,f,to,from)==want);
   if(f>=34)assert(!want);
  }
  const EpdWaveform *strong=display_main_switch_waveform(waves[kind],MODE_GL16);
  const EpdWaveformPhases *q=e0470_waveform_phases(strong,MODE_GL16);assert(q&&q->phases==39);
  for(int to=0;to<16;++to)for(int from=0;from<16;++from)for(int f=0;f<39;++f){
   int want=f<35?e0470_phase_action(p,f,to,from):f<37?(to==15&&from!=15?2:0):e0470_phase_action(p,f-2,to,from);
   assert(e0470_phase_action(q,f,to,from)==want);
  }
 }
 const EpdWaveform *root=display_main_switch_waveform(prior[0],MODE_GL16);
 const EpdWaveform *lower=display_main_switch_waveform(root,MODE_GL16);
 assert(e0470_waveform_phases(lower,MODE_GL16)->phases==52);
 before=allocations;for(int i=0;i<100;++i){assert(display_main_shelf37_waveform(false)==soft);assert(display_main_shelf37_waveform(true)==art);}
 assert(before==allocations);
 puts("PASS: actual37-phase16-gray art/chrome,256 pairs match vendor trim/DU endpoints,39-phase root erase, release tails, one6KiB cache/OOM retry/no warm allocations");
#else
 (void)bw;assert(!display_main_shelf37_waveform(false)&&!display_main_shelf37_waveform(true)&&!allocations);
 puts("PASS: shelf37 OFF allocates nothing and retains native48 fallback");
#endif
 assert(!memcmp(saved,gl->luts,sizeof(saved)));
}
'''
with tempfile.TemporaryDirectory(prefix='s31-shelf37-') as folder:
    t=Path(folder);c=t/'test.c';c.write_text(unit)
    flags=['cc','-std=c11','-Wall','-Wextra','-Werror','-g','-fsanitize=address,undefined',
      '-DKIIKO_BOARD_METALIO47=1','-Imain','-Itools/shelf_waveform_host_stubs','-Itools/waveform_host_stubs',
      '-Icomponents/epdiy/include','-Icomponents/epdiy/src','-Icomponents/e0470_epaper_waveform/include',
      '-Icomponents/e0470_epaper_waveform/metalio']
    files=[str(c),'boards/metalio47/src/main/display_main_waveform.c',
      'boards/metalio47/src/components/e0470_epaper_waveform/e0470_epaper_waveform.c',
      'components/e0470_epaper_waveform/e0470_waveform_trim.c']
    vendor=os.environ.get('KIIKO_METALIO_OFFICIAL_TRIM')
    if vendor:
        (t/'epd_internals.h').write_text('#include "epd_waveform.h"\n')
        flags+=['-DHAVE_VENDOR_ORACLE=1','-I'+str(t),'-I'+vendor]
        files+=[str(Path(vendor)/'my_waveform_trim.c')]
    for enabled in (1,0):
        exe=t/f'test-{enabled}'
        subprocess.run(flags+[f'-DKIIKO_S31_SHELF37={enabled}',*files,'-o',str(exe)],cwd=r,check=True)
        for fail in (0,1):subprocess.run([str(exe),str(fail)],check=True)
    print('Independent vendor oracle:',bool(vendor))
