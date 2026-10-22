#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-3.0-or-later
"""Check the actual S31 pre-positioned LUT builder/feeder against raw phase bytes."""
from pathlib import Path
import subprocess,tempfile,re
from home_cover_cache_host_test import function as original_function
def function(name,path):
 with tempfile.TemporaryDirectory() as folder:
  normalized=Path(folder)/'source.c'
  normalized.write_text('\n'.join(line.lstrip() for line in re.sub(r'__attribute__\(\(optimize\("O3"\)\)\)', '', path.read_text()).splitlines()))
  return original_function(name,normalized)
r=Path(__file__).resolve().parents[1]
prefix=r'''
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#define IRAM_ATTR
static bool enabled;
bool epd_pipeline_enabled(void){return enabled;}
typedef struct{const uint8_t*luts;}EpdWaveformPhases;
'''
src=prefix+function('epd_build_1ppB_lut_1k',r/'boards/metalio47/src/components/epdiy/src/output_common/lut.c')+'\n'+function('calc_epd_input_1ppB_1k_S3_VE_aligned',r/'components/epdiy/src/riscv_pixels.c')+r'''
static unsigned seed=0x7e3e17;
static unsigned next(void){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
int main(void){
 uint8_t phases[64*64],lut[1024];_Alignas(16) uint8_t input[1216],output[320],expected[320];unsigned checks=0;
 for(unsigned i=0;i<sizeof(phases);++i){phases[i]=next();}
 EpdWaveformPhases p={phases};
 for(int frame=0;frame<64;++frame){
  epd_build_1ppB_lut_1k(lut,&p,frame);
  for(unsigned v=0;v<256;++v){unsigned action=(phases[frame*64+(v>>4)*4+(v&15)/4]>>(6-2*(v&3)))&3;
   for(unsigned pos=0;pos<4;++pos)assert(lut[4*v+pos]==(action<<(2*pos)));}
  for(int enabled_test=0;enabled_test<2;++enabled_test){enabled=enabled_test;
   for(unsigned width=0;width<=1216;width+=4){
    memset(output,0xa5,sizeof(output));memcpy(expected,output,sizeof(output));
    for(unsigned i=0;i<width;++i)input[i]=next();
    for(unsigned i=0;i<width/4;++i){unsigned value=0;for(unsigned x=0;x<4;++x){unsigned v=input[i*4+x];unsigned action=(phases[frame*64+(v>>4)*4+(v&15)/4]>>(6-2*(v&3)))&3;value|=action<<(6-2*x);}expected[i]=value;}
    calc_epd_input_1ppB_1k_S3_VE_aligned((uint32_t*)input,output,lut,width);
    assert(!memcmp(output,expected,sizeof(output)));++checks;
   }
  }
 }
 printf("PASS: %u actual scalar/pre-positioned LUT cases,64 raw phases,all transitions/positions,lengths0..1216,canaries\n",checks);
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(src)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-unknown-attributes','-DCONFIG_IDF_TARGET_ESP32S31=1','-DESP_PLATFORM=1','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
