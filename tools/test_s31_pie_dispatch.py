#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-3.0-or-later
"""Check actual C dispatch/tails with a byte oracle; device tests verify PIE instructions."""
from pathlib import Path
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]
UNIT=r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
static unsigned mask_calls,diff_calls,checks;
int mock_core;
void epd_pie_mask_blocks(uint8_t* p,const uint8_t* m,unsigned blocks){
    assert(mock_core==1&&blocks&&!(((uintptr_t)p|(uintptr_t)m)&15));++mask_calls;
    for(unsigned i=0;i<blocks*16;++i)p[i]&=m[i];
}
uint32_t epd_pie_interlace_blocks(const uint8_t* t,const uint8_t* f,uint8_t* out,uint8_t* col,unsigned blocks){
    assert(mock_core==1&&blocks&&!(((uintptr_t)t|(uintptr_t)f|(uintptr_t)out|(uintptr_t)col)&15));++diff_calls;
    uint32_t dirty=0;
    for(unsigned x=0;x<blocks*32;++x){
        unsigned shift=4*(x&1),a=(t[x/2]>>shift)&15,b=(f[x/2]>>shift)&15;
        out[x]=(a<<4)|b;col[x/2]|=(a^b)<<shift;dirty|=a^b;
    }
    return dirty;
}
enum {N=1312};
static _Alignas(16) uint8_t t[N],f[N],got[2*N],want[2*N],col[N],cols[N],mask[N];
static unsigned seed=0xfaa735;
static unsigned random_word(void){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
static void test_diff(int pixels,int offset){
    memset(got,0xa5,sizeof(got));memcpy(want,got,sizeof(got));
    memset(col,0x21,sizeof(col));memcpy(cols,col,sizeof(col));bool dirty=false;
    for(int x=0;x<pixels;++x){unsigned shift=4*(x&1),a=(t[offset+x/2]>>shift)&15,b=(f[offset+x/2]>>shift)&15;
        want[offset+x]=(a<<4)|b;cols[offset+x/2]|=(a^b)<<shift;dirty|=a!=b;}
    unsigned before=diff_calls;
    assert(!!epd_interlace_4bpp_line_VE(t+offset,f+offset,got+offset,col+offset,pixels)==dirty);
    assert(!memcmp(got,want,sizeof(got))&&!memcmp(col,cols,sizeof(col)));
    assert(diff_calls-before==(unsigned)(mock_core==1&&epd_pie_pixels_enabled()&&pixels>=32&&offset==0));++checks;
}
static void test_mask(int bytes,int offset,int moffset){
    memcpy(got,t,N);memcpy(want,got,sizeof(got));
    for(int x=0;x<bytes;++x)want[offset+x]&=mask[moffset+x];
    unsigned before=mask_calls;epd_apply_line_mask_VE(got+offset,mask+moffset,bytes);
    assert(!memcmp(got,want,sizeof(got)));
    assert(mask_calls-before==(unsigned)(mock_core==1&&epd_pie_pixels_enabled()&&bytes>=16&&offset==0&&moffset==0));++checks;
}
int main(void){
    for(int i=0;i<N;++i){t[i]=random_word();f[i]=random_word();mask[i]=random_word();}
    for(mock_core=0;mock_core<=1;++mock_core)for(int enable=0;enable<2;++enable){
        epd_pie_pixels_enable(enable);
        for(int offset=0;offset<16;++offset){
            for(int n=0;n<260;++n){test_diff(n,offset);test_mask(n,offset,offset);}
            test_diff(1216,offset);test_diff(1215,offset);test_mask(304,offset,(offset+7)&15);test_mask(1216,offset,offset);
        }
        assert(epd_interlace_4bpp_line_VE(NULL,f,got,col,32)==0);
        epd_apply_line_mask_VE(NULL,mask,32);epd_apply_line_mask_VE(got,NULL,32);
    }
    mock_core=1;epd_pie_pixels_enable(true);
    for(int a=0;a<16;++a)for(int b=0;b<16;++b){memset(t,a*17,N);memset(f,b*17,N);test_diff(64,0);}
    assert(mask_calls&&diff_calls);
    printf("PASS: %u C-dispatch/oracle cases,Core0 exclusion,Core1 selection,toggles,16 alignments,odd/tail/full-panel pixels,canaries and all gray pairs; assembly requires device verification\n",checks);
}
'''

with tempfile.TemporaryDirectory() as folder:
    r=Path(folder)
    (r/'esp_cpu.h').write_text('#pragma once\nextern int mock_core;\nstatic inline int esp_cpu_get_core_id(void){return mock_core;}\n')
    (r/'esp_attr.h').write_text('#define IRAM_ATTR\n')
    source=(ROOT/'components/epdiy/src/riscv_pixels.c').read_text()
    source=source.split('#if defined(CONFIG_IDF_TARGET_ESP32S31)\n#include "output_common/lut.h"')[0]
    (r/'unit.c').write_text(source+UNIT)
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-unknown-attributes',
                    '-fsanitize=address,undefined','-DCONFIG_IDF_TARGET_ESP32S31=1','-DKIIKO_LOCAL_LATENCY=1',
                    '-I'+str(r),'-I'+str(ROOT/'boards/metalio47/src/components/epdiy/src'),
                    str(r/'unit.c'),'-o',str(r/'test')],check=True)
    subprocess.run([str(r/'test')],check=True)
