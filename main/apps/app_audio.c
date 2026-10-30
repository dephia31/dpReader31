/* SPDX-License-Identifier: Apache-2.0
 * Stage1 audio proof: explicit local WAV playback; no TTS requests or network.
 * Render paints only. Page exit, lock and media loss join playback before return.
 */
#include "app.h"
#include "ui_kit.h"
#include "ui_menu.h"
#include "ui_gesture.h"
#include "metalio_audio.h"
#include "display.h"
#include <string.h>
#include <stdio.h>
static char requested[320],selected[320];
static metalio_audio_state_t shown;
static bool suspended;
bool app_audio_request_open(const char *path){
    if(!path||strncmp(path,"/sdcard/",8)||strlen(path)>=sizeof(requested))return false;
    strcpy(requested,path);return true;
}
static void enter(app_ctx_t *ctx){
    (void)ctx;metalio_audio_stop();strcpy(selected,requested);requested[0]=0;
    suspended=true;shown=metalio_audio_state();
}
static void stop(app_ctx_t *ctx){(void)ctx;metalio_audio_stop();display_set_bulk_io(false);suspended=true;}
static bool active(void){metalio_audio_state_t s=metalio_audio_state();return s==METALIO_AUDIO_STARTING||s==METALIO_AUDIO_PLAYING;}
static void render(app_ctx_t *ctx,uint8_t *fb){
    (void)ctx;ui_clear_page(fb);
    ui_text(fb,342,100,36,"Nghe WAV thử nghiệm",EPD_DRAW_ALIGN_CENTER,false);
    ui_text(fb,342,200,26,"PCM 16-bit · 16 kHz · âm lượng 15%",EPD_DRAW_ALIGN_CENTER,false);
    const char *name=strrchr(selected,'/');name=name?name+1:selected;
    ui_text_box(fb,(EpdRect){40,270,604,80},24,name,EPD_DRAW_ALIGN_LEFT,false);
    ui_text_box(fb,(EpdRect){40,370,604,100},24,!selected[0]?"Mở tệp WAV từ mục Tệp để nghe thử":suspended?"Đã dừng":active()?"Đang chuẩn bị / phát WAV...":metalio_audio_message(),EPD_DRAW_ALIGN_LEFT,false);
    ui_draw_round_rect(fb,(EpdRect){80,500,524,110},18,0);
    ui_text(fb,342,570,32,active()?"Dừng":"Phát lại từ đầu",EPD_DRAW_ALIGN_CENTER,false);
    ui_text(fb,342,740,24,"Chạm để phát / dừng",EPD_DRAW_ALIGN_CENTER,false);
    ui_text(fb,342,830,24,"Phím trái: quay lại Tệp",EPD_DRAW_ALIGN_CENTER,false);
}
static app_redraw_t toggle(app_ctx_t *ctx){
    (void)ctx;
    if(!selected[0])return APP_REDRAW_NONE;
    if(active()){metalio_audio_stop();display_set_bulk_io(false);suspended=true;}
    else {suspended=false;display_set_bulk_io(true);if(metalio_audio_play_wav(selected)!=ESP_OK)display_set_bulk_io(false);}
    shown=metalio_audio_state();return APP_REDRAW_PAGE;
}
static app_redraw_t key(app_ctx_t *ctx,int key){
    if(key==UI_KEY_1||key==UI_KEY_3){stop(ctx);ctx->request_return=true;return APP_REDRAW_NONE;}
    if(key==UI_KEY_2)return toggle(ctx);
    return APP_REDRAW_NONE;
}
static app_redraw_t gesture(app_ctx_t *ctx,const ui_gesture_event_t *ev){
    if(ev->type==UI_GESTURE_TAP&&ui_rect_hit((EpdRect){80,500,524,110},ev->x0,ev->y0))return toggle(ctx);
    return APP_REDRAW_NONE;
}
static app_redraw_t tick(app_ctx_t *ctx){
    (void)ctx;metalio_audio_state_t current=metalio_audio_state();
    /* Avoid scanning the panel while audio starts/runs; paint completion only. */
    if(current!=shown&&(current==METALIO_AUDIO_DONE||current==METALIO_AUDIO_ERROR||current==METALIO_AUDIO_IDLE)){
        shown=current;display_set_bulk_io(false);return APP_REDRAW_PAGE;
    }
    return APP_REDRAW_NONE;
}
static bool disabled(app_ctx_t *ctx){(void)ctx;return false;}
const app_desc_t app_audio={.title="Nghe WAV",.detail="Thử loa từ thẻ nhớ",.owns_keys=true,
    .enter_full=true,.menu_handle_enabled=disabled,.on_enter=enter,.on_exit=stop,.on_before_lock=stop,
    .on_media_lost=stop,.render=render,.on_key=key,.on_gesture=gesture,.on_tick=tick};
