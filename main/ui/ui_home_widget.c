/* SPDX-License-Identifier: Apache-2.0
 * 中文：原比例封面、局部磨砂书夹与曲面短底座时钟；不保存整页或浮点画布。
 * English: Proportional covers on a dark shelf and curved short-pedestal clock; never retain full pages or float canvases.
 */
#include "ui_home_widget.h"
#include "ui_home_shapes.h"
#include "ui_image_dither.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define W UI_HOME_WIDGET_W
#define H UI_HOME_WIDGET_H
static uint8_t pixel(const ui_home_cover_t *c,unsigned x,unsigned y) {
    return c->binary ? (c->pixels[y*c->stride+x/8]&(1u<<(x&7))?255:0) : ui_contrast_gray(c->pixels[y*c->stride+x]);
}
static float minimum(float a,float b) { return a<b?a:b; }
static float maximum(float a,float b) { return a>b?a:b; }
static float clamp(float x,float a,float b) { return minimum(b,maximum(a,x)); }
static void cover(uint8_t *canvas,const ui_home_cover_t *c,float cx,float cy,float height,float angle) {
    if (!c->pixels||!c->width||!c->height) return;
    float scale=minimum(height/c->height,90.0f/c->width),wide=c->width*scale,tall=c->height*scale;
    float co=cosf(angle),si=sinf(angle),rx=(fabsf(co)*wide+fabsf(si)*tall)/2+3,ry=(fabsf(si)*wide+fabsf(co)*tall)/2+3;
    int xmin=maximum(0,floorf(cx-rx-24)),xmax=minimum(W-1,ceilf(cx+rx-24));
    int ymin=maximum(0,floorf(cy-ry-604)),ymax=minimum(H-1,ceilf(cy+ry-604));
    for (int y=ymin;y<=ymax;++y) for (int x=xmin;x<=xmax;++x) {
        float dx=x+24.5f-cx,dy=y+604.5f-cy,u=co*dx+si*dy,v=-si*dx+co*dy;
        float qx=fabsf(u)-wide/2+5,qy=fabsf(v)-tall/2+5;
        float d=hypotf(maximum(qx,0),maximum(qy,0))+minimum(maximum(qx,qy),0)-5;
        if (d>=.5f) continue;
        float sx=clamp((u/wide+.5f)*c->width-.5f,0,c->width-1),sy=clamp((v/tall+.5f)*c->height-.5f,0,c->height-1);
        unsigned ix=sx,iy=sy,nx=ix+1<c->width?ix+1:ix,ny=iy+1<c->height?iy+1:iy;
        float fx=sx-ix,fy=sy-iy;
        float gray=(1-fy)*((1-fx)*pixel(c,ix,iy)+fx*pixel(c,nx,iy))+fy*((1-fx)*pixel(c,ix,ny)+fx*pixel(c,nx,ny));
        if (d>-.8f) gray+=(224-gray)*.7f;
        float alpha=clamp(.5f-d,0,1);
        canvas[y*W+x]=(uint8_t)lroundf(canvas[y*W+x]+(gray-canvas[y*W+x])*alpha);
    }
}
static float edge_distance(const uint16_t spans[][2],int count,int row,float x) {
    float d=5;
    for (int dy=-4;dy<=4;++dy) {
        int r=row+dy;
        if (r<0||r>=count||spans[r][0]==spans[r][1]) { d=minimum(d,(float)abs(dy)); continue; }
        float lo=spans[r][0]/128.f,hi=spans[r][1]/128.f;
        float dx=minimum(fabsf(x-lo),fabsf(x-hi));
        d=minimum(d,hypotf(dx,dy));
    }
    return d;
}
bool ui_home_folder_compose(uint8_t *out,uint8_t *scratch,const ui_home_cover_t covers[3],unsigned count,bool binary) {
    if (!out||!scratch||!covers||count>3) return false;
    uint8_t *canvas=scratch;
    memset(canvas,255,W*H);
    for(unsigned i=0;i<count;++i) {
        const ui_home_cover_t *c=&covers[i];
        if(!c->pixels||!c->width||!c->height)continue;
        float scale=minimum(210.f/c->height,90.f/c->width);
        float height=c->height*scale;
        cover(canvas,c,24+54+106*i,604+280-height/2,height,0);
    }
    // Match the dark shelf's top highlight, solid fascia and lower edge.
    for(int y=280;y<292;++y)for(int x=12;x<W-12;++x)
        canvas[y*W+x]=binary?0:y==280?0x58:y==291?0x10:0x30;
    memset(out,0,binary?UI_HOME_FOLDER_BW_BYTES:UI_HOME_FOLDER_GRAY_BYTES);
    for (int y=0;y<H;++y) for (int x=0;x<W;++x) {
        uint8_t g=binary?ui_image_dither_bw(canvas[y*W+x],x+24,y+604):ui_image_dither_gray(canvas[y*W+x],x+24,y+604);
        if (binary) { if(g)out[y*(W/8)+x/8]|=1u<<(x&7); }
        else out[y*(W/2)+x/2]|=(g>>4)<<((x&1)*4);
    }
    return true;
}
void ui_home_folder_draw(uint8_t *fb,const uint8_t *pixels,bool binary,EpdRect bounds) {
    if (!pixels||bounds.width<=0||bounds.height<=0) return;
    for (int y=0;y<bounds.height;++y) for (int x=0;x<bounds.width;++x) {
        unsigned sx=(unsigned)x*W/bounds.width,sy=(unsigned)y*H/bounds.height;
        uint8_t g=binary?(pixels[sy*(W/8)+sx/8]&(1u<<(sx&7))?255:0):((pixels[sy*(W/2)+sx/2]>>((sx&1)*4))&15)*17;
        if(g<255) epd_draw_pixel(bounds.x+x,bounds.y+y,g,fb);
    }
}

static int tx(EpdRect b,float x) { return b.x+lroundf((x-24)*b.width/W); }
static int ty(EpdRect b,float y) { return b.y+lroundf((y-640)*b.height/H); }
static void hand(uint8_t *fb,EpdRect b,float angle,int length,int width) {
    float scale=b.width/(float)W;
    int x=tx(b,177),y=ty(b,783),xx=x+lroundf(sinf(angle)*length*scale),yy=y-lroundf(cosf(angle)*length*scale);
    int half=maximum(0,lroundf(width*scale)/2);
    for (int i=-half;i<=half;++i) epd_draw_line(x+i,y,xx+i,yy,0x20,fb);
}
void ui_home_clock_body_draw(uint8_t *fb,EpdRect b,bool binary) {
    if (b.width<=0||b.height<=0) return;
    for(int y=0;y<b.height;++y) {
        int row=(unsigned)y*H/b.height+640-657;
        if(row<0||row>=304) continue;
        float lo=home_clock_spans[row][0]/128.f,hi=home_clock_spans[row][1]/128.f;
        if(hi<=lo)continue;
        for(int x=maximum(0,tx(b,lo)-b.x);x<minimum(b.width,tx(b,hi)-b.x+1);++x) {
            float px=24+(x+.5f)*W/b.width;
            float edge=edge_distance(home_clock_spans,304,row,px);
            // 在快刷阈值化前生成纯黑白机身，避免中灰被整片转白；相位固定，分钟更新只动表盘。
            // Make the fast body binary before page thresholding can turn midgray white; fixed phase keeps minute updates face-only.
            uint8_t gray=edge<2.5f?0x30:0x99;
            if(binary)gray=edge<2.5f?0:ui_image_dither_acrylic_bw(gray,b.x+x,b.y+y);
            epd_draw_pixel(b.x+x,b.y+y,gray,fb);
        }
    }
    int cx=tx(b,177),cy=ty(b,783);float scale=b.width/(float)W;
    epd_fill_circle(cx,cy,lroundf(97*scale),0xff,fb);
    for(int tick=0;tick<60;++tick) {
        float a=tick*6.2831853f/60;int outer=lroundf(89*scale),inner=lroundf((tick%5?88:84)*scale);
        epd_draw_line(cx+lroundf(sinf(a)*inner),cy-lroundf(cosf(a)*inner),cx+lroundf(sinf(a)*outer),cy-lroundf(cosf(a)*outer),binary?0x20:tick%5?0x90:0x30,fb);
    }
}
void ui_home_clock_hands_draw(uint8_t *fb,EpdRect b,const struct tm *date) {
    if (b.width<=0||b.height<=0||!date) return;
    int cx=tx(b,177),cy=ty(b,783);float scale=b.width/(float)W;
    for(int h=1;h<=12;++h) {
        float a=h*6.2831853f/12;char value[4];snprintf(value,sizeof(value),"%d",h);
        ui_text_fixed_ink_vc(fb,cx+lroundf(sinf(a)*72*scale),cy-lroundf(cosf(a)*72*scale),lroundf(23*scale),value,EPD_DRAW_ALIGN_CENTER,0x20);
    }
    hand(fb,b,((date->tm_hour%12)+date->tm_min/60.f)*6.2831853f/12,48,4);
    hand(fb,b,date->tm_min*6.2831853f/60,70,2);
    epd_fill_circle(cx,cy,maximum(2,lroundf(6*scale)),0x10,fb);
}
void ui_home_clock_draw(uint8_t *fb,EpdRect b,const struct tm *date,bool binary) {
    if (!date) return;
    ui_home_clock_body_draw(fb,b,binary);
    ui_home_clock_hands_draw(fb,b,date);
}
