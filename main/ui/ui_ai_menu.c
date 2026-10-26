/* SPDX-License-Identifier: Apache-2.0
 * 中文：复用既有液态玻璃；仅展开时模糊一次，保存一块底图，输入期间零分配/零绘制。
 * English: Reuse liquid glass, blur once on opening, save one bounded underlay; input allocates and paints nothing.
 */
#include "ui_ai_menu.h"
#include "ui_kit.h"
#include "ui_image_dither.h"
#include "ui_glass.h"
#include "esp_heap_caps.h"
#include <stdlib.h>
#include <string.h>
// 图标、下方标签和选中点共用横向节拍；列间空隙不可误触。
// Icons,labels and selection dots share a horizontal rhythm; inter-column gaps remain inert.
EpdRect ui_ai_menu_option(const ui_ai_menu_t *m,int option){
    int left=m->rect.x+16+option*(m->rect.width-32)/APP_AI_ACTION_COUNT;
    int right=m->rect.x+16+(option+1)*(m->rect.width-32)/APP_AI_ACTION_COUNT;
    return (EpdRect){left+4,m->rect.y+58,right-left-8,138};
}
static void readable_circle(uint8_t *fb,int cx,int cy){
    const int radius=42;
    for(int y=cy-radius;y<cy+radius;++y)for(int x=cx-radius;x<cx+radius;++x){
        int dx=2*(x-cx)+1,dy=2*(y-cy)+1;
        if(dx*dx+dy*dy>4*radius*radius)continue;
        unsigned v=ui_read_pixel(fb,x,y);v+=(15-v)*60/100;
        epd_draw_pixel(x,y,(uint8_t)(v<<4),fb);
    }
    epd_draw_circle(cx,cy,radius,0x60,fb);
    epd_draw_circle(cx,cy,radius-1,0xf0,fb);
}
static int hit(const ui_ai_menu_t*m,int x,int y){
    for(int i=0;i<APP_AI_ACTION_COUNT;++i)if(ui_rect_hit(ui_ai_menu_option(m,i),x,y))return i;
    return -1;
}
void ui_ai_menu_end(ui_ai_menu_t *m){heap_caps_free(m->underlay);memset(m,0,sizeof(*m));}
bool ui_ai_menu_restore(ui_ai_menu_t *m,uint8_t *fb){
    if(!m->underlay||m->frame!=fb||m->rotation!=epd_get_rotation())return false;
    EpdRect r=m->rect;
    for(int y=0;y<r.height;++y)for(int x=0;x<r.width;++x){
        size_t i=(size_t)y*r.width+x;
        epd_draw_pixel(r.x+x,r.y+y,(uint8_t)(((m->underlay[i/2]>>((i&1)*4))&15)<<4),fb);
    }
    return true;
}
bool ui_ai_menu_begin(ui_ai_menu_t *m,uint8_t *fb,app_ai_action_t selected,bool reader){
    (void)reader;
    if(m->open||!fb||epd_rotated_display_width()<684||epd_rotated_display_height()<368)return false;
    const int w=epd_rotated_display_width();
    EpdRect r={(w-648)/2,96,648,252};
    uint8_t *saved=heap_caps_calloc(1,(size_t)r.width*r.height/2,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!saved)return false;
    for(int y=0;y<r.height;++y)for(int x=0;x<r.width;++x){
        size_t i=(size_t)y*r.width+x;saved[i/2]|=ui_read_pixel(fb,r.x+x,r.y+y)<<((i&1)*4);
    }
    *m=(ui_ai_menu_t){.underlay=saved,.frame=fb,.rect=r,.rotation=epd_get_rotation(),.selected=selected,.pressed=-1,.open=true,.draining=true};
    if(!ui_glass_panel_frost(fb,r,40,52,160,true)){
        ui_fill_round_rect(fb,r,40,0xe0);ui_draw_round_rect(fb,r,40,0x60);
    }
    // 最后一项是反白而非加粗；标题与标签必须黑字，且在玻璃处理完成后绘制。
    // The final flag means inverted,not bold. Draw black labels only after glass processing.
    ui_text_fixed_vc(fb,r.x+r.width/2,r.y+32,28,"Phím tắt nhanh",EPD_DRAW_ALIGN_CENTER,false);
    static const char*labels[]={"Làm mới","Dấu trang","Đèn nền","Trang chủ","Tắt phím"};
    static const ui_icon_t icons[]={UI_ICON_REFRESH_CW,UI_ICON_BOOKMARK,UI_ICON_LIGHTBULB,UI_ICON_HOUSE,UI_ICON_X};
    for(int i=0;i<APP_AI_ACTION_COUNT;++i){
        EpdRect option=ui_ai_menu_option(m,i);int cx=option.x+option.width/2,cy=r.y+106;
        readable_circle(fb,cx,cy);
        ui_draw_icon(fb,cx,cy,36,icons[i],UI_GRAY_BLACK);
        // 标签直接叠在玻璃上，不加独立白色底块。
        // Draw labels directly on the glass, without separate white backing.
        ui_text_fixed_vc(fb,cx,r.y+174,22,labels[i],EPD_DRAW_ALIGN_CENTER,false);
        if(i==(int)selected){
            epd_fill_circle(cx+22,cy-22,6,UI_GRAY_WHITE,fb);
            epd_fill_circle(cx+22,cy-22,4,UI_GRAY_BLACK,fb);
        }
    }
    ui_text_fixed_vc(fb,r.x+r.width/2,r.y+224,18,"Chọn tác vụ nhấn ngắn · Dấu trang chỉ hoạt động khi đọc sách",EPD_DRAW_ALIGN_CENTER,false);
    return true;
}
int ui_ai_menu_feed(ui_ai_menu_t *m,const app_ctx_t *ctx,bool valid){
    if(!m->open)return UI_AI_NO_ACTION;
    const cst836u_touch_t*t=ctx->touch;
    if(!valid||!t||t->count>1){m->armed=false;m->draining=true;return UI_AI_NO_ACTION;}
    if(m->draining){if(!t->touched)m->draining=false;return UI_AI_NO_ACTION;}
    if(ctx->pressed){m->pressed=hit(m,t->x,t->y);m->x0=t->x;m->y0=t->y;m->armed=true;}
    if(m->armed&&t->touched&&(hit(m,t->x,t->y)!=m->pressed||abs((int)t->x-m->x0)>20||abs((int)t->y-m->y0)>20))m->armed=false;
    if(ctx->released&&m->armed){m->armed=false;return m->pressed<0?UI_AI_CLOSE:m->pressed;}
    return UI_AI_NO_ACTION;
}
EpdRect ui_ai_menu_save_error(ui_ai_menu_t*m,uint8_t*fb){
    EpdRect r={m->rect.x+16,m->rect.y+204,m->rect.width-32,36};
    ui_fill_round_rect(fb,r,16,UI_GRAY_WHITE);
    ui_text_fixed_vc(fb,r.x+r.width/2,r.y+r.height/2,20,"Lưu thất bại, vui lòng thử lại",EPD_DRAW_ALIGN_CENTER,false);return r;
}
