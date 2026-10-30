#!/usr/bin/env python3
"""Actual glyph raster/cache paths: packed mono, AA separation, fallback, OOM."""
from pathlib import Path
import subprocess,tempfile,sys,re
from home_cover_cache_host_test import function
from font_mono_host_test import FLAGS,SOURCES,COMP,ROOT
source=ROOT/'main/font/ttf_font.c';text=source.read_text()
unit=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "ttf_mono.h"
#include "ttf_stroke_filter.h"
#include "vn_diacritics.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
#define KIIKO_BOARD_METALIO47 1
#define TTF_CACHE_LIMIT (1024*1024)
#define TTF_CACHE_BUCKETS 256
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
static bool s_antialias,admit=true,force_fallback,fail_alloc;
static int current_weight=400,s_stroke_adjust;
static stbtt_fontinfo font_info;
'''
a=text.index('typedef struct glyph_entry {');b=text.index('} glyph_entry_t;',a)+len('} glyph_entry_t;');unit+=text[a:b]+'\n'
unit+=r'''
static bool font_ready=true,bench_on;
static struct{uint64_t glyphs,hits,misses,raster_us;}bench;
static int64_t esp_timer_get_time(void){return 0;}
static void warm_runs(int px,const char *s){(void)px;(void)s;}
static void run_apply(size_t offset){(void)offset;}
static glyph_entry_t *cache_buckets[256];
static size_t cache_bytes,s_cache_total,reads;
static uint8_t *data;static size_t length;static ttf_mono_face_t *face;
static bool read_font(void*ctx,uint32_t off,void*dst,size_t n){(void)ctx;++reads;if((size_t)off>length||n>length-off)return false;memcpy(dst,data+off,n);return true;}
static bool mono_bitmap(uint32_t cp,float scale,ttf_mono_bitmap_t*b){return !force_fallback&&ttf_mono_render_stroke(face,cp,scale,-1,s_stroke_adjust,b);}
static bool cache_reserve(size_t bytes){(void)bytes;return admit;}
static void lru_touch(glyph_entry_t *entry){(void)entry;}
static bool pack_glyph_tree(int glyph){(void)glyph;return true;}
static void *heap_caps_malloc(size_t n,int caps){(void)caps;return fail_alloc?NULL:malloc(n);}
static void *heap_caps_calloc(size_t n,size_t size,int caps){(void)caps;return fail_alloc?NULL:calloc(n,size);}
static void heap_caps_free(void*p){free(p);}
'''
for name in ('glyph_alpha','cache_bucket','cache_lookup','rasterize_glyph','clamp_px','decode_utf8','get_glyph','ttf_measure_line_px'):
 unit+=function(name,source)+'\n'
unit+=r'''
static void clean(void){for(int i=0;i<256;++i){glyph_entry_t *g=cache_buckets[i];while(g){glyph_entry_t*n=g->hash_next;free(g->bitmap);free(g);g=n;}cache_buckets[i]=NULL;}cache_bytes=s_cache_total=0;}
static long ink(glyph_entry_t*g){long n=0;for(int y=0;y<g->height;++y)for(int x=0;x<g->width;++x)n+=glyph_alpha(g,x,y)==255;return n;}
static void check_binary(glyph_entry_t*g){assert(g&&g->mono_mode&&g->packed_mono);for(int y=0;y<g->height;++y)for(int x=0;x<g->width;++x)assert(glyph_alpha(g,x,y)==0||glyph_alpha(g,x,y)==255);assert(g->bitmap_bytes==((g->width+7)/8)*(size_t)g->height);}
int main(int argc,char**argv){assert(argc==2);FILE*f=fopen(argv[1],"rb");assert(f);fseek(f,0,SEEK_END);length=ftell(f);rewind(f);data=malloc(length);assert(fread(data,1,length,f)==length);fclose(f);
 assert(stbtt_InitFont(&font_info,data,stbtt_GetFontOffsetForIndex(data,0)));face=ttf_mono_open(read_font,NULL,length);assert(face);
 glyph_entry_t *mono=rasterize_glyph('m',73);check_binary(mono);
 size_t before=reads,used=s_cache_total;assert(cache_lookup('m',73)==mono);assert(reads==before&&s_cache_total==used);
 s_antialias=true;assert(!cache_lookup('m',73));glyph_entry_t*gray=rasterize_glyph('m',73);assert(gray&&!gray->mono_mode&&!gray->packed_mono&&gray->advance_x==mono->advance_x);
 bool intermediate=false;for(int y=0;y<gray->height;++y)for(int x=0;x<gray->width;++x){uint8_t a=glyph_alpha(gray,x,y);intermediate|=a>0&&a<255;}assert(intermediate);
 assert(mono->layout_top==gray->layout_top&&mono->layout_height==gray->layout_height&&mono->layout_left==gray->layout_left);
 s_antialias=false;assert(cache_lookup('m',73)==mono);
 for(s_stroke_adjust=-32;s_stroke_adjust<=32;++s_stroke_adjust){glyph_entry_t*g=rasterize_glyph('n',73);check_binary(g);assert(g->advance_x==rasterize_glyph('n',73)->advance_x);}
 long counts[5];int levels[]={-16,-4,0,4,16};
 for(int i=0;i<5;++i){s_stroke_adjust=levels[i];counts[i]=0;for(const char*p="mnosea";*p;++p){glyph_entry_t*g=rasterize_glyph(*p,73);check_binary(g);counts[i]+=ink(g);}}
 assert(counts[0]<counts[1]&&counts[1]<counts[2]&&counts[2]<counts[3]&&counts[3]<counts[4]);
 printf("PASS signed fine stroke ink: %ld < %ld < %ld < %ld < %ld\n",counts[0],counts[1],counts[2],counts[3],counts[4]);
 s_stroke_adjust=0;force_fallback=true;glyph_entry_t*fallback=rasterize_glyph('o',73);check_binary(fallback);
 fail_alloc=true;assert(!rasterize_glyph('p',73));fail_alloc=false;
 admit=false;assert(!rasterize_glyph('q',73));admit=true;
 force_fallback=false;s_antialias=true;int old_above,old_below,new_above,new_below;
 ttf_measure_line_px(73,"Trái tim của Tiểu Trương vẫn đang đập.",&old_above,&old_below);
 s_antialias=false;ttf_measure_line_px(73,"Trái tim của Tiểu Trương vẫn đang đập.",&new_above,&new_below);
 assert(old_above==new_above&&old_below==new_below);
 assert(mono->bitmap_bytes<gray->bitmap_bytes/4);clean();ttf_mono_close(face);assert(!ttf_mono_bytes());free(data);
 puts("PASS actual raster/cache: hinted1bpp,all65strokelevels,AA-key separation,stableadvances,cachehits noIO,native fallback,OOM/rejection/cleanup");}
'''
with tempfile.TemporaryDirectory() as directory:
 p=Path(directory);(p/'test.c').write_text(unit)
 subprocess.run(['cc',*FLAGS,str(p/'test.c'),str(ROOT/'main/font/ttf_mono.c'),*[str(COMP/'vendor/src'/s) for s in SOURCES],'-lm','-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test'),sys.argv[1] if len(sys.argv)>1 else str(ROOT/'main/assets/builtin.ttf')],check=True)
