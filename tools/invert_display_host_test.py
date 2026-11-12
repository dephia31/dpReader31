"""Real dark-output adapter: DU/GL/GC routing, crop, rotation and history."""
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
enum EpdDrawMode {MODE_GC16,MODE_GL16,MODE_DU};
enum EpdRotation {EPD_ROT_LANDSCAPE,EPD_ROT_PORTRAIT,EPD_ROT_INVERTED_LANDSCAPE,EPD_ROT_INVERTED_PORTRAIT};
typedef struct {int x,y,width,height;} EpdRect;
typedef int EpdWaveform;
static EpdWaveform E0470_FULL_WAVEFORM,E0470_WAVEFORM,bw_wave,gray_wave,flash_wave;
typedef struct {uint8_t *front_fb,*back_fb;} EpdiyHighlevelState;
#define METALIO_CPU_GUARD_OR_RETURN(x) ((void)0)
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
#define APP_UI_GC16_EVERY 4
#define BYTES 128
#define _swap_int(a,b) do{int v=(a);(a)=(b);(b)=v;}while(0)
static bool enabled,fail,oom,full_seen,flash_available=true;
static bool s_full_refresh_requested,s_main_fault,s_output_inverted;
static uint8_t *s_inverted_front;
static unsigned s_inverted_pages;
static int scans;
static enum EpdDrawMode seen_mode;
static enum EpdRotation rotation;
static EpdRect seen_area;
static const EpdWaveform *seen_wave;
static int epd_width(void){return 64;}
static int epd_height(void){return 4;}
static EpdRect epd_full_screen(void){return (EpdRect){0,0,64,4};}
static enum EpdRotation epd_get_rotation(void){return rotation;}
static bool app_settings_screen_inverted(void){return enabled;}
static bool pico_online_display_begin(void){return true;}
static void pico_online_display_end(bool h){assert(h);}
static void *heap_caps_aligned_alloc(int a,size_t n,int c){(void)a;(void)c;return oom?NULL:malloc(n);}
static void continuous_du_reset(void){}
static void display_main_transition_cancel(void){}
static const EpdWaveform *display_main_bw_waveform(void){return &bw_wave;}
static const EpdWaveform *display_main_gray_waveform(void){return &gray_wave;}
static const EpdWaveform *display_reader_flash_waveform(void){return flash_available?&flash_wave:NULL;}
static void use_scan_for(const EpdWaveform *w,enum EpdDrawMode m){seen_wave=w;seen_mode=m;}
static void epd_poweron(void){}
static void epd_hl_waveform(EpdiyHighlevelState *h,const EpdWaveform *w){(void)h;(void)w;}
static void ui_page_cleaned(enum EpdDrawError r){assert(r==EPD_DRAW_SUCCESS);}
static void rails_keepalive(void){}
static uint8_t captured[BYTES],old_physical[BYTES];
'''
# Use the actual highlevel rotation routine, not the new crop helper, for scans.
unit+=function('_inverse_rotated_area',root/'components/epdiy/src/highlevel.c')
unit+=r'''
static enum EpdDrawError scan(EpdiyHighlevelState *h,enum EpdDrawMode m,bool full,EpdRect a){
 ++scans;full_seen=full;seen_mode=m;seen_area=a;
 memcpy(captured,h->front_fb,BYTES);memcpy(old_physical,h->back_fb,BYTES);
 assert(m==MODE_GC16?(seen_wave==&E0470_FULL_WAVEFORM||seen_wave==&flash_wave):
        seen_wave==(m==MODE_DU?&bw_wave:&gray_wave));
 if(fail)return EPD_DRAW_FAILED_ALLOC;
 EpdRect p=full?epd_full_screen():_inverse_rotated_area(a.x,a.y,a.width,a.height);
 // Model real highlevel's aligned 32-column scan/copy. The adapter must freeze
 // neighboring pending pixels, even when they share a byte or scan column.
 int start=p.x/32*16,end=(p.x+p.width+31)/32*16;if(end>32)end=32;
 for(int y=p.y;y<p.y+p.height;y++)memcpy(h->back_fb+y*32+start,h->front_fb+y*32+start,end-start);
 return EPD_DRAW_SUCCESS;
}
static enum EpdDrawError epd_hl_update_screen_full(EpdiyHighlevelState *h,enum EpdDrawMode m,int t){assert(m==MODE_GC16&&t==25);return scan(h,m,true,epd_full_screen());}
static enum EpdDrawError epd_hl_update_area(EpdiyHighlevelState *h,enum EpdDrawMode m,int t,EpdRect a){assert(m!=MODE_GC16&&t==25);return scan(h,m,false,a);}
'''
unit+=function('display_inverted_crop',src)+'\n'+function('display_inverted_with',src)+'\n'+function('display_inverted_output',src)
unit+=r'''
static void pixel(uint8_t *fb,int x,int y,unsigned v){int i=y*32+x/2,s=(x&1)*4;fb[i]=(fb[i]&~(15<<s))|(v<<s);}
static unsigned value(const uint8_t *fb,int x,int y){return(fb[y*32+x/2]>>((x&1)*4))&15;}
static void map(int x,int y,int *px,int *py){
 switch(rotation){case EPD_ROT_PORTRAIT:*px=63-y;*py=x;break;
 case EPD_ROT_INVERTED_LANDSCAPE:*px=63-x;*py=3-y;break;
 case EPD_ROT_INVERTED_PORTRAIT:*px=y;*py=3-x;break;
 default:*px=x;*py=y;break;}
}
static uint8_t front[BYTES],back[BYTES],saved[BYTES];
static EpdiyHighlevelState h={front,back};
static void reset(void){memset(front,255,BYTES);memset(back,255,BYTES);enabled=true;s_output_inverted=true;s_full_refresh_requested=s_main_fault=fail=false;s_inverted_pages=0;rotation=0;}
int main(void){
 reset();s_output_inverted=false;oom=true;
 assert(display_inverted_output(&h,false,NULL,true)==EPD_DRAW_FAILED_ALLOC&&scans==0&&h.front_fb==front);
 oom=false;for(int i=0;i<BYTES;i++)front[i]=(uint8_t)i;memcpy(saved,front,BYTES);
 assert(display_inverted_output(&h,false,NULL,true)==0&&full_seen&&s_output_inverted);
 for(int i=0;i<BYTES;i++)assert(captured[i]==(uint8_t)(front[i]^255)&&old_physical[i]==255);
 assert(h.front_fb==front&&!memcmp(front,saved,BYTES)&&!memcmp(back,front,BYTES));
 // New and old gray transitions use GL; unrelated unchanged gray permits DU.
 front[0]=0x7e;assert(display_inverted_output(&h,false,NULL,true)==0&&seen_mode==MODE_GL16);
 reset();front[1]=back[1]=0x77;front[0]=0;
 assert(display_inverted_output(&h,false,NULL,true)==0&&seen_mode==MODE_DU&&!full_seen);
 front[1]=255;assert(display_inverted_output(&h,false,NULL,true)==0&&seen_mode==MODE_GL16);
 int before=scans;unsigned count=s_inverted_pages;
 assert(display_inverted_output(&h,false,NULL,true)==0&&scans==before&&s_inverted_pages==count);
 reset();front[0]=0x7e;before=scans;
 assert(display_inverted_with(&h,false,NULL,false,true)==0&&scans==before+1);
 assert(full_seen&&seen_wave==&flash_wave&&captured[0]==0x81&&back[0]==0x7e&&s_inverted_pages==0);
 assert(display_inverted_with(&h,false,NULL,false,true)==0&&scans==before+1);
 front[0]=0x19;fail=true;
 assert(display_inverted_with(&h,false,NULL,false,true)==EPD_DRAW_FAILED_ALLOC&&back[0]==0x7e&&h.front_fb==front);
 fail=false;
 assert(display_inverted_with(&h,false,NULL,false,true)==0&&seen_wave==&E0470_FULL_WAVEFORM&&back[0]==0x19);
 reset();front[0]=0;flash_available=false;
 assert(display_inverted_with(&h,false,NULL,false,true)==0&&seen_wave==&E0470_FULL_WAVEFORM);
 flash_available=true;
 // Exact logical crop through all rotations; outside pixel in the same packed
 // byte and additional pending pixels must never leak into this presentation.
 for(int r=0;r<4;r++){
  reset();rotation=r;int px,py;map(1,1,&px,&py);pixel(front,px,py,0);
  pixel(front,px^1,py,0);pixel(front,40,3,0);
  EpdRect a={1,1,1,1};memcpy(saved,front,BYTES);
  assert(display_inverted_output(&h,false,&a,false)==0&&seen_mode==MODE_DU&&!full_seen);
  for(int y=0;y<4;y++)for(int x=0;x<64;x++){
   bool target=x==px&&y==py;
   assert(value(captured,x,y)==(target?15:0));
   assert(value(back,x,y)==(target?0:15));
  }
  assert(!memcmp(front,saved,BYTES)&&s_inverted_pages==0);
 }
 reset();EpdRect outside={1000,1000,10,10};before=scans;
 assert(display_inverted_output(&h,false,&outside,false)==0&&scans==before);
 EpdRect clipped={-1,-1,2,2};pixel(front,0,0,0);pixel(front,1,0,0);
 assert(display_inverted_output(&h,false,&clipped,false)==0&&seen_area.x==0&&seen_area.y==0&&seen_area.width==1&&seen_area.height==1);
 assert(value(back,0,0)==0&&value(back,1,0)==15);
 // Menu/slider updates never count as page turns or trigger periodic GC.
 reset();EpdRect local={1,1,1,1};
 for(int i=0;i<12;i++){pixel(front,1,1,i&1?15:0);assert(display_inverted_output(&h,false,&local,false)==0&&!full_seen&&s_inverted_pages==0);}
 // Page cycle counts only changed successful page submissions.
 reset();for(int i=0;i<4;i++){pixel(front,0,0,i&1?15:0);assert(display_inverted_output(&h,false,NULL,true)==0);assert(full_seen==(i==3));}
 assert(s_inverted_pages==0);
 // On failure keep the complete logical baseline and require a full recovery.
 reset();memcpy(saved,back,BYTES);front[0]=0;fail=true;
 assert(display_inverted_output(&h,false,NULL,true)!=0&&!memcmp(back,saved,BYTES)&&s_full_refresh_requested&&s_inverted_pages==0);
 fail=false;assert(display_inverted_output(&h,false,NULL,true)==0&&full_seen&&!s_full_refresh_requested);
 enabled=false;assert(display_inverted_output(&h,false,&local,false)==0&&full_seen&&!s_output_inverted&&!memcmp(back,front,BYTES));
 enabled=true;fail=true;memcpy(saved,back,BYTES);
 assert(display_inverted_output(&h,false,NULL,true)!=0&&!s_output_inverted&&!memcmp(back,saved,BYTES));
 free(s_inverted_front);
 puts("PASS S31 dark DU/GL/GC: gray transitions, unchanged gray, all rotations, clipping, exact crops/aligned neighbors, no-op/menu/page cycle, OOM/failure/toggle history");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(unit)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
# All local public presenters retain the requested crop, including full-area GL.
s=src.read_text()
for name in ('update_display_area_with','update_display_area_diff_with','update_display_area_full_with','update_display_click_feedback','update_display_reader_panel','update_display_reader_panel_fast','update_display_reader_panel_gray4','update_display_gray_transition'):
 f=function(name,src)
 assert 'display_inverted_output(hl,' in f and '&area,false)' in f,name
assert 'display_inverted_output(hl,(mode & 0x3f)==MODE_GC16,&area,false)' in function('update_display_area_full_with',src)
# Validate the actual waveform accessors used by the new adapter: unchanged
# black/white/gray pixels must receive no pulse; changed pairs retain vendor LUTs.
luts=r'''
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include "epdiy.h"
#include "e0470_epaper_waveform.h"
#include "e0470_waveform_trim.h"
#include "display_main_waveform.h"
void *heap_caps_malloc(size_t n,int caps){(void)caps;return malloc(n);}
static unsigned drive(const EpdWaveformPhases *p,int f,int to,int from){return(p->luts[f*64+to*4+from/4]>>(6-2*(from%4)))&3;}
int main(void){
 e0470_waveform_init();
 for(int m=0;m<2;m++){
  int mode=m?MODE_GL16:MODE_DU;
  const EpdWaveform *w=m?display_main_gray_waveform():display_main_bw_waveform();
  const EpdWaveformPhases *p=e0470_waveform_phases(w,mode),*v=e0470_waveform_phases(&E0470_WAVEFORM,mode);
  assert(p&&v&&p->phases==(m?48:20)&&p->phases==v->phases);
  for(int f=0;f<p->phases;f++)for(int to=0;to<16;to++)for(int from=0;from<16;from++)
   assert(drive(p,f,to,from)==(to==from?0:drive(v,f,to,from)));
 }
 puts("PASS actual DU20/GL48 LUTs: equal black/white/grays held; every changed pair retains native drive pulses");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'lut.c').write_text(luts)
 cmd=['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined','-DDPREADER_BOARD_METALIO47=1']
 for inc in ('main','tools/waveform_host_stubs','tools/shelf_waveform_host_stubs','components/epdiy/include','components/epdiy/src','components/e0470_epaper_waveform/include','components/e0470_epaper_waveform/waveforms','components/e0470_epaper_waveform/metalio'):cmd+=['-I'+str(root/inc)]
 cmd += [str(p/'lut.c'),str(root/'boards/metalio47/src/main/display_main_waveform.c'),str(root/'boards/metalio47/src/components/e0470_epaper_waveform/e0470_epaper_waveform.c'),str(root/'components/e0470_epaper_waveform/e0470_waveform_trim.c'),'-o',str(p/'lut')]
 subprocess.run(cmd,check=True);subprocess.run([str(p/'lut')],check=True)
