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
    int column=option%3,row=option/3;
    int left=m->rect.x+16+column*(m->rect.width-32)/3;
    int right=m->rect.x+16+(column+1)*(m->rect.width-32)/3;
    return (EpdRect){left+4,m->rect.y+58+row*m->row_height,right-left-8,m->row_height-6};
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
    for(int i=0;i<m->count;++i)if(ui_rect_hit(ui_ai_menu_option(m,i),x,y))return i;
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
    uint32_t mask=app_settings_action_menu();
    app_ai_action_t actions[APP_AI_MENU_COUNT];int count=0;
    for(int action=0;action<APP_AI_ACTION_COUNT;++action)
        if((mask&(1u<<action))&&count<APP_AI_MENU_COUNT)actions[count++]=action;
    int rows=count?(count+2)/3:1;
    int row_height=(epd_rotated_display_height()-96-24-84)/rows;
    if(row_height>144)row_height=144;
    if(row_height<120)return false;
    EpdRect r={(w-648)/2,96,648,84+rows*row_height};
    uint8_t *saved=heap_caps_calloc(1,(size_t)r.width*r.height/2,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!saved)return false;
    for(int y=0;y<r.height;++y)for(int x=0;x<r.width;++x){
        size_t i=(size_t)y*r.width+x;saved[i/2]|=ui_read_pixel(fb,r.x+x,r.y+y)<<((i&1)*4);
    }
    *m=(ui_ai_menu_t){.underlay=saved,.frame=fb,.rect=r,.rotation=epd_get_rotation(),.selected=selected,.pressed=-1,.open=true,.draining=true,.count=count,.row_height=row_height};
    memcpy(m->actions,actions,(size_t)count*sizeof(actions[0]));
    ui_fill_round_rect(fb,r,40,UI_GRAY_WHITE);
    ui_draw_control_frame(fb,r,40,0x60);
    // 最后一项是反白而非加粗；标题与标签必须黑字，且在玻璃处理完成后绘制。
    // The final flag means inverted,not bold. Draw black labels only after glass processing.
    ui_text_fixed_vc(fb,r.x+r.width/2,r.y+32,28,"Menu thao tác nhanh",EPD_DRAW_ALIGN_CENTER,false);
    static const char*labels[]={"Làm mới","Dấu trang","Đèn nền","Trang chủ","Không dùng","KoSync","Trang trước","Trang sau","Chương trước","Chương sau","Menu đọc sách"};
    static const ui_icon_t icons[]={UI_ICON_REFRESH_CW,UI_ICON_BOOKMARK,UI_ICON_LIGHTBULB,UI_ICON_HOUSE,UI_ICON_X,UI_ICON_ROTATE_CW,UI_ICON_CHEVRON_LEFT,UI_ICON_CHEVRON_RIGHT,UI_ICON_CHEVRON_LEFT,UI_ICON_CHEVRON_RIGHT,UI_ICON_LIST};
    for(int i=0;i<m->count;++i){
        EpdRect option=ui_ai_menu_option(m,i);int cx=option.x+option.width/2,cy=option.y+48;
        int action=m->actions[i];readable_circle(fb,cx,cy);
        ui_draw_icon(fb,cx,cy,36,icons[action],UI_GRAY_BLACK);
        ui_text_fixed_vc(fb,cx,option.y+116,22,labels[action],EPD_DRAW_ALIGN_CENTER,false);
    }
    if(!count)ui_text_fixed_vc(fb,r.x+r.width/2,r.y+110,22,"Thêm tác vụ trong Cài đặt → Menu thao tác nhanh",EPD_DRAW_ALIGN_CENTER,false);
    return true;
}
int ui_ai_menu_feed(ui_ai_menu_t *m,const app_ctx_t *ctx,bool valid){
    if(!m->open)return UI_AI_NO_ACTION;
    const cst836u_touch_t*t=ctx->touch;
    if(!valid||!t||t->count>1){m->armed=false;m->draining=true;return UI_AI_NO_ACTION;}
    if(m->draining){if(!t->touched)m->draining=false;return UI_AI_NO_ACTION;}
    if(ctx->pressed){m->pressed=hit(m,t->x,t->y);m->x0=t->x;m->y0=t->y;m->armed=true;}
    if(m->armed&&t->touched&&(hit(m,t->x,t->y)!=m->pressed||abs((int)t->x-m->x0)>20||abs((int)t->y-m->y0)>20))m->armed=false;
    if(ctx->released&&m->armed){m->armed=false;return m->pressed<0?UI_AI_CLOSE:m->actions[m->pressed];}
    return UI_AI_NO_ACTION;
}
EpdRect ui_ai_menu_save_error(ui_ai_menu_t*m,uint8_t*fb){
    EpdRect r={m->rect.x+16,m->rect.y+204,m->rect.width-32,36};
    ui_fill_round_rect(fb,r,16,UI_GRAY_WHITE);
    ui_text_fixed_vc(fb,r.x+r.width/2,r.y+r.height/2,20,"Lưu thất bại, vui lòng thử lại",EPD_DRAW_ALIGN_CENTER,false);return r;
}
