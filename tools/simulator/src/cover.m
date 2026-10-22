#define SIM_PLATFORM 1
#include "host.h"
#import <Cocoa/Cocoa.h>
#import <ImageIO/ImageIO.h>
#include "book_cover.h"
#include "book_cover_auto.h"
#include "book_epub.h"
#include <string.h>

static CGImageRef image(const uint8_t *p,size_t n){CFDataRef data=CFDataCreate(NULL,p,n);CGImageSourceRef source=CGImageSourceCreateWithData(data,NULL);CFRelease(data);if(!source)return NULL;CGImageRef result=CGImageSourceCreateImageAtIndex(source,0,NULL);CFRelease(source);return result;}
static bool decode(CGImageRef image,unsigned w,unsigned h,uint8_t*out,bool fill){if(!image||!w||!h)return false;CGColorSpaceRef space=CGColorSpaceCreateDeviceGray();CGContextRef c=CGBitmapContextCreate(out,w,h,8,w,space,kCGImageAlphaNone);CGColorSpaceRelease(space);if(!c)return false;memset(out,255,w*h);double sw=CGImageGetWidth(image),sh=CGImageGetHeight(image),scale=fill?fmax(w/sw,h/sh):fmin(w/sw,h/sh);double dw=sw*scale,dh=sh*scale;CGContextSetInterpolationQuality(c,kCGInterpolationHigh);CGContextDrawImage(c,CGRectMake((w-dw)/2,(h-dh)/2,dw,dh),image);CGContextRelease(c);return true;}
bool book_image_grayscale(const uint8_t*p,size_t n,bool png,unsigned w,unsigned h,uint8_t*out){(void)png;CGImageRef img=image(p,n);bool ok=decode(img,w,h,out,true);if(img)CGImageRelease(img);return ok;}
bool book_cover_thumbnail(const uint8_t*p,size_t n,bool png,uint8_t*out){return book_image_grayscale(p,n,png,176,240,out);}
book_crop_t book_cover_crop(unsigned sw,unsigned sh,unsigned dw,unsigned dh){if(!sw||!sh||!dw||!dh)return(book_crop_t){0};double ratio=fmax((double)dw/sw,(double)dh/sh);unsigned w=dw/ratio,h=dh/ratio;return(book_crop_t){(sw-w)/2,(sh-h)/2,w,h};}
static CGImageRef file_image(const char*path){NSURL *url=[NSURL fileURLWithPath:[NSString stringWithUTF8String:sim_path(path)]];CGImageSourceRef source=CGImageSourceCreateWithURL((__bridge CFURLRef)url,NULL);if(!source)return NULL;CGImageRef result=CGImageSourceCreateImageAtIndex(source,0,NULL);CFRelease(source);return result;}
bool book_image_file_dimensions(const char*p,bool png,unsigned*w,unsigned*h){(void)png;CGImageRef img=file_image(p);if(!img)return false;*w=CGImageGetWidth(img);*h=CGImageGetHeight(img);CGImageRelease(img);return true;}
bool book_image_file_grayscale(const char*p,bool png,unsigned w,unsigned h,uint8_t*out){(void)png;CGImageRef img=file_image(p);bool ok=decode(img,w,h,out,true);if(img)CGImageRelease(img);return ok;}
bool book_cover_load_gray(const char*p,const char*t,const char*a,uint8_t*out,bool allow,bool*pending){if(pending)*pending=false;if(!allow&&strcasestr(p,".epub")){if(pending)*pending=true;return false;}uint8_t*data=NULL;size_t n;bool png;if(allow&&book_epub_cover(p,&data,&n,&png)==0){bool ok=book_cover_thumbnail(data,n,png,out);free(data);if(ok)return true;}return book_auto_cover_render(p,t,a,176,240,out);}
bool book_cover_load_list_gray(const char*p,const char*t,const char*a,uint8_t*out,bool allow,bool*pending,unsigned*w,unsigned*h){*w=176;*h=240;if(pending)*pending=false;if(!allow&&strcasestr(p,".epub")){if(pending)*pending=true;return false;}uint8_t*data=NULL;size_t n;bool png;if(allow&&book_epub_cover(p,&data,&n,&png)==0){CGImageRef img=image(data,n);free(data);if(img){double scale=fmin(176.0/CGImageGetWidth(img),240.0/CGImageGetHeight(img));*w=lround(CGImageGetWidth(img)*scale);*h=lround(CGImageGetHeight(img)*scale);bool ok=decode(img,*w,*h,out,false);CGImageRelease(img);if(ok)return true;}}return book_auto_cover_render(p,t,a,176,240,out);}
bool book_cover_fast_cache_read(const char*p,const char*t,const char*a,book_fast_cover_key_t key,uint8_t*out,bool*edge){(void)p;(void)t;(void)a;(void)key;(void)out;(void)edge;return false;}
void book_cover_fast_cache_write(const char*p,const char*t,const char*a,book_fast_cover_key_t key,const uint8_t*out,bool edge){(void)p;(void)t;(void)a;(void)key;(void)out;(void)edge;}
void sim_frame_write(const uint8_t*frame,const char*path){@autoreleasepool{uint8_t*gray=malloc(684*1216);for(int y=0;y<1216;y++)for(int x=0;x<684;x++){int px=1215-y,py=x;uint8_t b=frame[py*608+px/2];gray[y*684+x]=(px%2?b>>4:b&15)*17;}CGColorSpaceRef space=CGColorSpaceCreateDeviceGray();CGContextRef c=CGBitmapContextCreate(gray,684,1216,8,684,space,kCGImageAlphaNone);CGColorSpaceRelease(space);CGImageRef img=CGBitmapContextCreateImage(c);NSString*file=[NSString stringWithUTF8String:path];NSURL*url=[NSURL fileURLWithPath:[file stringByAppendingString:@".tmp"]];CGImageDestinationRef dest=CGImageDestinationCreateWithURL((__bridge CFURLRef)url,CFSTR("public.png"),1,NULL);CGImageDestinationAddImage(dest,img,NULL);CGImageDestinationFinalize(dest);CFRelease(dest);CGImageRelease(img);CGContextRelease(c);free(gray);rename([[file stringByAppendingString:@".tmp"] fileSystemRepresentation],[file fileSystemRepresentation]);}}

int book_cover_load_io(const char *p,const char *t,const char *a,uint8_t *out,bool list,unsigned *w,unsigned *h,bool *automatic){*automatic=false;return (list?book_cover_load_list_gray(p,t,a,out,true,NULL,w,h):book_cover_load_gray(p,t,a,out,true,NULL))?1:-1;}
bool book_cover_list_cache_dimensions(const char*p,const char*t,const char*a,unsigned*w,unsigned*h){return false;}
void book_cover_store_generated(const char*p,const char*t,const char*a,const uint8_t*x,bool list,int64_t size,int64_t modified){}
void book_cover_fast_cache_write_checked(const char*p,const char*t,const char*a,book_fast_cover_key_t k,const uint8_t*x,bool edge,int64_t size,int64_t modified){}

bool book_png_file_layer(const char*p,unsigned w,unsigned h,uint8_t*gray,uint8_t*alpha){
 if(!w||!h||!gray||!alpha)return false;
 CGImageRef img=file_image(p);if(!img)return false;
 uint8_t *rgba=calloc((size_t)w*h,4);CGColorSpaceRef space=CGColorSpaceCreateDeviceRGB();
 CGContextRef c=CGBitmapContextCreate(rgba,w,h,8,w*4,space,kCGImageAlphaPremultipliedLast|kCGBitmapByteOrder32Big);CGColorSpaceRelease(space);
 if(!c){free(rgba);CGImageRelease(img);return false;}
 CGContextSetInterpolationQuality(c,kCGInterpolationHigh);CGContextDrawImage(c,CGRectMake(0,0,w,h),img);
 for(size_t i=0;i<(size_t)w*h;i++){unsigned a=rgba[4*i+3];alpha[i]=a;unsigned v=(77u*rgba[4*i]+150u*rgba[4*i+1]+29u*rgba[4*i+2])>>8;gray[i]=a?(uint8_t)fmin(255,v*255u/a):255;}
 CGContextRelease(c);CGImageRelease(img);free(rgba);return true;
}
