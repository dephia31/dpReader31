"""Check the real S31 output adapter without panel hardware."""
from pathlib import Path
import subprocess,tempfile
from home_cover_cache_host_test import function
root=Path(__file__).resolve().parents[1]
src=root/'boards/metalio47/src/main/display.c'
unit=r'''
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
enum EpdDrawError { EPD_DRAW_SUCCESS,EPD_DRAW_FAILED_ALLOC,EPD_DRAW_INVALID_CROP };
enum EpdDrawMode {MODE_GC16,MODE_GL16};
typedef int EpdWaveform;
static EpdWaveform E0470_FULL_WAVEFORM,E0470_WAVEFORM;
typedef struct {uint8_t *front_fb,*back_fb;} EpdiyHighlevelState;
#define METALIO_CPU_GUARD_OR_RETURN(x) ((void)0)
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
#define APP_UI_GC16_EVERY 4
static bool enabled,fail,oom,full_seen;
static bool s_full_refresh_requested,s_main_fault,s_output_inverted;
static uint8_t *s_inverted_front;
static unsigned s_inverted_pages;
static uint8_t expected_front[8],expected_back[8];
static int scans;
static int epd_width(void){return 4;}
static int epd_height(void){return 4;}
static bool app_settings_screen_inverted(void){return enabled;}
static bool pico_online_display_begin(void){return true;}
static void pico_online_display_end(bool h){assert(h);}
static void *heap_caps_aligned_alloc(int a,size_t n,int c){(void)a;(void)c;return oom?NULL:malloc(n);}
static void continuous_du_reset(void){}
static void display_main_transition_cancel(void){}
static void use_scan_for(const EpdWaveform *w,enum EpdDrawMode m){(void)w;(void)m;}
static void epd_poweron(void){}
static void epd_hl_waveform(EpdiyHighlevelState *h,const EpdWaveform *w){(void)h;(void)w;}
static void ui_page_cleaned(enum EpdDrawError r){assert(r==EPD_DRAW_SUCCESS);}
static void rails_keepalive(void){}
static enum EpdDrawError scan(EpdiyHighlevelState *h,bool full){
 ++scans;full_seen=full;
 assert(!memcmp(h->front_fb,expected_front,8));
 assert(!memcmp(h->back_fb,expected_back,8));
 if(fail)return EPD_DRAW_FAILED_ALLOC;
 memcpy(h->back_fb,h->front_fb,8);return EPD_DRAW_SUCCESS;
}
static enum EpdDrawError epd_hl_update_screen_full(EpdiyHighlevelState *h,enum EpdDrawMode m,int t){assert(m==MODE_GC16&&t==25);return scan(h,true);}
static enum EpdDrawError epd_hl_update_screen(EpdiyHighlevelState *h,enum EpdDrawMode m,int t){assert(m==MODE_GL16&&t==25);return scan(h,false);}
'''
unit+=function('display_inverted_output',src)
unit+=r'''
static void expect(uint8_t *front,uint8_t *back,bool old_dark,bool new_dark){
 for(int i=0;i<8;i++){expected_front[i]=front[i]^(new_dark?255:0);expected_back[i]=back[i]^(old_dark?255:0);}
}
int main(void){
 uint8_t front[]={0x01,0x23,0x45,0x67,0x89,0xab,0xcd,0xef},back[8],saved[8];
 memset(back,255,8);memcpy(saved,front,8);EpdiyHighlevelState h={front,back};
 enabled=true;oom=true;assert(display_inverted_output(&h,false)==EPD_DRAW_FAILED_ALLOC&&scans==0&&h.front_fb==front);
 oom=false;expect(front,back,false,true);assert(display_inverted_output(&h,false)==0&&full_seen);
 assert(h.front_fb==front&&!memcmp(front,saved,8)&&!memcmp(back,front,8));
 front[0]=0x7e;expect(front,back,true,true);assert(display_inverted_output(&h,false)==0&&!full_seen);
 memcpy(saved,back,8);front[1]=0x99;expect(front,back,true,true);fail=true;
 assert(display_inverted_output(&h,false)!=0&&!memcmp(back,saved,8)&&s_full_refresh_requested);
 fail=false;expect(front,back,true,true);assert(display_inverted_output(&h,false)==0&&full_seen&&!s_full_refresh_requested);
 enabled=false;expect(front,back,true,false);assert(display_inverted_output(&h,false)==0&&full_seen);
 assert(!s_output_inverted&&!memcmp(back,front,8)&&h.front_fb==front);
 // Failed light-to-dark transition retains the previous polarity/history.
 enabled=true;fail=true;memcpy(saved,back,8);expect(front,back,false,true);
 assert(display_inverted_output(&h,false)!=0&&!s_output_inverted&&!memcmp(back,saved,8));
 free(s_inverted_front);puts("PASS S31 invert: all 16 grays, logical caches/history, toggles, differential scans, OOM and failed draw recovery flags");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(unit)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
