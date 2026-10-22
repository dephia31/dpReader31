/* 浏览器只替换图像解码边界；封面布局由固件负责。/ Replace only image codecs; firmware owns cover layout. */
#include "host.h"
#include "book_cover.h"
#include "book_cover_auto.h"
#include "book_epub.h"
#include <math.h>
#include <string.h>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_MAX_DIMENSIONS 4096
#include "stb_image.h"
static bool resize(const uint8_t *rgba,unsigned sw,unsigned sh,unsigned w,unsigned h,uint8_t *gray,uint8_t *alpha,bool fill){
    if(!rgba||!sw||!sh||!w||!h||sw>8192||sh>8192)return false;
    double scale=fill?fmax((double)w/sw,(double)h/sh):fmin((double)w/sw,(double)h/sh);
    double ox=(w-sw*scale)/2,oy=(h-sh*scale)/2;memset(gray,255,w*h);if(alpha)memset(alpha,0,w*h);
    for(unsigned y=0;y<h;y++)for(unsigned x=0;x<w;x++){
        double sx=(x+.5-ox)/scale-.5,sy=(y+.5-oy)/scale-.5;if(sx<-.5||sy<-.5||sx>sw-.5||sy>sh-.5)continue;
        int ix=fmax(0,fmin(sw-1,lround(sx))),iy=fmax(0,fmin(sh-1,lround(sy)));const uint8_t *p=rgba+4*((size_t)iy*sw+ix);
        unsigned g=(p[0]*77+p[1]*150+p[2]*29)>>8;gray[(size_t)y*w+x]=alpha?g:(g*p[3]+255*(255-p[3]))/255;if(alpha)alpha[(size_t)y*w+x]=p[3];
    }return true;
}
bool book_image_grayscale(const uint8_t*p,size_t n,bool png,unsigned w,unsigned h,uint8_t*out){int sw=0,sh=0,c;uint8_t *rgba=stbi_load_from_memory(p,n,&sw,&sh,&c,4);bool ok=resize(rgba,sw,sh,w,h,out,NULL,true);stbi_image_free(rgba);return ok;}
bool book_cover_thumbnail(const uint8_t*p,size_t n,bool png,uint8_t*out){return book_image_grayscale(p,n,png,BOOK_COVER_W,BOOK_COVER_H,out);}
book_crop_t book_cover_crop(unsigned sw,unsigned sh,unsigned dw,unsigned dh){if(!sw||!sh||!dw||!dh)return(book_crop_t){0};double scale=fmax((double)dw/sw,(double)dh/sh);unsigned w=dw/scale,h=dh/scale;return(book_crop_t){(sw-w)/2,(sh-h)/2,w,h};}
bool book_image_file_dimensions(const char*p,bool png,unsigned*w,unsigned*h){int x,y,c;if(!stbi_info(sim_path(p),&x,&y,&c))return false;*w=x;*h=y;return true;}
bool book_image_file_grayscale(const char*p,bool png,unsigned w,unsigned h,uint8_t*out){int sw=0,sh=0,c;uint8_t *rgba=stbi_load(sim_path(p),&sw,&sh,&c,4);bool ok=resize(rgba,sw,sh,w,h,out,NULL,true);stbi_image_free(rgba);return ok;}
bool book_png_file_layer(const char*p,unsigned w,unsigned h,uint8_t*gray,uint8_t*alpha){int sw=0,sh=0,c;uint8_t *rgba=stbi_load(sim_path(p),&sw,&sh,&c,4);bool ok=resize(rgba,sw,sh,w,h,gray,alpha,false);stbi_image_free(rgba);return ok;}
bool book_cover_load_list_gray(const char*p,const char*t,const char*a,uint8_t*out,bool allow,bool*pending,unsigned*w,unsigned*h){
    *w=BOOK_COVER_W;*h=BOOK_COVER_H;if(pending)*pending=false;uint8_t*data=NULL;size_t n=0;bool png;
    if(allow&&book_epub_cover(p,&data,&n,&png)==ESP_OK){int sw=0,sh=0,c;uint8_t*rgba=stbi_load_from_memory(data,n,&sw,&sh,&c,4);free(data);if(rgba){double scale=fmin((double)*w/sw,(double)*h/sh);*w=lround(sw*scale);*h=lround(sh*scale);bool ok=resize(rgba,sw,sh,*w,*h,out,NULL,false);stbi_image_free(rgba);if(ok)return true;}}
    return book_auto_cover_render(p,t,a,*w,*h,out);
}
bool book_cover_load_gray(const char*p,const char*t,const char*a,uint8_t*out,bool allow,bool*pending){unsigned w,h;uint8_t*data=NULL;size_t n=0;bool png;if(pending)*pending=false;if(allow&&book_epub_cover(p,&data,&n,&png)==ESP_OK){bool ok=book_cover_thumbnail(data,n,png,out);free(data);if(ok)return true;}return book_auto_cover_render(p,t,a,BOOK_COVER_W,BOOK_COVER_H,out);}
int book_cover_load_io(const char*p,const char*t,const char*a,uint8_t*out,bool list,unsigned*w,unsigned*h,bool*automatic){*automatic=false;return(list?book_cover_load_list_gray(p,t,a,out,true,NULL,w,h):book_cover_load_gray(p,t,a,out,true,NULL))?1:-1;}
bool book_cover_list_cache_dimensions(const char*p,const char*t,const char*a,unsigned*w,unsigned*h){return false;}
bool book_cover_fast_cache_read(const char*p,const char*t,const char*a,book_fast_cover_key_t k,uint8_t*x,bool*e){return false;}
void book_cover_fast_cache_write(const char*p,const char*t,const char*a,book_fast_cover_key_t k,const uint8_t*x,bool e){}
void book_cover_fast_cache_write_checked(const char*p,const char*t,const char*a,book_fast_cover_key_t k,const uint8_t*x,bool e,int64_t size,int64_t modified){}
void book_cover_store_generated(const char*p,const char*t,const char*a,const uint8_t*x,bool list,int64_t size,int64_t modified){}
void sim_frame_write(const uint8_t*frame,const char*path){}
