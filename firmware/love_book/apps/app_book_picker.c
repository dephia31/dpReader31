/* SPDX-License-Identifier: Apache-2.0
 * 用户2026-11-06：Love Book默认打开Books，提供目录导航和刷新，不依赖书架。
 * User revision 2026-11-06: Love Book opens Books with navigation and refresh, independently of the shelf.
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
#include <unistd.h>
#include "app.h"
#include "app_registry.h"
#include "app_content_open.h"
#include "app_transfer_mode.h"
#include "book_file_path.h"
#if DPREADER_LOVE_BOOK
#include "book_progress.h"
#include "book_store.h"
#include "book_title.h"
#endif
#include "read_pico_sd.h"
#include "settings.h"
#include "ui_gesture.h"
#include "ui_kit.h"
#include "ui_nav.h"
#include "ui_menu.h"
#include "esp_heap_caps.h"

#define PICK_ROWS 8
#define PICK_PATH 288
#define PICK_TOP 308
#define PICK_ROW_H 84

typedef struct { char name[256]; bool directory; uint64_t bytes; } picker_item_t;
static picker_item_t *s_items;
static char s_directory[PICK_PATH];
static char s_notice[96];
static unsigned s_page,s_count;
static bool s_more,s_pending,s_default_directory,s_return_from_usb;
extern const app_desc_t app_book_picker;

#if DPREADER_LOVE_BOOK
static void scan_page(void);
static char s_delete_path[PICK_PATH];
static struct stat s_delete_stat;

static EpdRect delete_button(int top) { return (EpdRect){532, top + 9, 116, 66}; }
static EpdRect delete_choice(bool confirm) {
    return (EpdRect){confirm ? 354 : 48, 700, 282, 76};
}
static void cancel_delete(app_ctx_t *ctx) {
    (void)ctx;
    s_delete_path[0] = 0;
}
static app_redraw_t confirm_delete(void) {
    struct stat current;
    bool removed = read_pico_sd_io_ready() &&
        stat(s_delete_path, &current) == 0 && S_ISREG(current.st_mode) &&
        current.st_size == s_delete_stat.st_size && current.st_mtime == s_delete_stat.st_mtime &&
        unlink(s_delete_path) == 0;
    if (removed) {
        (void)book_progress_forget(s_delete_path);
        (void)book_title_clear(s_delete_path);
        book_store_notify_changed();
    }
    s_delete_path[0] = 0;
    s_page = 0;
    scan_page();
    snprintf(s_notice, sizeof(s_notice), "%s", removed ? "Đã xóa sách" : "Không thể xóa. Kiểm tra lại thẻ nhớ.");
    return APP_REDRAW_PAGE;
}
#endif

static EpdRect page_button(unsigned i) {return (EpdRect){i?492:36,1006,156,72};}
static EpdRect page_label(void) {return (EpdRect){208,1006,268,72};}
static bool page_hit(EpdRect box,unsigned x,unsigned y) {
    return x>=(unsigned)box.x&&x<(unsigned)(box.x+box.width)&&y>=(unsigned)box.y&&y<(unsigned)(box.y+box.height);
}
static EpdRect usb_button(void) {return (EpdRect){466,80,182,72};}
static EpdRect tool_button(unsigned i) {return (EpdRect){36+(int)i*208,236,196,60};}
static bool card_path(const char *path) {
    if(!path || (strcmp(path,"/sdcard") && strncmp(path,"/sdcard/",8)))return false;
    return book_file_path_visible(path);
}
static void default_directory(void) {
    const char *choices[]={"/sdcard/Books","/sdcard/books",app_settings_books_dir(),"/sdcard"};
    snprintf(s_directory,sizeof(s_directory),"/sdcard");
    for(unsigned i=0;i<sizeof(choices)/sizeof(choices[0]);++i) {
        struct stat st;const char *path=choices[i];
        if(card_path(path)&&strlen(path)<sizeof(s_directory)&&!stat(path,&st)&&S_ISDIR(st.st_mode)) {
            snprintf(s_directory,sizeof(s_directory),"%s",path);break;
        }
    }
    s_default_directory=false;
}
static app_redraw_t reload(bool books) {
    s_default_directory=books;s_page=0;s_count=0;s_more=false;s_pending=true;s_notice[0]=0;
    return APP_REDRAW_PAGE;
}
static app_redraw_t change_page(bool next) {
    if(s_pending || (next?!s_more:!s_page))return APP_REDRAW_NONE;
    if(next)++s_page;else --s_page;
    s_pending=true;s_count=0;s_more=false;s_notice[0]=0;
    return APP_REDRAW_PAGE;
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
        if(!book_file_path_visible(path))continue;
        struct stat st;if(stat(path,&st))continue;
        bool directory=S_ISDIR(st.st_mode);
        if(!directory&&(!S_ISREG(st.st_mode)||!book_file_path_supported(path)))continue;
        if(skip){--skip;continue;}
        if(s_count==PICK_ROWS){s_more=true;break;}
        snprintf(s_items[s_count].name,sizeof(s_items[s_count].name),"%s",entry->d_name);
        s_items[s_count].bytes=st.st_size>0?(uint64_t)st.st_size:0;
        s_items[s_count++].directory=directory;
    }
    closedir(dir);
    if(failed){s_count=0;s_more=false;snprintf(s_notice,sizeof(s_notice),"Không thể đọc thư mục. Hãy mở lại trang.");}
    else if(!s_count)snprintf(s_notice,sizeof(s_notice),"Không có sách TXT / EPUB trong thư mục này");
}
static void enter(app_ctx_t *ctx) {
    (void)ctx;
    reload(!s_return_from_usb||!card_path(s_directory));
    s_return_from_usb=false;
    read_pico_sd_start_probe();
}
static void leave(app_ctx_t *ctx) {
    (void)ctx;
#if DPREADER_LOVE_BOOK
    cancel_delete(ctx);
#endif
    free(s_items);s_items=NULL;s_count=0;s_pending=false;
}
static void lost(app_ctx_t *ctx) {
    leave(ctx);snprintf(s_directory,sizeof(s_directory),"/sdcard");
    snprintf(s_notice,sizeof(s_notice),"Thẻ nhớ chưa sẵn sàng");s_more=false;s_page=0;s_default_directory=true;
}
static void ready(app_ctx_t *ctx) {
    (void)ctx;
#if DPREADER_LOVE_BOOK
    cancel_delete(ctx);
#endif
    s_pending=true;
}
static app_redraw_t tick(app_ctx_t *ctx) {
    (void)ctx;
    if(!s_pending)return APP_REDRAW_NONE;
    read_pico_sd_info_t info={0};
    if(read_pico_sd_get_info(&info)==ESP_ERR_NOT_FINISHED)return APP_REDRAW_NONE;
    if(read_pico_sd_io_ready()&&s_default_directory)default_directory();
    s_pending=false;scan_page();return APP_REDRAW_PAGE;
}
static void render(app_ctx_t *ctx,uint8_t *fb) {
    (void)ctx;ui_clear_page(fb);ui_nav_status(fb);ui_nav_back(fb,36,78);
#if DPREADER_LOVE_BOOK
    if (s_delete_path[0]) {
        ui_text_caption(fb, (EpdRect){48, 310, 588, 90}, 36, "Xóa sách khỏi thẻ nhớ?", EPD_DRAW_ALIGN_CENTER, false);
        ui_text_caption(fb, (EpdRect){48, 420, 588, 160}, 24, strrchr(s_delete_path, '/') + 1, EPD_DRAW_ALIGN_CENTER, false);
        ui_draw_button(fb, delete_choice(false), "Hủy", false);
        ui_draw_button(fb, delete_choice(true), "Xóa", true);
        return;
    }
#endif
    ui_text(fb,122,91,46,"Mở sách",EPD_DRAW_ALIGN_LEFT,false);
    ui_draw_button(fb,usb_button(),"Bật USB",false);
    ui_text_caption(fb,(EpdRect){36,172,612,58},20,s_directory,EPD_DRAW_ALIGN_LEFT,false);
    ui_draw_button(fb,tool_button(0),"Books",false);
    ui_draw_button(fb,tool_button(1),"Thư mục cha",false);
    ui_draw_button(fb,tool_button(2),"Tải lại",false);
    for(unsigned i=0;i<s_count;++i) {
        int top=PICK_TOP+i*PICK_ROW_H;
        ui_draw_icon(fb,65,top+38,30,s_items[i].directory?UI_ICON_FOLDER_OPEN:UI_ICON_BOOK_OPEN,0);
        int text_width = 552;
#if DPREADER_LOVE_BOOK
        if (!s_items[i].directory) {
            text_width = 424;
            ui_draw_button(fb, delete_button(top), "Xóa", false);
        }
#endif
        ui_text_caption(fb,(EpdRect){96,top+2,text_width,46},25,s_items[i].name,EPD_DRAW_ALIGN_LEFT,false);
        char detail[64];
        if(s_items[i].directory)snprintf(detail,sizeof(detail),"Thư mục");
        else {
            const char *ext=strrchr(s_items[i].name,'.');
            const char *format=ext&&!strcasecmp(ext,".epub")?"EPUB":"TXT";
            if(s_items[i].bytes>=1024*1024)snprintf(detail,sizeof(detail),"%s · %llu.%llu MB",format,
                (unsigned long long)(s_items[i].bytes/(1024*1024)),(unsigned long long)(s_items[i].bytes%(1024*1024)*10/(1024*1024)));
            else snprintf(detail,sizeof(detail),"%s · %llu KB",format,(unsigned long long)((s_items[i].bytes+1023)/1024));
        }
        ui_text_caption(fb,(EpdRect){96,top+48,552,30},18,detail,EPD_DRAW_ALIGN_LEFT,false);
        ui_draw_separator(fb,top+PICK_ROW_H-1,36,612,0xc0);
    }
    if(s_notice[0]&&s_count)ui_text_caption(fb,(EpdRect){36,984,612,22},16,s_notice,EPD_DRAW_ALIGN_LEFT,false);
    if(!s_count)ui_text_caption(fb,(EpdRect){48,360,588,150},25,s_pending?"Đang đọc thẻ nhớ…":s_notice,EPD_DRAW_ALIGN_CENTER,false);
    char page[48];snprintf(page,sizeof(page),"Trang %u",s_page+1);
    ui_text_box(fb,page_label(),23,page,EPD_DRAW_ALIGN_CENTER,false);
    if(s_page)ui_draw_button(fb,page_button(0),"‹ Trước",false);
    if(s_more)ui_draw_button(fb,page_button(1),"Tiếp ›",false);
    ui_nav_draw(fb,-1);
}
static app_redraw_t back(app_ctx_t *ctx) {
    if(s_pending)return APP_REDRAW_NONE;
    if(!strcmp(s_directory,"/sdcard")){ui_nav_request(ctx,0);return APP_REDRAW_NONE;}
    char *slash=strrchr(s_directory,'/');if(slash)*slash=0;
    if(!card_path(s_directory))snprintf(s_directory,sizeof(s_directory),"/sdcard");
    s_page=0;s_pending=true;s_count=0;s_more=false;return APP_REDRAW_PAGE;
}
static app_redraw_t gesture(app_ctx_t *ctx,const ui_gesture_event_t *ev) {
#if DPREADER_LOVE_BOOK
    if (s_delete_path[0]) {
        if (ev->type != UI_GESTURE_TAP) return APP_REDRAW_NONE;
        if (page_hit(delete_choice(true), ev->x0, ev->y0)) return confirm_delete();
        if (page_hit(delete_choice(false), ev->x0, ev->y0) || (ev->x0 < 110 && ev->y0 >= 76 && ev->y0 < 146)) {
            cancel_delete(ctx);
            return APP_REDRAW_PAGE;
        }
        return APP_REDRAW_NONE;
    }
#endif
    if(ev->type==UI_GESTURE_SWIPE_L||ev->type==UI_GESTURE_SWIPE_R) {
        if(ev->y0<PICK_TOP||ev->y0>=PICK_TOP+PICK_ROWS*PICK_ROW_H)return APP_REDRAW_NONE;
        return change_page(ev->type==UI_GESTURE_SWIPE_L);
    }
    if(ev->type!=UI_GESTURE_TAP)return APP_REDRAW_NONE;
    unsigned x=ev->x0,y=ev->y0;
    int tab=ui_nav_hit(x,y);if(tab>=0){ui_nav_request(ctx,tab);return APP_REDRAW_NONE;}
    if(x<110&&y>=76&&y<146)return back(ctx);
    if(s_pending)return APP_REDRAW_NONE;
    if(page_hit(usb_button(),x,y)) {
        extern const app_desc_t app_transfer;
        s_return_from_usb=true;
        app_transfer_request_usb_start();ctx->request_app=&app_transfer;
        return APP_REDRAW_NONE;
    }
    if(page_hit(tool_button(0),x,y))return reload(true);
    if(page_hit(tool_button(1),x,y)) {
        if(!strcmp(s_directory,"/sdcard"))return APP_REDRAW_NONE;
        return back(ctx);
    }
    if(page_hit(tool_button(2),x,y))return reload(false);
    if(page_hit(page_button(0),x,y))return change_page(false);
    if(page_hit(page_button(1),x,y))return change_page(true);
    if(y<PICK_TOP||y>=PICK_TOP+PICK_ROWS*PICK_ROW_H||x<36||x>=648)return APP_REDRAW_NONE;
    unsigned row=(y-PICK_TOP)/PICK_ROW_H;if(row>=s_count||!s_items)return APP_REDRAW_NONE;
    char path[PICK_PATH];
    if(snprintf(path,sizeof(path),"%s/%s",s_directory,s_items[row].name)>=(int)sizeof(path))return APP_REDRAW_NONE;
    struct stat st;
    if(!read_pico_sd_io_ready()||stat(path,&st)||
       (s_items[row].directory?!S_ISDIR(st.st_mode):!S_ISREG(st.st_mode))) {
        snprintf(s_notice,sizeof(s_notice),"Tệp đã thay đổi. Bấm Tải lại.");return APP_REDRAW_PAGE;
    }
    if(s_items[row].directory) {
        snprintf(s_directory,sizeof(s_directory),"%s",path);s_page=0;s_count=0;s_more=false;s_pending=true;
        return APP_REDRAW_PAGE;
    }
#if DPREADER_LOVE_BOOK
    if (page_hit(delete_button(PICK_TOP + row * PICK_ROW_H), x, y)) {
        snprintf(s_delete_path, sizeof(s_delete_path), "%s", path);
        s_delete_stat = st;
        return APP_REDRAW_PAGE;
    }
#endif
    if(app_book_request_open_from_home(path))ctx->request_app=app_at(1);
    else {snprintf(s_notice,sizeof(s_notice),"Không thể mở sách. Hãy thử lại.");return APP_REDRAW_PAGE;}
    return APP_REDRAW_NONE;
}
static app_redraw_t key(app_ctx_t *ctx,int key) {
#if DPREADER_LOVE_BOOK
    if (s_delete_path[0]) { cancel_delete(ctx); return APP_REDRAW_PAGE; }
#endif
    if(key==UI_KEY_2){ui_nav_request(ctx,0);return APP_REDRAW_NONE;}
    return back(ctx);
}
static bool no_handle(app_ctx_t *ctx) {(void)ctx;return false;}
const app_desc_t app_book_picker={
    .title="Mở sách",.detail="Chọn sách TXT / EPUB",.owns_keys=true,.enter_full=true,
    .menu_handle_enabled=no_handle,.persistent_navigation=true,
    .on_enter=enter,.on_exit=leave,.on_media_lost=lost,.on_media_ready=ready,
#if DPREADER_LOVE_BOOK
    .on_before_lock=cancel_delete,
#endif
    .render=render,.on_tick=tick,.on_gesture=gesture,.on_key=key,
};
