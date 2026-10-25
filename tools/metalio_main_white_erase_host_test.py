#!/usr/bin/env python3
"""Actual S31 root-switch LUTs and presentation scope. Optical ghosting needs panel tests.
SPDX-License-Identifier: Apache-2.0
"""
from pathlib import Path
import subprocess
import tempfile
from board_source_view import select_source
from home_cover_cache_host_test import function

ROOT=Path(__file__).resolve().parents[1]
unit=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "display_main_waveform.h"
#include "e0470_epaper_waveform.h"
static bool oom;
static unsigned allocations;
static size_t bytes;
void *heap_caps_malloc(size_t n,int flags){assert(flags==3&&n<=52*64);if(oom)return NULL;++allocations;bytes+=n;return malloc(n);}
static int count(const EpdWaveformPhases *p,int to,int from,int action){int n=0;for(int f=0;f<p->phases;++f)n+=e0470_phase_action(p,f,to,from)==action;return n;}
static void verify(const EpdWaveform *source,int mode){
 const EpdWaveformPhases *s=e0470_waveform_phases(source,mode);assert(s);
 uint8_t saved[50*64];memcpy(saved,s->luts,(size_t)s->phases*64);
 const EpdWaveform *w=display_main_switch_waveform(source,mode);
 assert(w!=source&&w->num_modes==1&&w->temp_intervals==source->temp_intervals);
 const EpdWaveformPhases *p=e0470_waveform_phases(w,mode);assert(p&&p->phases==s->phases+2&&!p->phase_times);
 int insert=s->phases-2;
 for(int from=0;from<16;++from)for(int to=0;to<16;++to){
  for(int f=0;f<p->phases;++f){
   int expected=f<insert?e0470_phase_action(s,f,to,from):
    f<insert+2?(to==15&&from!=15?2:0):e0470_phase_action(s,f-2,to,from);
   assert(e0470_phase_action(p,f,to,from)==expected);
  }
  assert(count(p,to,from,1)==count(s,to,from,1));
  assert(count(p,to,from,2)==count(s,to,from,2)+(to==15&&from!=15?2:0));
  if(to==15&&from!=15)for(int f=p->phases-2;f<p->phases;++f)assert(!e0470_phase_action(p,f,to,from));
 }
 assert(!memcmp(saved,s->luts,(size_t)s->phases*64));
 unsigned before=allocations;for(int i=0;i<100;i++)assert(display_main_switch_waveform(source,mode)==w);
 assert(before==allocations);
}
int main(void){
 e0470_waveform_init();
 const EpdWaveform *sources[]={display_main_normal_waveform(),display_main_gray_waveform(),display_main_bw_waveform(),
    display_main_shelf_waveform(MODE_GL16),display_main_navigation_waveform(),display_main_bw_turn_waveform(),
    display_main_shelf_soft_waveform()};
 int modes[]={MODE_GL16,MODE_GL16,MODE_DU,MODE_GL16,MODE_GL16,MODE_GL16,MODE_GL16};
 uint8_t raw[3][48*64];int native_modes[]={MODE_DU,MODE_GL16,MODE_GC16};
 for(int i=0;i<3;i++){const EpdWaveformPhases *p=e0470_waveform_phases(&E0470_FULL_WAVEFORM,native_modes[i]);memcpy(raw[i],p->luts,(size_t)p->phases*64);}
 // Failed first allocation is not cached as a success; the same source can retry later.
 oom=true;unsigned before=allocations;
 assert(display_main_switch_waveform(sources[0],modes[0])==sources[0]&&allocations==before);oom=false;
 for(unsigned i=0;i<sizeof(sources)/sizeof(sources[0]);i++)verify(sources[i],modes[i]);
 const EpdWaveform *first=display_main_switch_waveform(sources[0],MODE_GL16);
 verify(first,MODE_GL16);
 const EpdWaveform *second=display_main_switch_waveform(first,MODE_GL16);
 assert(e0470_waveform_phases(second,MODE_GL16)->phases==52);
 assert(display_main_switch_waveform(second,MODE_GL16)==second);
 assert(display_main_switch_waveform(NULL,MODE_GL16)==NULL);
 assert(display_main_switch_waveform(&E0470_FULL_WAVEFORM,MODE_GC16)==&E0470_FULL_WAVEFORM);
 assert(display_main_switch_waveform(&E0470_FOLLOW_WAVEFORM,MODE_DU)==&E0470_FOLLOW_WAVEFORM);
 // A future incompatible tail or timed source is left untouched; do not shorten release phases.
 static uint8_t bad[20][16][4];bad[19][15][0]=128;
 EpdWaveformPhases phase={20,(const uint8_t*)bad,NULL};const EpdWaveformPhases *range=&phase;
 EpdWaveformMode mode={MODE_GL16,1,&range};const EpdWaveformMode *mode_ptr=&mode;
 EpdWaveform wave={.num_modes=1,.num_temp_ranges=1,.mode_data=&mode_ptr,.temp_intervals=sources[0]->temp_intervals};
 assert(display_main_switch_waveform(&wave,MODE_GL16)==&wave);
 memset(bad,0,sizeof(bad));
 static EpdWaveform fake[10];static EpdWaveformMode fm[10];static const EpdWaveformMode *fp[10];
 static const EpdWaveformPhases *fr[10];
 for(int i=0;i<10;i++){fr[i]=&phase;fm[i]=(EpdWaveformMode){MODE_GL16,1,&fr[i]};fp[i]=&fm[i];fake[i]=wave;fake[i].mode_data=&fp[i];}
 unsigned used=0;
 for(int i=0;i<10;i++)used+=display_main_switch_waveform(&fake[i],MODE_GL16)!=&fake[i];
 assert(sources[0]==sources[6]); // reader images reuse the root soft-gray profile/cache slot
 assert(used==2); // six root/image profiles + one52-phase lower profile + two optional profiles, then bounded fallback
 assert(bytes<=12*52*64); // includes three original shelf allocations
 for(int i=0;i<3;i++){const EpdWaveformPhases *p=e0470_waveform_phases(&E0470_FULL_WAVEFORM,native_modes[i]);assert(!memcmp(raw[i],p->luts,(size_t)p->phases*64));}
 puts("PASS: actual normal/water/BW/shelf/nav LUTs add exactly2 white drives only for changed-white cells; every original action/order, gray pulse, white equality and release tail retained; OOM retry/bounded cache/vendor bytes intact");
}
'''

# Execute actual presentation scope and root visibility checks, independently of optical/LUT mocks.
scope=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stddef.h>
typedef struct {int ignored;}app_ctx_t;
typedef enum {APP_REDRAW_NONE,APP_REDRAW_AREA,APP_REDRAW_PAGE,APP_REDRAW_FULL,APP_REDRAW_DONE}app_redraw_t;
typedef struct app_desc_s {int index;bool visible,binary_ui,persistent_navigation;bool (*main_page_visible)(app_ctx_t*);}app_desc_t;
static const app_desc_t *s_presented_root,*s_presented_main,*drawing;
static bool erase_scope,home_scope,binary_scope,nav_scope,observed,home_observed;
static app_ctx_t ctx;
static int app_index_of(const app_desc_t *app){return app->index;}
static bool visible(app_ctx_t *ctx){(void)ctx;return drawing->visible;}
static bool display_main_switch_erase_set(bool enabled){bool prior=erase_scope;erase_scope=enabled;return prior;}
static bool display_main_shelf_home_erase_set(bool enabled){bool prior=home_scope;home_scope=enabled;return prior;}
static bool display_system_binary_set(bool enabled){bool prior=binary_scope;binary_scope=enabled;return prior;}
static bool display_system_navigation_set(bool enabled){bool prior=nav_scope;nav_scope=enabled;return prior;}
static void display_main_transition_disarm(void){}
'''
path=ROOT/'main/app/app_loop.c'
for name in ('main_page_ready','navigation_page_ready','main_present_finished'):
    scope+=select_source(function(name,path),'metalio47')+'\n'
scope+='static void app_present_impl(app_ctx_t *ctx,const app_desc_t *app,app_redraw_t redraw){observed=erase_scope;home_observed=home_scope;main_present_finished(ctx,app,redraw);}\n'
scope+=select_source(function('app_present',path),'metalio47')+'\n'
scope+=r'''
static void present(app_desc_t *app,app_redraw_t mode,bool expected){bool expected_home=mode==APP_REDRAW_PAGE&&s_presented_root&&s_presented_root->index==1&&app->index==0&&app->visible;drawing=app;observed=!expected;app_present(&ctx,app,mode);assert(observed==expected&&home_observed==expected_home&&!home_scope&&!erase_scope&&!binary_scope&&!nav_scope);}
int main(void){
 app_desc_t pages[5];for(int i=0;i<5;i++)pages[i]=(app_desc_t){.index=i,.visible=true,.main_page_visible=visible};
 for(int a=0;a<4;a++)for(int b=0;b<4;b++){
  s_presented_root=NULL;s_presented_main=NULL;present(&pages[a],APP_REDRAW_PAGE,false);
  present(&pages[b],APP_REDRAW_PAGE,a!=b);
  present(&pages[b],APP_REDRAW_PAGE,false); // same-page redraw or clock/cover completion
  present(&pages[a],APP_REDRAW_AREA,false);present(&pages[b],APP_REDRAW_FULL,false);
  pages[a].visible=false;present(&pages[a],APP_REDRAW_PAGE,false); // child/reader
  present(&pages[b],APP_REDRAW_PAGE,false);pages[a].visible=true;
 }
 present(&pages[4],APP_REDRAW_PAGE,false);present(&pages[0],APP_REDRAW_PAGE,false);
 present(&pages[1],APP_REDRAW_DONE,false);present(&pages[1],APP_REDRAW_NONE,false);
 puts("PASS: actual presenter scopes erasure only to4 visible root tabs and changed tab PAGE outputs; excludes first entry, same-page work, children/reading, explicit full refresh and non-root apps; state restored");
}
'''
with tempfile.TemporaryDirectory() as folder:
    d=Path(folder);c=d/'wave.c';c.write_text(unit);exe=d/'wave'
    command=['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-g','-fsanitize=address,undefined',
       '-DKIIKO_BOARD_METALIO47=1','-I'+str(ROOT/'main'),'-I'+str(ROOT/'tools/shelf_waveform_host_stubs'),
       '-I'+str(ROOT/'tools/waveform_host_stubs'),'-I'+str(ROOT/'components/epdiy/include'),
       '-I'+str(ROOT/'components/epdiy/src'),'-I'+str(ROOT/'components/e0470_epaper_waveform/include'),
       '-I'+str(ROOT/'components/e0470_epaper_waveform/metalio'),str(c),
       str(ROOT/'boards/metalio47/src/main/display_main_waveform.c'),
       str(ROOT/'boards/metalio47/src/components/e0470_epaper_waveform/e0470_epaper_waveform.c'),
       str(ROOT/'components/e0470_epaper_waveform/e0470_waveform_trim.c'),'-o',str(exe)]
    subprocess.run(command,check=True);subprocess.run([str(exe)],check=True)
    c=d/'scope.c';c.write_text(scope);exe=d/'scope'
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(c),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
    # Reuse the actual band-engine harness to validate the two additional phases and every failure tick.
    (d/'esp_log.h').write_text('#pragma once\n#include <stdio.h>\n#undef ESP_LOGI\n'
        '#define ESP_LOGI(tag,fmt,...) do { char b[256];(void)(tag);'
        '(void)snprintf(b,sizeof(b),fmt,__VA_ARGS__); } while(0)\n')
    (d/'esp_heap_caps.h').write_text((ROOT/'tools/water_turn_stubs/esp_heap_caps.h').read_text()+
        '\nstatic inline void *heap_caps_malloc(size_t n,int caps){(void)caps;return malloc(n);}\n')
    command=['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unknown-attributes',
       '-g','-fsanitize=address,undefined','-DKIIKO_BOARD_METALIO47=1','-DKIIKO_S31_TEXT_RIPPLE=0',
       '-DKIIKO_TEST_MAIN_SWITCH_ERASE=1','-I'+str(d),'-I'+str(ROOT/'components/epdiy/src'),
       '-I'+str(ROOT/'components/epdiy/include'),'-I'+str(ROOT/'components/e0470_epaper_waveform/include'),
       '-I'+str(ROOT/'components/e0470_epaper_waveform/metalio'),'-I'+str(ROOT/'components/e0470_page_turn/include'),
       '-I'+str(ROOT/'main'),'-I'+str(ROOT/'boards/metalio47/src/main'),'-I'+str(ROOT/'tools/water_turn_stubs'),
       '-I'+str(ROOT/'tools/metalio_stubs'),str(ROOT/'tools/metalio_text_ripple_host_test.c'),
       str(ROOT/'boards/metalio47/src/main/metalio_text_ripple.c'),
       str(ROOT/'boards/metalio47/src/main/display_main_waveform.c'),
       str(ROOT/'boards/metalio47/src/components/e0470_page_turn/e0470_page_turn.c'),
       str(ROOT/'boards/metalio47/src/components/e0470_epaper_waveform/e0470_epaper_waveform.c'),
       str(ROOT/'components/e0470_epaper_waveform/e0470_waveform_trim.c'),'-o',str(d/'bands')]
    subprocess.run(command,check=True);subprocess.run([str(d/'bands')],check=True)
