/* SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
 * 中文：五行字体名/文件大小/下载按钮，状态变化有界局刷，不展示逐项授权信息。
 * English: Five compact name/size/download rows; bounded status refresh, without per-item license chrome.
 */
#include "ui_online_fonts.h"
#include <stdio.h>
#include <string.h>
#include "display.h"
#include "display_main_waveform.h"
#include "ui_kit.h"
#include "ui_nav.h"
#include "ui_font.h"
#include "ttf_font.h"

static EpdRect back_rect(void){return (EpdRect){30,77,154,72};}
static EpdRect row_rect(unsigned i){return (EpdRect){36,242+(int)i*120,612,120};}
static EpdRect download_rect(unsigned i){EpdRect r=row_rect(i);return (EpdRect){542,r.y+22,106,70};}
static EpdRect status_rect(void){return (EpdRect){36,866,612,176};}
static EpdRect return_rect(void){return (EpdRect){36,1086,612,82};}
static EpdRect cancel_button(bool yes){return (EpdRect){yes?351:72,681,261,84};}
EpdRect ui_online_fonts_entry_rect(int top){return (EpdRect){488,top+12,164,74};}
void ui_online_fonts_draw_entry(uint8_t *fb,int top){
    ui_text_box(fb,ui_online_fonts_entry_rect(top),25,"Tải phông",EPD_DRAW_ALIGN_CENTER,false);
}
void ui_online_fonts_open(ui_online_fonts_t *ui,bool reading){
    memset(ui,0,sizeof(*ui));ui->reading=reading;
    online_font_status(&ui->status);
    for(unsigned i=0;i<ONLINE_FONT_COUNT;++i)ui->installed[i]=online_font_installed(i);
}
void ui_online_fonts_close(ui_online_fonts_t *ui){
    online_font_cancel_join();ui->closed=true;ui->confirm_cancel=false;
}
static const char *font_label(const online_font_item_t *item){
    // 原UI字库不增字；当前字库不足以显示名字时，使用同一字体的拉丁名。
    // Do not expand built-in assets; use the same face's Latin name if its label lacks coverage.
    return ui_font_has_text(item->name)||ttf_font_has_text(item->name)?item->name:item->ascii_name;
}
static unsigned percent(const online_font_status_t *s){return s->total?(unsigned)((uint64_t)s->received*100/s->total):0;}
void ui_online_fonts_draw(const ui_online_fonts_t *ui,uint8_t *fb){
    ui_text_set_system_scale(true);ui_clear_page(fb);ui_nav_status(fb);
    ui_nav_back(fb,36,79);
    ui_text_box(fb,(EpdRect){188,79,308,70},34,"Phông trực tuyến",EPD_DRAW_ALIGN_CENTER,false);
    ui_hairline(fb,176,36,612,0x70);
    ui_text_box(fb,(EpdRect){36,192,612,40},21,ui->reading?"Dùng cho phông đọc sách":"Dùng cho phông hệ thống",EPD_DRAW_ALIGN_LEFT,false);
    for(unsigned i=0;i<ONLINE_FONT_COUNT;++i){
        const online_font_item_t *item=online_font_item(i);EpdRect r=row_rect(i),button=download_rect(i);
        ui_text_box(fb,(EpdRect){r.x,r.y+26,382,68},26,font_label(item),EPD_DRAW_ALIGN_LEFT,false);
        char size[24];snprintf(size,sizeof(size),"%.1fMB",(double)item->size/1000000.0);
        ui_text_box(fb,(EpdRect){426,r.y+26,102,68},20,size,EPD_DRAW_ALIGN_RIGHT,false);
        bool active=ui->status.busy&&ui->status.item==i;
        const char *label=ui->installed[i]?"Đã tải":active?"Hủy":"Tải về";
        bool enabled=active||(!ui->installed[i]&&!ui->status.busy);
        ui_fill_round_rect(fb,button,16,enabled?UI_GRAY_BLACK:UI_GRAY_WHITE);
        if(!enabled)ui_draw_control_frame(fb,button,16,UI_GRAY_BLACK);
        ui_text_box(fb,ui_inset_rect(button,4),24,label,EPD_DRAW_ALIGN_CENTER,enabled);
        ui_hairline(fb,r.y+r.height-1,r.x,r.width,0x80);
    }
    EpdRect status=status_rect();
    if(ui->status.message[0])ui_text_box(fb,(EpdRect){status.x,status.y,status.width,92},22,ui->status.message,EPD_DRAW_ALIGN_LEFT,false);
    if(ui->status.phase==ONLINE_FONT_DOWNLOADING){
        unsigned p=percent(&ui->status);char text[16];snprintf(text,sizeof(text),"%u%%",p);
        ui_text_box(fb,(EpdRect){status.x,status.y+95,90,52},21,text,EPD_DRAW_ALIGN_LEFT,false);
        ui_fill_rect_fast((EpdRect){142,status.y+112,506,8},UI_GRAY_WHITE,fb);
        ui_fill_rect_fast((EpdRect){142,status.y+112,(int)(506*p/100),8},UI_GRAY_BLACK,fb);
    }
    ui_draw_control_frame(fb,return_rect(),18,UI_GRAY_BLACK);
    ui_text_box(fb,return_rect(),27,"Quay lại danh sách phông",EPD_DRAW_ALIGN_CENTER,false);
    if(ui->confirm_cancel){
        EpdRect dialog={48,438,588,354};ui_fill_round_rect(fb,dialog,24,UI_GRAY_WHITE);ui_draw_control_frame(fb,dialog,24,UI_GRAY_BLACK);
        ui_text_box(fb,(EpdRect){72,466,540,78},32,"Hủy tải xuống?",EPD_DRAW_ALIGN_CENTER,false);
        ui_text_box(fb,(EpdRect){76,554,532,100},24,"Giữ nguyên phông hiện tại",EPD_DRAW_ALIGN_CENTER,false);
        for(int yes=0;yes<2;++yes){EpdRect b=cancel_button(yes);ui_fill_round_rect(fb,b,16,yes?UI_GRAY_BLACK:UI_GRAY_WHITE);
            ui_draw_control_frame(fb,b,16,UI_GRAY_BLACK);ui_text_box(fb,b,25,yes?"Hủy tải":"Tiếp tục tải",EPD_DRAW_ALIGN_CENTER,yes);}
    }
    ui_text_set_system_scale(false);
}
app_redraw_t ui_online_fonts_gesture(ui_online_fonts_t *ui,const ui_gesture_event_t *event){
    if(event->type!=UI_GESTURE_TAP)return APP_REDRAW_NONE;
    int x=event->x0,y=event->y0;
    if(ui->confirm_cancel){
        if(ui_rect_hit(cancel_button(false),x,y)){ui->confirm_cancel=false;return APP_REDRAW_PAGE;}
        if(ui_rect_hit(cancel_button(true),x,y)){
            online_font_cancel_join();online_font_status(&ui->status);ui->confirm_cancel=false;
            if(ui->leave_after_cancel)ui->closed=true;
            return APP_REDRAW_PAGE;
        }
        return APP_REDRAW_NONE;
    }
    if(ui_rect_hit(back_rect(),x,y)||ui_rect_hit(return_rect(),x,y)){
        if(ui->status.busy){ui->confirm_cancel=true;ui->leave_after_cancel=true;}
        else ui->closed=true;
        return APP_REDRAW_PAGE;
    }
    for(unsigned i=0;i<ONLINE_FONT_COUNT;++i){
        EpdRect b=download_rect(i);
        if(!ui_rect_hit(b,x,y)||!ui_rect_hit(b,event->x,event->y))continue;
        if(ui->status.busy){if(ui->status.item==i){ui->confirm_cancel=true;ui->leave_after_cancel=false;return APP_REDRAW_PAGE;}return APP_REDRAW_NONE;}
        if(ui->installed[i])return APP_REDRAW_NONE;
        (void)online_font_start(i);online_font_status(&ui->status);return APP_REDRAW_PAGE;
    }
    return APP_REDRAW_NONE;
}
app_redraw_t ui_online_fonts_tick(ui_online_fonts_t *ui,int64_t now_ms){
    online_font_status_t next;online_font_status(&next);
    if(next.revision==ui->status.revision)return APP_REDRAW_NONE;
    bool phase=next.phase!=ui->status.phase||next.busy!=ui->status.busy;
    if(!phase&&(now_ms-ui->updated_ms<250||percent(&next)==ui->painted_percent))return APP_REDRAW_NONE;
    ui->updated_ms=now_ms;ui->painted_percent=percent(&next);ui->status=next;
    if(next.phase==ONLINE_FONT_DONE&&next.item<ONLINE_FONT_COUNT)ui->installed[next.item]=online_font_installed(next.item);
    return phase?APP_REDRAW_PAGE:APP_REDRAW_AREA;
}
bool ui_online_fonts_present(ui_online_fonts_t *ui,app_ctx_t *ctx,app_redraw_t redraw){
    if(redraw==APP_REDRAW_NONE||redraw==APP_REDRAW_DONE)return true;
    display_main_transition_cancel();ui_online_fonts_draw(ui,ctx->fb);
    EpdRect area=redraw==APP_REDRAW_AREA?status_rect():(EpdRect){0,0,UI_LOCK_WIDTH,UI_LOCK_HEIGHT};
    guard_draw_result(ctx->hl,redraw==APP_REDRAW_FULL?update_display_full(ctx->hl):
        update_display_area_diff_with(ctx->hl,display_main_bw_waveform(),MODE_DU,area));
    return true;
}
