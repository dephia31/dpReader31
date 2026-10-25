#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
"""中文：真实渐进JPEG、硬解替身、缩放与失败清理；不声称硬件性能。
English: Real progressive JPEG, hardware fixtures, resampling and failure cleanup; no hardware performance claims.
"""
from pathlib import Path
import argparse, subprocess, tempfile
from PIL import Image
from home_cover_cache_host_test import function
R=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser();p.add_argument('--jpeg-source',type=Path,required=True);p.add_argument('--jpeg-build',type=Path,required=True)
a=p.parse_args()
unit=r'''
#include <assert.h>
#include <stdint.h>
#include "metalio_jpeg.c"
'''+function('book_cover_crop',R/'main/book/book_cover.c')+r'''
static uint8_t *load(const char *p,size_t *n){FILE *f=fopen(p,"rb");assert(f);fseek(f,0,SEEK_END);*n=ftell(f);rewind(f);uint8_t *b=malloc(*n);assert(fread(b,1,*n,f)==*n);fclose(f);return b;}
int main(int argc,char **argv){
 assert(argc==7);unsigned w,h,pw,ph;bool gray;
 uint8_t a[64],b[64],canary[64];memset(canary,0x91,sizeof(canary));
 size_t n;uint8_t *jpg=load(argv[1],&n);
 assert(jpeg_hw_shape(jpg,n,&w,&h,&pw,&ph,&gray)&&w==34&&h==28&&pw==48&&ph==32&&!gray);
 assert(metalio_jpeg_gray(jpg,n,NULL,8,8,a)&&!test_engines&&test_hw_calls==1);
 // 每个像素与相同路径的文件解码一致，MCU填充不成为图像的一部分。
 // File and memory routes match; MCU padding must never leak into the visible image.
 FILE *f=fopen(argv[1],"rb");assert(metalio_jpeg_gray(NULL,n,f,8,8,b));assert(!memcmp(a,b,64));fclose(f);
 f=fopen(argv[6],"rb");fseek(f,0,SEEK_END);size_t exif=ftell(f);rewind(f);
 assert(metalio_jpeg_gray(NULL,exif,f,8,8,b)&&!memcmp(a,b,64));fclose(f);
 // 纯红/蓝检查不能只测中性灰；彩色权重不能反转。
 // Red/blue checks expose swapped color channels that neutral gray cannot detect.
 free(jpg);jpg=load(argv[2],&n);assert(metalio_jpeg_gray(jpg,n,NULL,8,8,a));
 for(int i=0;i<64;i++)assert(a[i]>70&&a[i]<82);
 test_hw_error=-1;assert(!metalio_jpeg_gray(jpg,n,NULL,8,8,canary)&&!test_engines);test_hw_error=0;
 test_alloc_fail=test_allocs+1;assert(!metalio_jpeg_gray(jpg,n,NULL,8,8,canary)&&!test_engines);test_alloc_fail=0;
 test_alloc_fail=test_allocs+2;assert(!metalio_jpeg_gray(jpg,n,NULL,8,8,canary)&&!test_engines);test_alloc_fail=0;
 test_free=JPEG_RESERVE;assert(!metalio_jpeg_gray(jpg,n,NULL,8,8,canary));test_free=32u*1024u*1024u;
 free(jpg);jpg=load(argv[3],&n);assert(metalio_jpeg_gray(jpg,n,NULL,8,8,a));
 for(int i=0;i<64;i++)assert(a[i]>22&&a[i]<36);
 free(jpg);jpg=load(argv[4],&n);assert(jpeg_hw_shape(jpg,n,&w,&h,&pw,&ph,&gray)&&gray&&pw==40&&ph==32);
 assert(metalio_jpeg_gray(jpg,n,NULL,8,8,a));free(jpg);
 jpg=load(argv[5],&n);assert(!jpeg_hw_shape(jpg,n,&w,&h,&pw,&ph,&gray));
 assert(metalio_jpeg_gray(jpg,n,NULL,8,8,a));
 unsigned min=255,max=0;for(int i=0;i<64;i++){if(a[i]<min)min=a[i];if(a[i]>max)max=a[i];}assert(max-min>150);
 f=fopen(argv[5],"rb");assert(metalio_jpeg_gray(NULL,n,f,8,8,b)&&!memcmp(a,b,64));fclose(f);
 test_time_step=3000000;assert(!metalio_jpeg_gray(jpg,n,NULL,8,8,canary));test_time_step=0;
 assert(!metalio_jpeg_gray(jpg,n/3,NULL,8,8,canary));
 test_free=JPEG_RESERVE;assert(!metalio_jpeg_gray(jpg,n,NULL,8,8,canary));test_free=32u*1024u*1024u;free(jpg);
 for(int i=0;i<64;i++)assert(canary[i]==0x91);
 // 面积缩小保留平均灰度；常量放大和居中裁切不读边界外。
 // Area reduction preserves average gray; enlargement/cropping never reads outside the source.
 uint8_t checker[256];for(int y=0;y<16;y++)for(int x=0;x<16;x++)checker[y*16+x]=(x+y)%2?255:0;
 assert(jpeg_scale(checker,16,16,16,8,8,a));for(int i=0;i<64;i++)assert(a[i]==128);
 memset(checker,37,256);assert(jpeg_scale(checker,2,2,16,8,8,a));for(int i=0;i<64;i++)assert(a[i]==37);
 assert(!jpeg_scale(checker,1,1,0,8,8,a));assert(!jpeg_hw_shape((uint8_t*)"bad",3,&w,&h,&pw,&ph,&gray));
 assert(!metalio_jpeg_gray(NULL,20,NULL,8,8,a));assert(!test_engines);
 puts("PASS: real full-progressive file/memory decode; padded RGB/gray, red/blue order, area resampling, corruption/OOM/timeout fallback and resource cleanup (hardware mocked)");
}
'''
with tempfile.TemporaryDirectory() as d:
    d=Path(d)
    im=Image.new('RGB',(34,28));im.putdata([(x*7,y*9,(x+y)*4) for y in range(28) for x in range(34)])
    im.save(d/'padded.jpg',quality=95,subsampling=2)
    raw=(d/'padded.jpg').read_bytes();(d/'exif.jpg').write_bytes(raw[:2]+b'\xff\xe1\x13\x8a'+b'X'*5000+raw[2:])
    Image.new('RGB',(32,32),'red').save(d/'red.jpg',quality=100)
    Image.new('RGB',(32,32),'blue').save(d/'blue.jpg',quality=100)
    Image.new('L',(34,28),128).save(d/'gray.jpg',quality=100)
    im=Image.new('L',(8,8));im.putdata([255 if ((x+y)%2) else 0 for y in range(8) for x in range(8)])
    im.save(d/'progressive.jpg',progressive=True,quality=100)
    (d/'test.c').write_text(unit)
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-g','-fsanitize=address,undefined',
        '-I'+str(R/'tools/metalio_jpeg_stubs'),'-I'+str(R/'main/book'),'-I'+str(a.jpeg_source),'-I'+str(a.jpeg_build),
        str(d/'test.c'),str(a.jpeg_build/'libjpeg.a'),'-o',str(d/'test')],check=True)
    subprocess.run([str(d/'test'),*[str(d/n) for n in ('padded.jpg','red.jpg','blue.jpg','gray.jpg','progressive.jpg','exif.jpg')]],check=True)
