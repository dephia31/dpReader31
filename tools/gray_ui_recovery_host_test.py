#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
"""Exercise real S31 panel/root outlets: native midgray pixels must never enter the text quantizer."""
from pathlib import Path
import subprocess,tempfile
from home_cover_cache_host_test import function
r=Path(__file__).resolve().parents[1]
fixture=r/'tools/display_host_test.c';u=fixture.read_text().split('int main(void)')[0]
u=u.replace('app_main_refresh_mode_t app_settings_main_refresh_mode(void) {return APP_MAIN_REFRESH_NORMAL;}',
'''static app_main_refresh_mode_t selected_mode;
app_main_refresh_mode_t app_settings_main_refresh_mode(void) {return selected_mode;}''')
u=u.replace('bool app_settings_main_fast_refresh(void) {return false;}',
            'bool app_settings_main_fast_refresh(void) {return selected_mode==APP_MAIN_REFRESH_FAST;}')
for name in ['e0470_update_regions','e0470_page_turn_with_waveform_compact']:
 u=u.replace(function(name,fixture),'')
u+=r'''
static int region_calls;static EpdRect region_body;
enum EpdDrawError e0470_update_regions(EpdiyHighlevelState*h,EpdRect area,EpdRect strong,const EpdWaveform*soft,const EpdWaveform*art,enum EpdDrawMode m){
 assert(ota_display_depth&&area.height==1120&&strong.y==192&&strong.height==800);
 assert(soft==&soft_gray&&art==&E0470_WAVEFORM&&m==MODE_GL16);
 ++region_calls;region_body=strong;return draw(h,m,25,false);
}
enum EpdDrawError e0470_page_turn_with_waveform_compact(EpdiyHighlevelState*h,EpdRect area,e0470_turn_dir_t d,const EpdWaveform*w,unsigned b){
 assert(ota_display_depth&&area.width==684&&area.height==1120&&d==E0470_TURN_RTL&&b==8);
 assert(w==&soft_gray);++water_calls;return draw(h,MODE_GL16,25,false);
}
static void fill(EpdiyHighlevelState*h,uint8_t body,uint8_t nav){
 for(int y=0;y<epd_height();++y){memset(h->front_fb+(size_t)y*608,body,544);memset(h->front_fb+(size_t)y*608+544,nav,64);}
}
static void equal_frame(EpdiyHighlevelState*h){memcpy(h->back_fb,h->front_fb,FB_BYTES);}
int main(void){
 static uint8_t front[FB_BYTES],back[FB_BYTES];EpdiyHighlevelState h={.front_fb=front,.back_fb=back};
 const EpdRect panel={0,468,684,748};selected_mode=APP_MAIN_REFRESH_NORMAL;
 // All16 source tones, including13/14 acrylic highlights, survive repeated panel settles without collapsing to white.
 for(int value=0;value<16;++value){
  fill(&h,(uint8_t)(value*17),0xee);equal_frame(&h);
  for(int again=0;again<3;++again){
   int before=full_draws;bool old=display_system_binary_set(again==1);(void)old;
   assert(update_display_reader_panel(&h,panel)==EPD_DRAW_SUCCESS&&full_draws==before+1);
   assert(front[0]==value*17&&front[544]==0xee&&last_mode==MODE_GL16&&waveform_at_draw==&E0470_WAVEFORM);
   assert(last_area.y==468&&last_area.height==748&&turn_tick_us==21000&&!ota_display_depth);
   assert(display_system_binary_set(false)==(again==1));
  }
 }
 // Covers/acrylic in ordinary and water retain exactly the same raw16-gray targets as manual full output.
 for(int mode=0;mode<2;++mode)for(int value=0;value<16;++value){
  selected_mode=mode?APP_MAIN_REFRESH_WATER:APP_MAIN_REFRESH_NORMAL;
  fill(&h,0xff,0xee);equal_frame(&h);fill(&h,(uint8_t)(value*17),0xee);
  int previous_region=region_calls,previous_water=water_calls;
  display_main_transition_arm((void*)1,false,true);
  if(!mode){EpdRect art={0,192,684,800};display_main_transition_gray_bands(&art,1);}
  assert(update_display_fast_page(&h)==EPD_DRAW_SUCCESS);
  assert(front[0]==value*17&&front[544]==0xee);
  if(value!=15)assert(mode?water_calls==previous_water+1:region_calls==previous_region+1);
  memcpy(target,front,FB_BYTES);assert(update_display_full(&h)==EPD_DRAW_SUCCESS);
  assert(!memcmp(front,target,FB_BYTES)&&last_mode==MODE_GC16);
 }
 // Explicit full requests precede regional output and retain every source tone.
 fill(&h,0xee,0xee);int before=full_draws;display_request_full_refresh();
 assert(update_display_reader_panel(&h,panel)==EPD_DRAW_SUCCESS&&full_draws==before+1&&last_mode==MODE_GC16&&front[0]==0xee);
 // Failure leaves raw target/history intact for the existing complete-gray recovery.
 fill(&h,0xaa,0xee);equal_frame(&h);fill(&h,0xdd,0xee);next_draw_error=EPD_DRAW_EMPTY_LINE_QUEUE;
 assert(update_display_reader_panel(&h,panel)==EPD_DRAW_EMPTY_LINE_QUEUE&&front[0]==0xdd&&back[0]==0xaa);
 assert(update_display_reader_panel(&h,panel)==EPD_DRAW_SUCCESS&&front[0]==0xdd&&back[0]==0xdd&&last_mode==MODE_GC16);
 assert(!ota_display_depth);
 puts("PASS: actual graphical outlets preserve16tones including13/14,repeat regional factory settling,match root art targets before/after manual full,retain water/nav/scopes,full priority and fault recovery");
}
'''
with tempfile.TemporaryDirectory(prefix='s31-gray-repair-') as folder:
 t=Path(folder);c=t/'ui.c';c.write_text(u)
 subprocess.run(['python3','tools/metalio_clear_test_fragment.py'],cwd=r,check=True)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable','-g','-fsanitize=address,undefined','-ffunction-sections','-fdata-sections','-Wl,-dead_strip','-DKIIKO_BOARD_METALIO47=1','-Ibuild/book-tests','-Itools/display_host_stubs','-Icomponents/continuous_du/include','-Icomponents/e0470_page_turn/include','-Imain','-Imain/app',str(c),'main/display.c','-o',str(t/'ui')],cwd=r,check=True)
 subprocess.run([str(t/'ui')],cwd=r,check=True)
