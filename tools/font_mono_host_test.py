#!/usr/bin/env python3
"""Build the exact firmware FreeType subset and exercise original-font streams."""
from pathlib import Path
import subprocess,tempfile,sys
ROOT=Path(__file__).resolve().parents[1]
COMP=ROOT/'components/kiiko_freetype'
SOURCES=['base/ftbase.c','base/ftinit.c','base/ftsystem.c','base/ftdebug.c','base/ftbitmap.c','base/ftmm.c','base/fttype1.c','truetype/truetype.c','sfnt/sfnt.c','raster/raster.c','autofit/autofit.c','psnames/psnames.c']
assert set(SOURCES)==set(__import__('re').findall(r'"vendor/src/([^"]+\.c)"',(COMP/'CMakeLists.txt').read_text()))
FLAGS=['-std=c11','-O2','-DFT2_BUILD_LIBRARY','-DFT_CONFIG_MODULES_H="kiiko_ftmodule.h"','-DFT_CONFIG_OPTIONS_H="kiiko_ftoption.h"','-I'+str(COMP/'vendor/include'),'-I'+str(COMP),'-I'+str(ROOT/'main/font')]
UNIT=r'''
#include "ttf_mono.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
typedef struct {unsigned char *data;size_t length,reads;bool fail;} source_t;
static bool read_font(void *ctx,uint32_t off,void *dst,size_t n){source_t*s=ctx;++s->reads;if(s->fail||(size_t)off>s->length||n>s->length-off)return false;memcpy(dst,s->data+off,n);return true;}
static unsigned char canvas[684*680];
static uint32_t decode(const char **p){const unsigned char*s=(const unsigned char*)*p;uint32_t c=*s++;int n=c<128?0:c<224?1:c<240?2:3;if(n)c&=(1u<<(6-n))-1;for(int i=0;i<n;++i)c=(c<<6)|(*s++&63);*p=(const char*)s;return c;}
static void draw(source_t *s,stbtt_fontinfo *info,ttf_mono_face_t *f,int baseline,bool hinted,const char *text,int size){
 (void)s;int pen=18;float scale=stbtt_ScaleForPixelHeight(info,size);
 while(*text){unsigned cp=decode(&text);int advance,lsb;stbtt_GetCodepointHMetrics(info,cp,&advance,&lsb);
  int w,h,x,y;uint8_t *bitmap=NULL;ttf_mono_bitmap_t mono={0};
  if(hinted){assert(ttf_mono_render(f,cp,scale,-1,&mono));w=mono.width;h=mono.height;x=mono.left;y=-mono.top;}
  else bitmap=stbtt_GetCodepointBitmap(info,scale,scale,cp,&w,&h,&x,&y);
  for(int gy=0;gy<h;++gy)for(int gx=0;gx<w;++gx){int px=pen+x+gx,py=baseline+y+gy;
   bool ink=hinted?(mono.pixels[gy*mono.pitch+gx/8]&(128>>(gx&7)))!=0:bitmap[gy*w+gx]>=128;
   if(ink&&px>=0&&px<684&&py>=0&&py<680)canvas[py*684+px]=0;}
  if(bitmap)stbtt_FreeBitmap(bitmap,NULL);pen+=(int)lroundf(advance*scale);
 }
}
int main(int argc,char **argv){
 assert(argc>=2);FILE*file=fopen(argv[1],"rb");assert(file);fseek(file,0,SEEK_END);size_t length=ftell(file);rewind(file);
 source_t source={.data=malloc(length),.length=length};assert(source.data&&fread(source.data,1,length,file)==length);fclose(file);
 stbtt_fontinfo info;assert(stbtt_InitFont(&info,source.data,stbtt_GetFontOffsetForIndex(source.data,0)));
 for(int cycle=0;cycle<3;++cycle){
  ttf_mono_face_t *face=ttf_mono_open(read_font,&source,length);assert(face);
  const char*sample="HimnoruĐường nét chữ tiếng Việt ẮẰẨỄỘỸ";
  unsigned glyphs=0;uint64_t digest=1469598103934665603ULL;clock_t start=clock();
  for(int size=18;size<=90;++size){const char*p=sample;while(*p){unsigned cp=decode(&p);ttf_mono_bitmap_t b;assert(ttf_mono_render(face,cp,stbtt_ScaleForPixelHeight(&info,size),-1,&b));assert(b.width>=0&&b.height>=0&&b.pitch>=(b.width+7)/8);++glyphs;
   const int metrics[]={b.width,b.height,b.left,b.top};
   for(unsigned k=0;k<4;++k)digest=(digest^(uint32_t)metrics[k])*1099511628211ULL;
   for(int y=0;y<b.height;++y)for(int x=0;x<b.width;++x)
    digest=(digest^((b.pixels[y*b.pitch+x/8]>>(7-(x&7)))&1u))*1099511628211ULL;
  }}
  printf("MONO_DIGEST %016llx\n",(unsigned long long)digest);
  assert(ttf_mono_bytes()<=512u*1024u);
  printf("PASS real hinted glyphs=%u, engine peak=%zuB, host render=%.1fms, stream reads=%zu\n",glyphs,ttf_mono_peak_bytes(),1000.*(clock()-start)/CLOCKS_PER_SEC,source.reads);
  if(cycle==0&&argc>2){memset(canvas,255,sizeof(canvas));
   draw(&source,&info,face,35,false,"Old mono / threshold",24);
   draw(&source,&info,face,125,false,"Trái tim của Tiểu",73);
   draw(&source,&info,face,225,false,"Trương vẫn đang đập.",73);
   draw(&source,&info,face,335,false,"Hinted mono / same advances",24);
   draw(&source,&info,face,425,true,"Trái tim của Tiểu",73);
   draw(&source,&info,face,525,true,"Trương vẫn đang đập.",73);
   FILE *out=fopen(argv[2],"wb");assert(out);fprintf(out,"P5\n684 680\n255\n");assert(fwrite(canvas,1,sizeof(canvas),out)==sizeof(canvas));fclose(out);}
  if(cycle==0 && stbtt__find_table(source.data,info.fontstart,"fvar")) {
   ttf_mono_bitmap_t light,heavy;float scale=stbtt_ScaleForPixelHeight(&info,73);
   assert(ttf_mono_render(face,'H',scale,300,&light));size_t n=(size_t)light.pitch*light.height;
   unsigned char *copy=malloc(n);assert(copy);memcpy(copy,light.pixels,n);
   assert(ttf_mono_render(face,'H',scale,700,&heavy));
   assert(light.width!=heavy.width||light.height!=heavy.height||light.pitch!=heavy.pitch||memcmp(copy,heavy.pixels,n)!=0);
   free(copy);puts("PASS variable wght300/700 produces distinct hinted glyphs");
  }
  source.fail=true;ttf_mono_bitmap_t unavailable;
  assert(!ttf_mono_render(face,'H',stbtt_ScaleForPixelHeight(&info,73),-1,&unavailable));
  source.fail=false;assert(ttf_mono_render(face,'H',stbtt_ScaleForPixelHeight(&info,73),-1,&unavailable));
  ttf_mono_close(face);assert(ttf_mono_bytes()==0);
 }
 source.fail=true;assert(!ttf_mono_open(read_font,&source,length));assert(ttf_mono_bytes()==0);
 source.fail=false;source.length=32;assert(!ttf_mono_open(read_font,&source,32));assert(ttf_mono_bytes()==0);
 assert(!ttf_mono_open(read_font,&source,0));assert(!ttf_mono_open(read_font,&source,17u*1024u*1024u));
 free(source.data);puts("PASS original-source IO failure/truncation, size bounds, complete library/face reclamation");
}
'''
if __name__=='__main__':
 with tempfile.TemporaryDirectory() as directory:
  p=Path(directory);(p/'test.c').write_text(UNIT)
  subprocess.run(['cc',*FLAGS,str(p/'test.c'),str(ROOT/'main/font/ttf_mono.c'),*[str(COMP/'vendor/src'/s) for s in SOURCES],'-lm','-o',str(p/'test')],check=True)
  subprocess.run([str(p/'test'),sys.argv[1] if len(sys.argv)>1 else str(ROOT/'main/assets/builtin.ttf'),*sys.argv[2:]],check=True)
