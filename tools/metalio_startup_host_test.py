#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
"""Render the supplied flash-resident logo and real built-in caption; verify pixels and bounds."""
from pathlib import Path
import argparse, ast, subprocess, tempfile, struct, zlib, sys
from home_cover_cache_host_test import function
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--output',type=Path);args=p.parse_args()
BOOT=ROOT/'boards/metalio47/src/main/app_main.c'
if not BOOT.exists():BOOT=ROOT/'main/app_main.c'
with tempfile.TemporaryDirectory() as folder:
 work=Path(folder)
 (work/'esp_heap_caps.h').write_text('''#pragma once
#include <stddef.h>
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
void *heap_caps_malloc(size_t n,int caps);
void *heap_caps_calloc(size_t n,size_t bytes,int caps);
''')
 (work/'miniz.h').write_text('''#pragma once
#include <stddef.h>
#include <zlib.h>
typedef struct {int state;}tinfl_decompressor;typedef int tinfl_status;
#define TINFL_STATUS_DONE 0
#define TINFL_FLAG_PARSE_ZLIB_HEADER 1
#define TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF 4
#define tinfl_init(s) ((s)->state=0)
static inline int tinfl_decompress(tinfl_decompressor *s,const unsigned char *in,size_t *n,unsigned char *base,unsigned char *out,size_t *m,int flags){(void)s;(void)base;(void)flags;uLongf len=*m;int result=uncompress(out,&len,in,*n);*m=len;return result==Z_OK?0:-1;}
''')
 assets='#include <stdint.h>\n'
 for name,symbol in [('builtin.ttf','_binary_builtin_ttf'),('ui-hanzi.bin','_binary_ui_hanzi_bin'),('startup_logo_4bpp.bin','_binary_startup_logo_4bpp_bin')]:
  raw=(ROOT/'main/assets'/name).read_bytes();var=name.replace('.','_').replace('-','_')
  if name=='startup_logo_4bpp.bin':
   assets+='const uint8_t startup_logo_start[]={'+','.join(map(str,raw))+'};\n'
   continue
  assets+=f'const uint8_t {var}[] asm("{symbol}_start")={{'+','.join(map(str,raw))+'};\n'
  assets+=f'asm(".globl {symbol}_end\\n.set {symbol}_end, {symbol}_start + {len(raw)}");\n'
 (work/'assets.c').write_text(assets)
 unit=r'''
#include "ui_font.h"
#include "startup_logo.h"
extern const uint8_t startup_logo_start[DPREADER_STARTUP_LOGO_WIDTH*DPREADER_STARTUP_LOGO_HEIGHT/2];
#define startup_logo_end (startup_logo_start+sizeof(startup_logo_start))
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint8_t pixels[684*1216],first[684*1216];
static size_t allocated;static unsigned clears,painted;static int minx=684,maxx=-1,miny=1216,maxy=-1;
void *heap_caps_malloc(size_t n,int caps){assert(caps==3);allocated+=n;return malloc(n);}
void *heap_caps_calloc(size_t n,size_t bytes,int caps){assert(caps==3);allocated+=n*bytes;return calloc(n,bytes);}
int epd_rotated_display_width(void){return 684;}int epd_rotated_display_height(void){return 1216;}
static void ui_clear_page(uint8_t *fb){memset(fb,255,sizeof(pixels));++clears;}
void epd_draw_pixel(int x,int y,uint8_t color,uint8_t *fb){assert(x>=0&&x<684&&y>=0&&y<1216);assert(y>=490&&y<750);fb[y*684+x]=(color>>4)*17u;++painted;if(x<minx)minx=x;if(x>maxx)maxx=x;if(y<miny)miny=y;if(y>maxy)maxy=y;}
'''+function('draw_startup_frame',BOOT)+r'''
int main(int argc,char **argv){
 assert(argc==2);
 // 第一帧在未加载任何外部字体时完成。/ Cold drawing uses the actual embedded font without a selected external face.
 draw_startup_frame(pixels);assert(clears==1&&painted>1500&&minx>=72&&maxx<612&&miny>=494&&maxy<750);
 const int left=342-DPREADER_STARTUP_LOGO_WIDTH/2,top=608-DPREADER_STARTUP_LOGO_HEIGHT/2-25;
 for(int y=0;y<DPREADER_STARTUP_LOGO_HEIGHT;++y)for(int x=0;x<DPREADER_STARTUP_LOGO_WIDTH;++x){
  uint8_t packed=startup_logo_start[(y*DPREADER_STARTUP_LOGO_WIDTH+x)/2];
  uint8_t gray=x&1?packed&15:packed>>4;
  assert(pixels[(top+y)*684+left+x]==gray*17);
 }
 size_t warm=allocated;memcpy(first,pixels,sizeof(pixels));
 draw_startup_frame(pixels);assert(allocated==warm&&!memcmp(first,pixels,sizeof(pixels)));
 assert(allocated<128*1024);unsigned middle=0;
 for(size_t i=0;i<sizeof(pixels);++i){if(pixels[i]!=0&&pixels[i]!=255)++middle;}
 assert(middle>50);FILE *file=fopen(argv[1],"wb");assert(file&&fwrite(pixels,1,sizeof(pixels),file)==sizeof(pixels));fclose(file);
 printf("PASS: supplied dpReader logo matches every packed asset pixel; centered bounds, original caption and warm cache reuse, PSRAM allocations=%zu bytes\n",allocated);
}
'''
 (work/'test.c').write_text(unit)
 subprocess.run(['cc','-std=gnu11','-Wall','-Wextra','-Werror','-Wno-unused-function',('-fsanitize=undefined' if sys.platform=='darwin' else '-fsanitize=address,undefined'),'-I'+str(work),'-I'+str(ROOT/'main/font'),'-I'+str(ROOT/'main/assets'),'-I'+str(ROOT/'tools/ui_gesture_stubs'),str(work/'test.c'),str(work/'assets.c'),str(ROOT/'main/font/ui_font.c'),str(ROOT/'main/font/ui_hanzi.c'),'-lz','-o',str(work/'test')],check=True)
 subprocess.run([str(work/'test'),str(work/'preview.raw')],check=True)
 if args.output:
  raw=(work/'preview.raw').read_bytes();assert len(raw)==684*1216
  def chunk(kind,data):return struct.pack('>I',len(data))+kind+data+struct.pack('>I',zlib.crc32(kind+data)&0xffffffff)
  rows=b''.join(b'\0'+raw[y*684:(y+1)*684] for y in range(1216))
  png=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',684,1216,8,0,0,0,0))+chunk(b'IDAT',zlib.compress(rows,9))+chunk(b'IEND',b'')
  args.output.parent.mkdir(parents=True,exist_ok=True);args.output.write_bytes(png)
  print('Native firmware preview:',args.output.resolve(),'PNG bytes:',len(png))
