#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
from pathlib import Path
import subprocess,os
os.chdir(Path(__file__).resolve().parents[1])
Path("build/home-card-audit").mkdir(parents=True,exist_ok=True)
ui=Path('main/apps/app_dashboard.c').read_text();driver=Path('boards/metalio47/src/components/epdiy/src/epdiy.c').read_text()
def fn(s,key):
 a=s.index(key);b=s.index('{',a);depth=1;i=b+1
 while depth:
  depth+=(s[i]=='{')-(s[i]=='}');i+=1
 return s[a:i]
code='''#include <stdint.h>
#include <assert.h>
#include <string.h>
typedef struct {int width,height;} EpdDisplay_t;
enum EpdRotation {EPD_ROT_LANDSCAPE,EPD_ROT_PORTRAIT,EPD_ROT_INVERTED_LANDSCAPE,EPD_ROT_INVERTED_PORTRAIT};
static EpdDisplay_t panel={12,20};static EpdDisplay_t *display=&panel;static enum EpdRotation display_rotation;
static const EpdDisplay_t *epd_get_display(void){return display;}
static enum EpdRotation epd_get_rotation(void){return display_rotation;}
'''+fn(driver,'void epd_draw_pixel(')+'\n'+fn(driver,'uint8_t epd_get_pixel(')+'\n'+fn(ui,'static uint8_t home_ink_at(')+'''
int main(void){uint8_t fb[120];for(int rot=0;rot<4;rot++){display_rotation=rot;int w=(rot&1)?20:12,h=(rot&1)?12:20;for(int y=0;y<h;y++)for(int x=0;x<w;x++){memset(fb,255,sizeof(fb));epd_draw_pixel(x,y,0x30,fb);assert(home_ink_at(fb,x,y)==0x30);if(x+1<w)assert(home_ink_at(fb,x+1,y)==0xf0);}}return 0;}
'''
p=Path('build/home-card-audit');(p/'rotation.c').write_text(code)
subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(p/'rotation.c'),'-o',str(p/'rotation')],check=True)
subprocess.run([str(p/'rotation')],check=True)
print('PASS actual S31 driver pixel writes/readback in all four rotations, both packed nibbles, white neighbours')
