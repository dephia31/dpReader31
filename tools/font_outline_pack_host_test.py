#!/usr/bin/env python3
"""Compare maintained streamed glyf packing with native static TTF outlines."""
from pathlib import Path
import subprocess, tempfile, sys
from home_cover_cache_host_test import function
root=Path(__file__).resolve().parents[1]
source=root/'main/font/ttf_font.c'
unit=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#define TTF_TREE_MAX 64
#define TTF_GLYF_ARENA (128*1024)
#define ESP_LOGE(...) ((void)0)
static uint8_t *original,*font_data,*file_loca;
static int num_glyphs,packed_root=-1,current_weight=400,packed_weight=-1;
static bool loca_long,cff_outline;
static uint32_t work_loca_off,work_glyf_off,file_glyf_off;
static uint16_t be16(const uint8_t *p){return p[0]*256u+p[1];}
static uint32_t be32(const uint8_t *p){return (uint32_t)be16(p)*65536u+be16(p+2);}
static void put_be16(uint8_t *p,uint16_t x){p[0]=x>>8;p[1]=x;}
static void put_be32(uint8_t *p,uint32_t x){put_be16(p,x>>16);put_be16(p+2,x);}
static bool file_read_at(uint32_t off,void *dst,size_t n){memcpy(dst,original+off,n);return true;}
static bool apply_gvar_to_blob(uint8_t *p,uint32_t *n,size_t cap,int gid){(void)p;(void)n;(void)cap;(void)gid;return false;}
'''
for name in ('file_glyph_off','file_glyph_len','set_work_loca','enqueue_composite_children','pack_glyph_tree'):
    unit+=function(name,source)+'\n'
unit+=r'''
int main(int argc,char **argv){
    assert(argc==2);FILE *f=fopen(argv[1],"rb");assert(f);
    fseek(f,0,SEEK_END);size_t n=ftell(f);rewind(f);
    original=malloc(n);assert(fread(original,1,n,f)==n);fclose(f);
    stbtt_fontinfo native,stream;
    assert(stbtt_InitFont(&native,original,stbtt_GetFontOffsetForIndex(original,0)));
    assert(native.glyf>0);
    work_glyf_off=(n+3)&~3u;font_data=calloc(1,work_glyf_off+TTF_GLYF_ARENA);
    memcpy(font_data,original,n);
    assert(stbtt_InitFont(&stream,font_data,stbtt_GetFontOffsetForIndex(font_data,0)));
    stream.glyf=work_glyf_off;
    num_glyphs=native.numGlyphs;loca_long=native.indexToLocFormat!=0;
    work_loca_off=native.loca;file_loca=original+native.loca;file_glyf_off=native.glyf;
    float scale=stbtt_ScaleForPixelHeight(&native,73);
    for(int gid=0;gid<num_glyphs;++gid){
        assert(pack_glyph_tree(gid));
        int w,h,x,y,w2,h2,x2,y2;
        uint8_t *a=stbtt_GetGlyphBitmap(&native,scale,scale,gid,&w,&h,&x,&y);
        uint8_t *b=stbtt_GetGlyphBitmap(&stream,scale,scale,gid,&w2,&h2,&x2,&y2);
        assert(w==w2&&h==h2&&x==x2&&y==y2);
        assert(!w||!h||memcmp(a,b,w*h)==0);
        stbtt_FreeBitmap(a,NULL);stbtt_FreeBitmap(b,NULL);
    }
    printf("PASS static TTF streamed packing: %d glyphs at73px exactly match original font raster\n",num_glyphs);
    free(original);free(font_data);
}
'''
with tempfile.TemporaryDirectory() as directory:
    d=Path(directory);(d/'test.c').write_text(unit)
    subprocess.run(['cc','-std=c11','-O2','-I'+str(root/'main/font'),str(d/'test.c'),'-lm','-o',str(d/'test')],check=True)
    subprocess.run([str(d/'test'),sys.argv[1] if len(sys.argv)>1 else str(root/'main/assets/builtin.ttf')],check=True)
