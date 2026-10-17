/* SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
 * 中文：透明PNG书封直接叠加在当前画面上；保留原画布比例，不依赖阅读记录或额外整帧缓存。
 * English: Composite a transparent PNG cover over the current frame, preserving canvas ratio without reader history or an extra full-frame cache.
 */
#include "book_lock_bookmark.h"
#include "book_cover.h"
#include "ui_image_dither.h"
#include "esp_heap_caps.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>

typedef struct {
    uint8_t *visited,*pending;
    uint16_t *rows;
    unsigned height,head,tail,count;
} white_flood_t;
static void white_flood_mark(white_flood_t *f,size_t at,unsigned y) {
    uint8_t bit=(uint8_t)(1u<<(at&7));
    if(f->visited[at>>3]&bit)return;
    f->visited[at>>3]|=bit;
    if(f->pending[y])return;
    f->pending[y]=1;f->rows[f->tail]=(uint16_t)y;
    f->tail=(f->tail+1)%f->height;++f->count;
}
// 不透明白底PNG才自动去白：四邻接连通到画布边缘，封闭白色图案保留。
// Remove only near-white regions connected to an opaque PNG's canvas edges by four-neighbor paths; preserve enclosed whites.
// 原始alpha优先，避免透明素材的白色主体被误删；工作区仅一位/像素和有界行队列。
// Original alpha takes priority, protecting white artwork in transparent assets; workspace is one bit/pixel plus a bounded row queue.
static bool remove_white_background(uint8_t *gray,uint8_t *alpha,unsigned w,unsigned h) {
    size_t count=(size_t)w*h;
    if(!gray||!alpha||!w||!h||w>684||h>1216)return false;
    for(size_t i=0;i<count;++i)if(alpha[i]!=255)return true;
    bool border=false;
    for(unsigned x=0;x<w;++x)if(gray[x]>=250||gray[(h-1)*w+x]>=250){border=true;break;}
    if(!border)for(unsigned y=0;y<h;++y)if(gray[(size_t)y*w]>=250||gray[(size_t)y*w+w-1]>=250){border=true;break;}
    if(!border)return true;
    size_t bits=(count+7)/8,queue_offset=(bits+h+1)&~(size_t)1;
    uint8_t *work=heap_caps_malloc(queue_offset+2*h,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!work)return false;
    memset(work,0,queue_offset);
    white_flood_t f={.visited=work,.pending=work+bits,.rows=(uint16_t *)(work+queue_offset),.height=h};
    for(unsigned x=0;x<w;++x){
        if(gray[x]>=250)white_flood_mark(&f,x,0);
        size_t at=(size_t)(h-1)*w+x;if(gray[at]>=250)white_flood_mark(&f,at,h-1);
    }
    for(unsigned y=1;y+1<h;++y){
        size_t at=(size_t)y*w;if(gray[at]>=250)white_flood_mark(&f,at,y);
        if(gray[at+w-1]>=250)white_flood_mark(&f,at+w-1,y);
    }
    while(f.count){
        unsigned y=f.rows[f.head];f.head=(f.head+1)%h;--f.count;f.pending[y]=0;
        size_t row=(size_t)y*w;
        for(unsigned x=0;x<w;){
            if(gray[row+x]<250){++x;continue;}
            unsigned left=x;bool reached=false;
            while(x<w&&gray[row+x]>=250){size_t at=row+x++;if(f.visited[at>>3]&(1u<<(at&7)))reached=true;}
            if(!reached)continue;
            for(unsigned xx=left;xx<x;++xx){
                size_t at=row+xx;f.visited[at>>3]|=(uint8_t)(1u<<(at&7));
                if(y&&gray[at-w]>=250)white_flood_mark(&f,at-w,y-1);
                if(y+1<h&&gray[at+w]>=250)white_flood_mark(&f,at+w,y+1);
            }
        }
    }
    for(size_t i=0;i<count;++i)if(f.visited[i>>3]&(1u<<(i&7)))gray[i]=alpha[i]=0;
    free(work);return true;
}
static uint8_t pixel(const uint8_t *fb,int x,int y) {
    int px=x,py=y;
    switch(epd_get_rotation()) {
        case EPD_ROT_PORTRAIT:px=epd_width()-y-1;py=x;break;
        case EPD_ROT_INVERTED_PORTRAIT:px=y;py=epd_height()-x-1;break;
        case EPD_ROT_INVERTED_LANDSCAPE:px=epd_width()-x-1;py=epd_height()-y-1;break;
        default:break;
    }
    return (epd_get_pixel(px,py,epd_width(),epd_height(),fb)>>4)*17u;
}
bool book_lock_bookmark_draw(uint8_t *fb,const char *path) {
    unsigned sw,sh;
    const char *ext=path?strrchr(path,'.'):NULL;
    if(!fb||!ext||strcasecmp(ext,".png")||!book_image_file_dimensions(path,true,&sw,&sh)||!sw||!sh)return false;
    // 以整个PNG画布居中等比缩放，684x1216时逐像素对齐，不识别或裁剪主体。
    // Center-fit the entire PNG canvas; 684x1216 maps pixel-for-pixel, without subject detection or cropping.
    unsigned w=684,h=(uint64_t)sh*w/sw;
    if(h>1216){h=1216;w=(uint64_t)sw*h/sh;}
    if(!w||!h)return false;
    size_t count=(size_t)w*h;
    if(heap_caps_get_free_size(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT)<2*count+512u*1024u)return false;
    uint8_t *layers=heap_caps_malloc(2*count,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!layers)return false;
    uint8_t *alpha=layers+count;
    bool ok=book_png_file_layer(path,w,h,layers,alpha) && remove_white_background(layers,alpha,w,h);
    int ox=(684-(int)w)/2,oy=(1216-(int)h)/2;
    if(ok) {
        // 小幅偏移的柔影只作用在非透明轮廓附近。/ A softly offset shadow affects only the nontransparent silhouette.
        for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x){
            unsigned a=alpha[(size_t)y*w+x];if(!a||x+ox+4>=684||y+oy+6>=1216)continue;
            int xx=ox+x+4,yy=oy+y+6;
            unsigned bg=pixel(fb,xx,yy);
            epd_draw_pixel(xx,yy,ui_image_dither_gray((bg*(255-a/5)+127)/255,xx,yy),fb);
        }
        for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x){
            size_t at=(size_t)y*w+x;unsigned a=alpha[at];if(!a)continue;
            int xx=ox+x,yy=oy+y;
            unsigned value=layers[at]+(pixel(fb,xx,yy)*(255-a)+127)/255;
            if(value>255)value=255;
            epd_draw_pixel(xx,yy,ui_image_dither_gray(value,xx,yy),fb);
        }
    }
    free(layers);return ok;
}
void book_lock_bookmark_preview_paint(uint8_t *fb,const uint8_t *canvas,EpdRect area) {
    if(!fb||!canvas||area.width<1||area.height<1)return;
    for(int y=0;y<area.height;++y)for(int x=0;x<area.width;++x)
        epd_draw_pixel(area.x+x,area.y+y,pixel(canvas,x*684/area.width,y*1216/area.height)&0xf0,fb);
}
