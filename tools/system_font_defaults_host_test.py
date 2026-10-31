"""Run actual UI-size migration/persistence and large-font picker geometry."""
from pathlib import Path
import re,subprocess,tempfile
from home_cover_cache_host_test import function
from board_source_view import select_source
root=Path(__file__).resolve().parents[1];src=(root/'main/settings.c').read_text()
initial=re.search(r'static uint8_t s_system_size = \d+;',src)[0]
u=r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
typedef int nvs_handle_t;typedef int esp_err_t;
#define ESP_OK 0
#define NVS_READWRITE 1
#define NVS_NS "test"
#define NVS_KEY_SYS_SIZE "sys_size"
static int exists,stored,revision,pending,pending_revision,fault,calls,commits;
static int nvs_open(const char *n,int m,int *h){(void)n;(void)m;*h=1;calls=0;pending=stored;pending_revision=revision;return fault==1?-1:0;}
static int nvs_get_u8(int h,const char *n,uint8_t *v){assert(h==1);if(!strcmp(n,"sys_size_rev")){*v=(uint8_t)revision;return revision?0:-1;}assert(!strcmp(n,"sys_size"));*v=(uint8_t)stored;return exists?0:-1;}
static int nvs_set_u8(int h,const char *n,uint8_t v){assert(h==1);++calls;if(fault==calls+1)return -1;if(!strcmp(n,"sys_size_rev"))pending_revision=v;else{assert(!strcmp(n,"sys_size"));pending=v;}return 0;}
static int nvs_commit(int h){assert(h==1);if(fault==4)return -1;stored=pending;revision=pending_revision;exists=1;++commits;return 0;}
static void nvs_close(int h){assert(h==1);}
'''+initial+'\n'
for name in ('system_size_load','app_settings_system_font_size','app_settings_set_system_font_size'):
 u+=function(name,root/'main/settings.c')+'\n'
u+='static _Bool s_system_text_scale=1;\n'+function('ui_text_system_percent',root/'main/ui/ui_kit.c')+'\n'+function('ui_text_effective_px',root/'main/ui/ui_kit.c')+'\n'
for name in ('reader_ui_extra','reader_font_picker_top','bookmark_row_step'):
 u+=function(name,root/'main/apps/app_book.c')+'\n'
settings=select_source((root/'main/apps/app_device_settings.c').read_text(),'metalio47')
with tempfile.TemporaryDirectory() as d:
 p=Path(d);selected=p/'settings.c';selected.write_text(settings)
 u+='typedef struct {int x,y,width,height;} EpdRect;\n#define UI_NAV_TOP 1096\n'
 u+=re.search(r'typedef struct \{\s*EpdRect size, preview;.*?\} system_font_layout_t;',settings,re.S)[0]+'\n'
 for name in ('settings_viewport_end','system_font_layout','system_font_item_rect'):
  u+=function(name,selected)+'\n'
 u+=r'''
int main(void){
 assert(app_settings_system_font_size()==100);
 s_system_size=100;assert(ui_text_system_percent()==160);
 for(int px=12;px<=44;px+=2){int adjusted=px+2;assert(ui_text_effective_px(px)==(adjusted<46?(adjusted*160+50)/100:adjusted));}
 assert(reader_ui_extra(100)==60&&reader_font_picker_top()==270&&bookmark_row_step()==112);
 s_system_size=120;assert(ui_text_system_percent()==192);
 s_system_text_scale=0;assert(ui_text_effective_px(26)==28);s_system_text_scale=1;
 exists=1;stored=202;revision=1;system_size_load();assert(s_system_size==100&&stored==100&&revision==2);
 app_settings_set_system_font_size(160);assert(stored==160&&s_system_size==160);
 int before=commits;system_size_load();assert(s_system_size==160&&commits==before);
 for(int f=1;f<=4;f++){
  stored=202;revision=1;fault=f;system_size_load();assert(s_system_size==100&&stored==202&&revision==1);
  fault=0;system_size_load();assert(stored==100&&revision==2);
  app_settings_set_system_font_size(140);fault=f;app_settings_set_system_font_size(100);
  assert(s_system_size==140&&stored==140);fault=0;
 }
 for(unsigned n=0;n<256;n++){
  s_system_size=100;app_settings_set_system_font_size((uint8_t)n);
  if(n>=100&&n<=202&&(!(n%10)||n==144||n==202))assert(s_system_size==n);
  else assert(s_system_size==100);
 }
 stored=201;revision=2;system_size_load();assert(s_system_size==100);
 const unsigned scales[]={100,144,150,200,202};
 for(unsigned i=0;i<sizeof(scales)/sizeof(scales[0]);++i){
  s_system_size=scales[i];system_font_layout_t l=system_font_layout();
  assert(l.size.y+l.size.height<l.preview_title&&l.preview_title<l.preview.y);
  assert(l.preview.y+l.preview.height<l.font_title&&l.font_title<l.first);
  assert(l.slots>=2);
  for(int n=0;n<l.slots;n++){EpdRect r=system_font_item_rect(n);assert(r.y+r.height<UI_NAV_TOP-42);}
 }
 return 0;
}
'''
 (p/'test.c').write_text(u)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
# The size-grid paint and touch maps must offer identical choices within the viewport.
choices=re.findall(r'const uint8_t sizes\[\]=\{([^}]+)\}',settings)
assert len(choices)==2 and choices[0]==choices[1]
values=list(map(int,choices[0].split(',')));assert values[-1]==202 and len(values)==13
assert 298+((len(values)-1)//2)*110+96<1096
print('PASS: new100% equals old160% in actual raster scaling, body unchanged, effective reader geometry, one-time upgrade to100/user persistence, NVS faults and bounded font-picker hit maps')
