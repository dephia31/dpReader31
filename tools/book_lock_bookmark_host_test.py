#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
# 中文：编译真实PNG行解码与透明书封，验证任意当前底图、alpha、黑色主体和原画布比例。
# English: Compile the real PNG row decoder and cover compositor; verify arbitrary current backgrounds, alpha, black subjects and canvas fit.
from pathlib import Path
import os,re,struct,subprocess,tempfile,zlib
from home_cover_cache_host_test import function
r=Path(__file__).resolve().parents[1]
c=r/'main/book/book_cover.c'
u=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <errno.h>
#include "png.h"
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
#define BYTES (684u*1216u/2u)
#define BOOK_IMAGE_FILE_MAX (50u*1024u*1024u)
typedef struct {int x,y,width,height;}EpdRect;
enum {EPD_ROT_PORTRAIT=1,EPD_ROT_INVERTED_PORTRAIT=2,EPD_ROT_INVERTED_LANDSCAPE=3};
static unsigned budget=8u*1024u*1024u;
static size_t heap_caps_get_free_size(unsigned flags){(void)flags;return budget;}
static void *heap_caps_malloc(size_t n,unsigned flags){(void)flags;return budget>=n?malloc(n):NULL;}
static int epd_get_rotation(void){return 0;}
static int epd_width(void){return 684;}
static int epd_height(void){return 1216;}
static uint8_t epd_get_pixel(int x,int y,int w,int h,const uint8_t *fb){assert(x>=0&&y>=0&&x<w&&y<h);int i=y*w+x;return ((fb[i/2]>>(i%2?4:0))&15)<<4;}
static void epd_draw_pixel(int x,int y,uint8_t gray,uint8_t *fb){assert(x>=0&&y>=0&&x<684&&y<1216);int i=y*684+x;fb[i/2]=(fb[i/2]& (i%2?15:240))|((gray>>4)<<(i%2?4:0));}

static void *cover_alloc(size_t n){return heap_caps_malloc(n,0);}
static void decode_yield(unsigned at){(void)at;}
typedef struct{unsigned x,y,width,height;}book_crop_t;
'''
for n in ('luminance','scale_axis'):u+=function(n,c)+'\n'
u+='typedef struct{uint16_t at;uint8_t frac;}scale_step_t;\n'
for n in ('scale_map','scale_next','bilinear4','book_cover_crop'):u+=function(n,c)+'\n'
u+='typedef struct{const uint8_t *data;size_t size,pos;FILE *file;}png_input_t;\n'
for n in ('png_read_mem','png_gray_input','book_png_file_layer'):u+=function(n,c)+'\n'
u+=r'''
static bool book_image_file_dimensions(const char *path,bool png,unsigned *w,unsigned *h){
 (void)png;uint8_t b[24];FILE *f=fopen(path,"rb");if(!f)return false;bool ok=fread(b,1,24,f)==24&&!memcmp(b,"\211PNG\r\n\032\n",8);fclose(f);
 *w=(uint32_t)b[16]<<24|(uint32_t)b[17]<<16|(uint32_t)b[18]<<8|b[19];*h=(uint32_t)b[20]<<24|(uint32_t)b[21]<<16|(uint32_t)b[22]<<8|b[23];return ok;
}
'''
source=(r/'main/book/book_lock_bookmark.c').read_text()
source=re.sub(r'^#include[^\n]*\n','',source,flags=re.M)
source=source.replace('#define ROOT "/sdcard/.read-pico-cache"','#define ROOT CACHE_ROOT')
u+=function("ui_image_dither_gray",r/"main/ui/ui_image_dither.c")+"\n"
u+=source
u+=r'''

static void check_white_regions(void){
 uint8_t gray[31*19],before[31*19],alpha[31*19],reach[31*19];unsigned queue[31*19],seed=47;
 // 和独立像素BFS对照，覆盖分岔、窄缝、封闭白岛及单行单列。
 // Compare against an independent pixel BFS across branches, narrow passages, enclosed islands and one-row/column inputs.
 for(unsigned trial=0;trial<400;++trial){
  unsigned w=trial%31+1,h=trial%19+1,n=w*h,head=0,tail=0;
  memset(alpha,255,n);memset(reach,0,n);
  for(unsigned at=0;at<n;++at){seed=seed*1664525u+1013904223u;gray[at]=(seed>>29)<5?255:(seed&1?249:80);}
  memcpy(before,gray,n);
  for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x){unsigned at=y*w+x;if((!x||!y||x+1==w||y+1==h)&&gray[at]>=250&&!reach[at]){reach[at]=1;queue[tail++]=at;}}
  while(head<tail){unsigned at=queue[head++],x=at%w,y=at/w,adj[4]={x?at-1:at,x+1<w?at+1:at,y?at-w:at,y+1<h?at+w:at};for(unsigned d=0;d<4;++d){unsigned next=adj[d];if(!reach[next]&&gray[next]>=250){reach[next]=1;queue[tail++]=next;}}}
  assert(remove_white_background(gray,alpha,w,h));
  for(unsigned at=0;at<n;++at){assert(alpha[at]==(reach[at]?0:255));assert(gray[at]==(reach[at]?0:before[at]));}
 }
 // 真实透明度不作颜色键，保留白色主体；内存不足不修改输入。
 // Existing alpha bypasses color-keying and preserves white art; allocation failure leaves inputs untouched.
 memset(gray,255,sizeof(gray));memset(alpha,255,sizeof(alpha));alpha[100]=128;
 assert(remove_white_background(gray,alpha,31,19)&&alpha[0]==255&&alpha[100]==128&&gray[0]==255);
 alpha[100]=255;budget=0;assert(!remove_white_background(gray,alpha,31,19));budget=8u*1024u*1024u;
 for(unsigned at=0;at<sizeof(gray);++at)assert(gray[at]==255&&alpha[at]==255);
 uint8_t *large=malloc(2*684u*1216u);assert(large);memset(large,255,2*684u*1216u);
 assert(remove_white_background(large,large+684u*1216u,684,1216));
 for(unsigned at=0;at<2*684u*1216u;++at)assert(!large[at]);free(large);
}
int main(int argc,char **argv){
 assert(argc>=4);check_white_regions();
 uint8_t *fb=malloc(BYTES),*original=malloc(BYTES);assert(fb&&original);memset(original,0xaa,BYTES);
 memcpy(fb,original,BYTES);
 assert(book_lock_bookmark_draw(fb,argv[2]));
 // 整张正方形PNG上下留白，黑色保留不透明；透明区保留原文字底图。
 // A square PNG has vertical canvas margins; black stays opaque, transparency preserves reading pixels.
 assert(pixel(fb,300,100)==170&&pixel(fb,300,1150)==170);
 assert(pixel(fb,80,350)==170);assert(pixel(fb,520,350)==0);
 assert(pixel(fb,80,800)>=187&&pixel(fb,80,800)<=221);assert(pixel(fb,520,800)==255);
 if(argc>=6){
  FILE *bg=fopen(argv[4],"rb");assert(bg&&fread(original,1,BYTES,bg)==BYTES);fclose(bg);
  memcpy(fb,original,BYTES);
  assert(book_lock_bookmark_draw(fb,argv[5]));
  FILE *out=fopen("preview.pgm","wb");assert(out);fprintf(out,"P5\n684 1216\n255\n");
  for(int y=0;y<1216;++y)for(int x=0;x<684;++x)fputc(pixel(fb,x,y),out);fclose(out);
 }
 // 首页、设置、阅读等任意页面直接作为底图；没有书籍文件或阅读记录前提。
 // Any Home/settings/reader frame works without a book file or reading-history prerequisite.
 for(unsigned shade=0;shade<16;++shade){
  memset(fb,shade*17u,BYTES);assert(book_lock_bookmark_draw(fb,argv[2]));
  assert(pixel(fb,300,100)==shade*17u&&pixel(fb,80,350)==shade*17u&&pixel(fb,520,350)==0);
 }
 // 不透明白底PNG：边缘显示当前页面，黑色外框及内部白色图案保持不透明。
 // Opaque white-background PNG: reveal the current page at the edges, retaining the black border and enclosed white artwork.
 memset(fb,0xaa,BYTES);assert(book_lock_bookmark_draw(fb,argv[3]));
 assert(pixel(fb,10,300)==170&&pixel(fb,100,400)==0&&pixel(fb,342,600)==255);
 memcpy(original,fb,BYTES);budget=0;
 assert(!book_lock_bookmark_draw(fb,argv[2])&&!memcmp(fb,original,BYTES));budget=8u*1024u*1024u;
 assert(!book_lock_bookmark_draw(fb,"file.jpg")&&!memcmp(fb,original,BYTES));
 char invalid[512];snprintf(invalid,sizeof(invalid),"%s/broken.png",argv[1]);
 FILE *bad=fopen(invalid,"wb"),*good=fopen(argv[2],"rb");assert(bad&&good);
 uint8_t header[24];assert(fread(header,1,sizeof(header),good)==sizeof(header));fclose(good);
 assert(fwrite(header,1,sizeof(header),bad)==sizeof(header));fclose(bad);
 assert(!book_lock_bookmark_draw(fb,invalid)&&!memcmp(fb,original,BYTES));
 free(fb);free(original);
 puts("PASS: real PNG alpha/premultiplication, opaque black, aspect-fit canvas, all 16 current-page backgrounds, 400 flood/BFS comparisons, white-edge removal with enclosed whites and original alpha preserved, low-memory fallback and no internal-RAM allocation");
}
'''
def chunk(k,b):return struct.pack('>I',len(b))+k+b+struct.pack('>I',zlib.crc32(k+b))
raw=b''.join(b'\0'+b''.join(bytes((255,255,255,0)) if x<16 and y<16 else bytes((0,0,0,255)) if y<16 else bytes((255,255,255,128)) if x<16 else bytes((255,255,255,255)) for x in range(32)) for y in range(32))
png=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',32,32,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(raw))+chunk(b'IEND',b'')
with tempfile.TemporaryDirectory() as d:
 d=Path(d);(d/'test.c').write_text(u);(d/'fixture.png').write_bytes(png)
 white_raw=b''.join(b'\0'+b''.join(bytes((0,0,0)) if 3<=x<=28 and 3<=y<=28 and not (9<=x<=22 and 9<=y<=22) else bytes((255,255,255)) for x in range(32)) for y in range(32))
 (d/'white.png').write_bytes(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',32,32,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(white_raw))+chunk(b'IEND',b''))
 vendor=r/'managed_components/espressif__libpng/libpng'
 if not vendor.exists():vendor=Path('/private/tmp/kiikoread-rc90-lock-build-20261011/managed_components/espressif__libpng/libpng')
 (d/'pnglibconf.h').write_bytes((vendor/'scripts/pnglibconf.h.prebuilt').read_bytes())
 names='png pngerror pngget pngmem pngpread pngread pngrio pngrtran pngrutil pngset pngtrans pngwrite pngwio pngwtran pngwutil'.split()
 subprocess.run(['cc','-std=gnu11','-O2','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-misleading-indentation','-fsanitize=address,undefined','-DPNG_ARM_NEON_OPT=0','-DPNG_INTEL_SSE_OPT=0','-DCACHE_ROOT="'+str(d/'cache')+'"','-I'+str(d),'-I'+str(vendor),str(d/'test.c')]+[str(vendor/(n+'.c')) for n in names]+['-lz','-lm','-o',str(d/'test')],check=True)
 args=[str(d/'test'),str(d),str(d/'fixture.png'),str(d/'white.png')]
 if os.environ.get('BOOKMARK_SAMPLE') and os.environ.get('BOOKMARK_BACKGROUND'):args += [os.environ['BOOKMARK_BACKGROUND'],os.environ['BOOKMARK_SAMPLE']]
 subprocess.run(args,check=True)
