/* SPDX-License-Identifier: Apache-2.0 */
#include "ui_choice.h"
#include <stdio.h>
#include <string.h>
EpdRect ui_choice_box(void) {
    int w=epd_rotated_display_width()-48,h=epd_rotated_display_height()-64;
    if(w>648)w=648;
    if(h>960)h=960;
    return (EpdRect){(epd_rotated_display_width()-w)/2,(epd_rotated_display_height()-h)/2,w,h};
}
static int choice_columns(void){return ui_choice_box().width>=480?2:1;}
static int choice_rows(void){int n=(ui_choice_box().height-240)/104;return n<1?1:n>6?6:n;}
int ui_choice_slots(void){return choice_columns()*choice_rows();}
EpdRect ui_choice_item(int slot) {
    EpdRect r=ui_choice_box();int cols=choice_columns(),pitch=(r.width-36)/cols;
    int step=(r.height-240)/choice_rows();
    return (EpdRect){r.x+18+(slot%cols)*pitch,r.y+108+(slot/cols)*step,pitch-10,step-12};
}
static EpdRect choice_close(void){EpdRect r=ui_choice_box();return (EpdRect){r.x+r.width-74,r.y+18,56,64};}
static int choice_page_width(void){int n=(ui_choice_box().width-60)/3;return n>140?140:n;}
static EpdRect choice_page_button(int next){EpdRect r=ui_choice_box();int w=choice_page_width();return (EpdRect){next?r.x+r.width-18-w:r.x+18,r.y+r.height-80,w,60};}
void ui_choice_begin(ui_choice_t *c,int kind,const char *title,int count,int current) {
    memset(c,0,sizeof(*c));if(count<=0||ui_choice_box().height<320||ui_choice_box().width<240)return;
    c->open=true;c->kind=kind;c->title=title;c->count=count;
    c->current=current>=0&&current<count?current:-1;
    c->start=c->current>=0?(current/ui_choice_slots())*ui_choice_slots():0;
}
bool ui_choice_page(ui_choice_t *c,int direction) {
    if(!direction)return false;
    int next=c->start+(direction<0?-ui_choice_slots():ui_choice_slots());
    if(next<0||next>=c->count)return false;
    c->start=next;return true;
}
int ui_choice_tap(ui_choice_t *c,int x,int y) {
    if(!c->open)return UI_CHOICE_NONE;
    if(!ui_rect_hit(ui_choice_box(),x,y)||ui_rect_hit(choice_close(),x,y)){c->open=false;return UI_CHOICE_CANCEL;}
    for(int n=0;n<2;n++)if(ui_rect_hit(choice_page_button(n),x,y))
        return ui_choice_page(c,n?1:-1)?UI_CHOICE_PAGE:UI_CHOICE_NONE;
    for(int i=0;i<ui_choice_slots()&&c->start+i<c->count;i++)
        if(ui_rect_hit(ui_choice_item(i),x,y))return c->start+i;
    return UI_CHOICE_NONE;
}
void ui_choice_draw(const ui_choice_t *c,uint8_t *fb,ui_choice_label_fn label) {
    if(!c->open)return;
    ui_clear_page(fb);ui_text_set_system_scale(true);
    EpdRect r=ui_choice_box();
    ui_fill_round_rect(fb,r,24,UI_GRAY_WHITE);ui_draw_control_frame(fb,r,24,UI_GRAY_BLACK);
    ui_text_box(fb,(EpdRect){r.x+24,r.y+18,r.width-110,70},27,c->title,EPD_DRAW_ALIGN_LEFT,false);
    EpdRect close=choice_close();ui_text_box(fb,close,25,"×",EPD_DRAW_ALIGN_CENTER,false);
    for(int i=0;i<ui_choice_slots()&&c->start+i<c->count;i++){
        int index=c->start+i;EpdRect item=ui_choice_item(i);bool active=index==c->current;
        ui_fill_round_rect(fb,item,16,active?UI_GRAY_BLACK:UI_GRAY_WHITE);ui_draw_control_frame(fb,item,16,UI_GRAY_BLACK);
        char text[96];label(c->kind,index,text,sizeof(text));
        ui_text_box(fb,ui_inset_rect(item,10),23,text,EPD_DRAW_ALIGN_CENTER,active);
    }
    for(int n=0;n<2;n++){
        EpdRect b=choice_page_button(n);ui_draw_control_frame(fb,b,14,UI_GRAY_BLACK);
        ui_text_box(fb,b,23,n?"Sau":"Trước",EPD_DRAW_ALIGN_CENTER,false);
    }
    char page[40];snprintf(page,sizeof(page),"%d / %d",c->start/ui_choice_slots()+1,(c->count+ui_choice_slots()-1)/ui_choice_slots());
    ui_text_box(fb,(EpdRect){r.x+26+choice_page_width(),r.y+r.height-80,r.width-52-2*choice_page_width(),60},22,page,EPD_DRAW_ALIGN_CENTER,false);
    if(c->error[0])ui_text_box(fb,(EpdRect){r.x+20,r.y+r.height-122,r.width-40,30},16,c->error,EPD_DRAW_ALIGN_CENTER,false);
}
