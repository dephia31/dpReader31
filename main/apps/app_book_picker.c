/* SPDX-License-Identifier: Apache-2.0
 * Simple mode reads one bounded directory page; no shelf, artwork or worker.
 * Directory order follows the card, avoiding a whole-directory sort buffer.
 */
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include "app.h"
#include "app_registry.h"
#include "app_content_open.h"
#include "book_file_path.h"
#include "read_pico_sd.h"
#include "settings.h"
#include "ui_gesture.h"
#include "ui_kit.h"
#include "ui_nav.h"
#include "ui_menu.h"
#include "esp_heap_caps.h"

#define PICK_ROWS 8
#define PICK_PATH 288
#define PICK_TOP 244
#define PICK_ROW_H 88

typedef struct { char name[256]; bool directory; } picker_item_t;
static picker_item_t *s_items;
static char s_directory[PICK_PATH];
static char s_notice[96];
static unsigned s_page,s_count;
static bool s_more,s_pending;
extern const app_desc_t app_book_picker;

static EpdRect page_button(unsigned i) {return (EpdRect){i?492:36,1006,156,72};}
static EpdRect page_label(void) {return (EpdRect){208,1006,268,72};}
static bool page_hit(EpdRect box,unsigned x,unsigned y) {
    return x>=(unsigned)box.x&&x<(unsigned)(box.x+box.width)&&y>=(unsigned)box.y&&y<(unsigned)(box.y+box.height);
}
static bool card_path(const char *path) {
    return path && (!strcmp(path,"/sdcard") || !strncmp(path,"/sdcard/",8));
}
static void scan_page(void) {
    s_count=0;s_more=false;s_notice[0]=0;
    if(!read_pico_sd_io_ready()){snprintf(s_notice,sizeof(s_notice),"Thẻ nhớ chưa sẵn sàng");return;}
    if(!s_items)s_items=heap_caps_calloc(PICK_ROWS,sizeof(*s_items),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!s_items){snprintf(s_notice,sizeof(s_notice),"Không đủ bộ nhớ. Hãy mở lại trang.");return;}
    DIR *dir=opendir(s_directory);
    if(!dir){snprintf(s_notice,sizeof(s_notice),"Không thể mở thư mục");return;}
    unsigned skip=s_page*PICK_ROWS;
    struct dirent *entry;
    bool failed=false;
    for(;;) {
        errno=0;entry=readdir(dir);
        if(!entry){failed=errno!=0;break;}
        if(entry->d_name[0]=='.')continue;
        char path[PICK_PATH];
        if(snprintf(path,sizeof(path),"%s/%s",s_directory,entry->d_name)>=(int)sizeof(path))continue;
        struct stat st;if(stat(path,&st))continue;
        bool directory=S_ISDIR(st.st_mode);
        if(!directory&&(!S_ISREG(st.st_mode)||!book_file_path_supported(path)))continue;
        if(skip){--skip;continue;}
        if(s_count==PICK_ROWS){s_more=true;break;}
        snprintf(s_items[s_count].name,sizeof(s_items[s_count].name),"%s",entry->d_name);
        s_items[s_count++].directory=directory;
    }
    closedir(dir);
    if(failed){s_count=0;s_more=false;snprintf(s_notice,sizeof(s_notice),"Không thể đọc thư mục. Hãy mở lại trang.");}
    else if(!s_count)snprintf(s_notice,sizeof(s_notice),"Không có sách TXT / EPUB trong thư mục này");
}
static void enter(app_ctx_t *ctx) {
    (void)ctx;
    if(!card_path(s_directory)) {
        const char *root=app_settings_books_dir();
        struct stat st;
        snprintf(s_directory,sizeof(s_directory),"%s",card_path(root)&&!stat(root,&st)&&S_ISDIR(st.st_mode)?root:"/sdcard");
    }
    s_page=0;s_pending=true;s_count=0;s_more=false;s_notice[0]=0;
    read_pico_sd_start_probe();
}
static void leave(app_ctx_t *ctx) {
    (void)ctx;free(s_items);s_items=NULL;s_count=0;s_pending=false;
}
static void lost(app_ctx_t *ctx) {
    leave(ctx);snprintf(s_directory,sizeof(s_directory),"/sdcard");
    snprintf(s_notice,sizeof(s_notice),"Thẻ nhớ chưa sẵn sàng");s_more=false;s_page=0;
}
static void ready(app_ctx_t *ctx) { (void)ctx;s_pending=true; }
static app_redraw_t tick(app_ctx_t *ctx) {
    (void)ctx;
    if(!s_pending)return APP_REDRAW_NONE;
    read_pico_sd_info_t info={0};
    if(read_pico_sd_get_info(&info)==ESP_ERR_NOT_FINISHED)return APP_REDRAW_NONE;
    s_pending=false;scan_page();return APP_REDRAW_PAGE;
}
static void render(app_ctx_t *ctx,uint8_t *fb) {
    (void)ctx;ui_clear_page(fb);ui_nav_status(fb);ui_nav_back(fb,36,78);
    ui_text(fb,122,91,46,"Mở sách",EPD_DRAW_ALIGN_LEFT,false);
    ui_text_caption(fb,(EpdRect){36,172,612,58},20,s_directory,EPD_DRAW_ALIGN_LEFT,false);
    for(unsigned i=0;i<s_count;++i) {
        int top=PICK_TOP+i*PICK_ROW_H;
        ui_draw_icon(fb,65,top+38,30,s_items[i].directory?UI_ICON_FOLDER_OPEN:UI_ICON_BOOK_OPEN,0);
        ui_text_caption(fb,(EpdRect){96,top+6,552,70},25,s_items[i].name,EPD_DRAW_ALIGN_LEFT,false);
        ui_draw_separator(fb,top+PICK_ROW_H-1,36,612,0xc0);
    }
    if(!s_count)ui_text_caption(fb,(EpdRect){48,360,588,150},25,s_pending?"Đang đọc thẻ nhớ…":s_notice,EPD_DRAW_ALIGN_CENTER,false);
    char page[48];snprintf(page,sizeof(page),"Trang %u",s_page+1);
    ui_text_box(fb,page_label(),23,page,EPD_DRAW_ALIGN_CENTER,false);
    if(s_page)ui_draw_button(fb,page_button(0),"‹ Trước",false);
    if(s_more)ui_draw_button(fb,page_button(1),"Tiếp ›",false);
    ui_nav_draw(fb,-1);
}
static app_redraw_t back(app_ctx_t *ctx) {
    if(!strcmp(s_directory,"/sdcard")){ui_nav_request(ctx,0);return APP_REDRAW_NONE;}
    char *slash=strrchr(s_directory,'/');if(slash)*slash=0;
    if(!card_path(s_directory))snprintf(s_directory,sizeof(s_directory),"/sdcard");
    s_page=0;s_pending=true;s_count=0;s_more=false;return APP_REDRAW_PAGE;
}
static app_redraw_t gesture(app_ctx_t *ctx,const ui_gesture_event_t *ev) {
    if(ev->type!=UI_GESTURE_TAP)return APP_REDRAW_NONE;
    unsigned x=ev->x0,y=ev->y0;
    int tab=ui_nav_hit(x,y);if(tab>=0){ui_nav_request(ctx,tab);return APP_REDRAW_NONE;}
    if(x<110&&y>=76&&y<146)return back(ctx);
    if(s_pending)return APP_REDRAW_NONE;
    if(page_hit(page_button(0),x,y)||page_hit(page_button(1),x,y)) {
        if(page_hit(page_button(0),x,y)&&s_page)--s_page;
        else if(page_hit(page_button(1),x,y)&&s_more)++s_page;
        else return APP_REDRAW_NONE;
        s_pending=true;s_count=0;s_more=false;return APP_REDRAW_PAGE;
    }
    if(y<PICK_TOP||y>=PICK_TOP+PICK_ROWS*PICK_ROW_H||x<36||x>=648)return APP_REDRAW_NONE;
    unsigned row=(y-PICK_TOP)/PICK_ROW_H;if(row>=s_count||!s_items)return APP_REDRAW_NONE;
    char path[PICK_PATH];
    if(snprintf(path,sizeof(path),"%s/%s",s_directory,s_items[row].name)>=(int)sizeof(path))return APP_REDRAW_NONE;
    if(s_items[row].directory) {
        snprintf(s_directory,sizeof(s_directory),"%s",path);s_page=0;s_count=0;s_more=false;s_pending=true;
        return APP_REDRAW_PAGE;
    }
    if(app_book_request_open_from_home(path))ctx->request_app=app_at(1);
    return APP_REDRAW_NONE;
}
static app_redraw_t key(app_ctx_t *ctx,int key) {
    if(key==UI_KEY_2){ui_nav_request(ctx,0);return APP_REDRAW_NONE;}
    return back(ctx);
}
static bool no_handle(app_ctx_t *ctx) {(void)ctx;return false;}
const app_desc_t app_book_picker={
    .title="Mở sách",.detail="Chọn sách TXT / EPUB",.owns_keys=true,.enter_full=true,
    .menu_handle_enabled=no_handle,.persistent_navigation=true,
    .on_enter=enter,.on_exit=leave,.on_media_lost=lost,.on_media_ready=ready,
    .render=render,.on_tick=tick,.on_gesture=gesture,.on_key=key,
};
