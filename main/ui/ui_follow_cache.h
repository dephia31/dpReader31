/* SPDX-License-Identifier: Apache-2.0
 * 中文：倒置竖屏列表的一位黑白缓存，无分配，只读写列表区域。
 * English: Allocation-free one-bit list cache for inverted portrait; fixed chrome stays untouched.
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static inline bool ui_follow_cache_valid(const uint8_t *cache, const uint8_t *fb,
    int width, int height, int top, int bottom, int extent, int offset) {
    return cache && fb && width > 0 && !(width & 1) && height > 0 && top >= 0 &&
        bottom <= width && bottom > top && !(top & 1) && !(bottom & 1) &&
        offset >= 0 && extent >= bottom-top && offset <= extent-(bottom-top);
}
static inline bool ui_follow_cache_capture(uint8_t *cache, const uint8_t *fb,
    int width, int height, int top, int bottom, int extent, int offset) {
    if (!ui_follow_cache_valid(cache,fb,width,height,top,bottom,extent,offset)) return false;
    const size_t stride = ((size_t)extent+7)/8;
    for (int row=0;row<height;++row) {
        uint8_t *out=cache+(size_t)row*stride;
        const uint8_t *in=fb+(size_t)row*(width/2)+top/2;
        for (int x=0;x<bottom-top;x+=8) {
            unsigned count=(unsigned)(bottom-top-x); if(count>8)count=8;
            unsigned bits=0;
            for(unsigned n=0;n<count;n+=2) {
                uint8_t pair=in[(x+(int)n)/2];
                bits|=((pair>>3)&1u)<<n; bits|=((pair>>7)&1u)<<(n+1);
            }
            unsigned at=(unsigned)(offset+x),shift=at&7;
            unsigned mask=((1u<<count)-1)<<shift, value=bits<<shift;
            out[at/8]=(out[at/8]&~mask)|value;
            if(shift+count>8)out[at/8+1]=(out[at/8+1]&~(mask>>8))|(value>>8);
        }
    }
    return true;
}
static inline bool ui_follow_cache_blit(const uint8_t *cache, uint8_t *fb,
    int width, int height, int top, int bottom, int extent, int offset) {
    if (!ui_follow_cache_valid(cache,fb,width,height,top,bottom,extent,offset)) return false;
    static const uint8_t pair[4]={0x00,0x0f,0xf0,0xff};
    static const uint16_t four[16]={0,15,240,255,3840,3855,4080,4095,61440,61455,61680,61695,65280,65295,65520,65535};
    const size_t stride=((size_t)extent+7)/8;
    for (int row=0;row<height;++row) {
        const uint8_t *in=cache+(size_t)row*stride;
        uint8_t *out=fb+(size_t)row*(width/2)+top/2;
        for (int x=0;x<bottom-top;x+=8) {
            unsigned at=(unsigned)(offset+x),shift=at&7;
            unsigned count=(unsigned)(bottom-top-x); if(count>8)count=8;
            unsigned bits=in[at/8]>>shift;
            if (shift+count>8) bits|=(unsigned)in[at/8+1]<<(8-shift);
            if(count==8){uint32_t word=(uint32_t)four[bits&15]|((uint32_t)four[(bits>>4)&15]<<16);memcpy(out+x/2,&word,sizeof(word));}
            else for(unsigned n=0;n<count;n+=2)out[(x+(int)n)/2]=pair[(bits>>n)&3];
        }
    }
    return true;
}
