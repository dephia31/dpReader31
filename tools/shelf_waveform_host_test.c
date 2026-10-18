/* SPDX-License-Identifier: Apache-2.0
 * 真实书架派生表的分配失败、重试和固定预算。/ Real shelf LUT allocation failures, retries and bounded memory.
 */
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include "display_main_waveform.h"
#include "e0470_epaper_waveform.h"
static int fail_at,attempts;
static size_t allocated;
void* heap_caps_malloc(size_t size,int caps){
    assert(caps==3&&size==48*16*4);++attempts;
    if(fail_at<0||attempts==fail_at)return NULL;
    void* p=malloc(size);assert(p);allocated+=size;return p;
}
int main(int argc,char** argv){
    assert(argc==2);fail_at=atoi(argv[1]);e0470_waveform_init();
    for(int pass=0;pass<3;++pass){
        const EpdWaveform* soft=display_main_shelf_soft_waveform();
        const EpdWaveform* art=display_main_shelf_waveform(MODE_GL16);
        const EpdWaveform* gc=display_main_shelf_waveform(MODE_GC16);
        if(fail_at<0){
            assert(soft==display_main_gray_waveform());assert(art==&E0470_FULL_WAVEFORM&&gc==&E0470_FULL_WAVEFORM);assert(!allocated);continue;
        }
        if(pass==2){
            assert(soft!=display_main_gray_waveform()&&art!=&E0470_WAVEFORM&&gc!=&E0470_WAVEFORM);
            assert(allocated==3*48*16*4);
            int before=attempts;
            assert(soft==display_main_shelf_soft_waveform());assert(art==display_main_shelf_waveform(MODE_GL16));assert(gc==display_main_shelf_waveform(MODE_GC16));assert(before==attempts);
        }
    }
    printf("shelf waveform fail=%d: safe fallback/retry, stable cache, <=9216B, no per-input allocations passed\n",fail_at);
    return 0;
}
