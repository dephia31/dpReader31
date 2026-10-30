#!/usr/bin/env python3
"""Exercise the actual hinted gray raster, tone mapping and bounded font lifetime."""
from pathlib import Path
import subprocess,tempfile,sys
from font_mono_host_test import FLAGS,SOURCES,COMP,ROOT
UNIT=r'''
#include "ttf_mono.h"
#include "ttf_aa_tone.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
static unsigned char *data;static size_t length;static bool fail;
static bool read_font(void*c,uint32_t off,void*out,size_t n){(void)c;if(fail||off>length||n>length-off)return false;memcpy(out,data+off,n);return true;}
static unsigned decode(const char**p){unsigned a=(unsigned char)*(*p)++;if(a<128)return a;unsigned n=(a&224)==192?1:(a&240)==224?2:3;unsigned v=a&((1u<<(6-n))-1);while(n--)v=(v<<6)|((unsigned char)*(*p)++&63);return v;}
static unsigned char canvas[684*520];
static void line(stbtt_fontinfo*info,ttf_mono_face_t*face,int base,const char*text,bool hinted,int px){int pen=20;float scale=stbtt_ScaleForPixelHeight(info,px);while(*text){unsigned cp=decode(&text);int w,h,l,t,advance;unsigned char*allocated=NULL;const unsigned char*b;int pitch;
 if(hinted){ttf_mono_bitmap_t bm;assert(ttf_gray_render_stroke(face,cp,scale,-1,0,&bm));w=bm.width;h=bm.height;l=bm.left;t=-bm.top;b=bm.pixels;pitch=bm.pitch;}
 else{allocated=stbtt_GetCodepointBitmap(info,scale,scale,cp,&w,&h,&l,&t);b=allocated;pitch=w;}
 for(int y=0;y<h;++y)for(int x=0;x<w;++x){int dx=pen+l+x,dy=base+t+y;if(dx<0||dx>=684||dy<0||dy>=520)continue;unsigned alpha=b[y*pitch+x];unsigned cov=hinted?ttf_aa_coverage(alpha):(unsigned)lroundf(255*sqrtf(alpha/255.f));unsigned ink=255-cov;if(ink<canvas[dy*684+dx])canvas[dy*684+dx]=ink;}
 stbtt_GetCodepointHMetrics(info,cp,&advance,NULL);pen+=(int)lroundf(advance*scale);free(allocated);}}
int main(int argc,char**argv){assert(argc>=2);FILE*f=fopen(argv[1],"rb");assert(f);fseek(f,0,SEEK_END);length=ftell(f);rewind(f);data=malloc(length);assert(fread(data,1,length,f)==length);fclose(f);stbtt_fontinfo info;assert(stbtt_InitFont(&info,data,0));
 unsigned black=0,gray=0,glyphs=0;for(int a=0;a<256;++a){unsigned c=ttf_aa_coverage(a);assert(c==(unsigned)a);if(a)assert(c>=ttf_aa_coverage(a-1));}assert(!ttf_aa_coverage(0)&&ttf_aa_coverage(255)==255);
 for(int cycle=0;cycle<3;++cycle){ttf_mono_face_t*face=ttf_mono_open(read_font,NULL,length);assert(face);
 for(int px=18;px<=90;++px){const char*p="HimnoruĐường nét chữ tiếng Việt ẮẰẨỄỘỸ";while(*p){unsigned cp=decode(&p);ttf_mono_bitmap_t b;assert(ttf_gray_render_stroke(face,cp,stbtt_ScaleForPixelHeight(&info,px),-1,0,&b));assert(b.pitch>=b.width);for(int y=0;y<b.height;++y)for(int x=0;x<b.width;++x){unsigned a=b.pixels[y*b.pitch+x];black+=a==255;gray+=a>0&&a<255;}++glyphs;}}
 for(int steps=-32;steps<=32;++steps){ttf_mono_bitmap_t b;assert(ttf_gray_render_stroke(face,0x1EAF,stbtt_ScaleForPixelHeight(&info,73),-1,steps,&b));}
 fail=true;ttf_mono_bitmap_t b;assert(!ttf_gray_render_stroke(face,'H',stbtt_ScaleForPixelHeight(&info,73),-1,0,&b));fail=false;assert(ttf_gray_render_stroke(face,'H',stbtt_ScaleForPixelHeight(&info,73),-1,0,&b));
 if(cycle==0&&argc>2){memset(canvas,255,sizeof(canvas));line(&info,face,35,"Old AA / native coverage",false,24);line(&info,face,125,"Trái tim của Tiểu",false,73);line(&info,face,220,"Trương vẫn đang đập.",false,73);line(&info,face,290,"Light auto-hint / gamma 1.0",true,24);line(&info,face,380,"Trái tim của Tiểu",true,73);line(&info,face,475,"Trương vẫn đang đập.",true,73);FILE*out=fopen(argv[2],"wb");assert(out);fprintf(out,"P5\n684 520\n255\n");assert(fwrite(canvas,1,sizeof(canvas),out)==sizeof(canvas));fclose(out);}
 assert(ttf_mono_bytes()<=512*1024);ttf_mono_close(face);assert(!ttf_mono_bytes());}
 assert(black&&gray);free(data);printf("PASS actual hinted gray: %u glyphs/3cycles, solidblack=%u edgepixels=%u, all65signedstrokes,256monotoniccoveragelevels,streamerror/recovery,512KiBcap,zeroafterclose\n",glyphs,black,gray);}
'''
with tempfile.TemporaryDirectory() as directory:
 p=Path(directory);(p/'test.c').write_text(UNIT)
 subprocess.run(['cc',*FLAGS,str(p/'test.c'),str(ROOT/'main/font/ttf_mono.c'),*[str(COMP/'vendor/src'/s) for s in SOURCES],'-lm','-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test'),sys.argv[1] if len(sys.argv)>1 else str(ROOT/'main/assets/builtin.ttf'),*sys.argv[2:]],check=True)
