#!/usr/bin/env python3
# SPDX-License-Identifier: LGPL-3.0-or-later
"""中文：验证真实S31行队列的内部RAM指针表及环绕。/ English: Verify actual S31 internal pointer allocation and ring wrap."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1]
unit=r'''
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "line_queue.h"
static unsigned tables,rows;
void*heap_caps_calloc(size_t n,size_t size,unsigned caps){assert(n==128&&size==sizeof(uint8_t*)&&caps==3);++tables;return calloc(n,size);}
void*heap_caps_aligned_alloc(size_t align,size_t bytes,unsigned caps){assert(align==16&&bytes==304&&caps==1);++rows;return aligned_alloc(align,bytes);}
void heap_caps_free(void*p){free(p);}
int main(void){
 LineQueue_t q=lq_init(128,304);assert(tables==1&&rows==128);uint8_t out[320];
 for(int loop=0;loop<20;++loop){
  for(int i=0;i<127;++i){uint8_t*p=lq_current(&q);assert(p&&!((uintptr_t)p&15));memset(p,i+loop,304);lq_commit(&q);}assert(!lq_current(&q));
  for(int i=0;i<127;++i){memset(out,0xa5,320);assert(!lq_read(&q,out));for(int x=0;x<320;++x)assert(out[x]==(x<304?(uint8_t)(i+loop):0xa5));}assert(lq_read(&q,out)==-1);
 }
 lq_reset(&q);assert(lq_read(&q,out)==-1&&lq_current(&q));lq_free(&q);
 puts("PASS: actual S31 internal-RAM pointer-sized table,128 aligned line buffers,2540 ring copies/full/empty/wrap/reset and output canaries");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'esp_attr.h').write_text('#define IRAM_ATTR\n');(p/'sdkconfig.h').write_text('#define KIIKO_BOARD_METALIO47 1\n#define CONFIG_IDF_TARGET_ESP32S31 1\n')
 (p/'esp_heap_caps.h').write_text('#include <stddef.h>\n#define MALLOC_CAP_INTERNAL 1\n#define MALLOC_CAP_8BIT 2\nvoid*heap_caps_calloc(size_t,size_t,unsigned);\nvoid*heap_caps_aligned_alloc(size_t,size_t,unsigned);\nvoid heap_caps_free(void*);\n')
 (p/'test.c').write_text(unit)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-DCONFIG_IDF_TARGET_ESP32S31=1','-fsanitize=address,undefined','-I'+str(p),'-I'+str(r/'components/epdiy/src/output_common'),str(p/'test.c'),str(r/'components/epdiy/src/output_common/line_queue.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
