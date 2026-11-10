/* SPDX-License-Identifier: Apache-2.0
 * Long-press text selects instead of opening TOC; TOC remains in reader tools.
 * Geometry and annotations are restricted to horizontal text in this first version.
 * A tap on an EPUB footnote reference opens its text in a small popup instead of jumping pages.
 */
#include "book_notes_ui.h"
#include "book_annotations.h"
#include "book_annotation_exchange.h"
#include "book_dictionary.h"
#include "ui_keyboard.h"
#include "ui_kit.h"
#include "ttf_font.h"
#include "esp_heap_caps.h"
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>
#ifndef NOTES_ROOT
#define NOTES_ROOT "/sdcard/.kiikoread-notes"
#endif
#ifndef DICT_ROOT
#define DICT_ROOT "/sdcard/dict"
#endif
enum { CLOSED, SELECT, EDIT_NOTE, LOOKUP, LIST, FOOTNOTE };
enum { DEFINITION_PX=32, DEFINITION_LINE_H=44, DEFINITION_LINES=10, NOTES_LIST_ROWS=7, NOTES_LIST_ROW_H=80 };
// 脚注引用的触控外扩，编号字很小。/ Touch slop around tiny footnote markers.
enum { FOOTNOTE_SLOP=8, FOOTNOTE_TEXT_X=62, FOOTNOTE_BUTTON_H=58 };
typedef struct {
    book_annotations_t annotations, committed;
    book_dictionary_t dictionary;
    const char *text;
    size_t length,start,end,anchor_start,anchor_end,page;
    uint32_t generation;
    int mode,list_at,definition_page,menu_y;
    bool valid,dragging,searching,unsupported;
    char draft[BOOK_NOTE_CAP],query[256],message[160];
    ui_text_edit_t edit;
    DIR *root,*child;
    char child_path[512],book_path[512],storage_root[512];
    const html_link_t *links;
    size_t link_count;
    book_notes_footnote_fn footnote_load;
    char *footnote;
    unsigned footnote_lines;
    EpdRect footnote_panel;
} notes_t;
static notes_t *s;
static const EpdRect dictionary_panel={36,240,612,650};
static const EpdRect notes_list_panel={24,240,636,700};
static void close_dirs(void) {
    if(s->child)closedir(s->child);
    if(s->root)closedir(s->root);
    s->root=s->child=NULL;s->searching=false;
}
void book_notes_cancel(void) {
    if(!s)return;
    ui_gesture_enable_held_drag(false);
    if(s->mode==EDIT_NOTE)ui_keyboard_end();
    book_dictionary_close(&s->dictionary);close_dirs();s->mode=CLOSED;s->dragging=false;
    free(s->footnote);s->footnote=NULL;
}
void book_notes_reset(void) { if(!s)return;book_notes_cancel();free(s);s=NULL; }
bool book_notes_active(void) {return s&&s->mode!=CLOSED;}
bool book_notes_open_list(size_t page) {
    if(!s)return false;
    book_notes_cancel();s->mode=LIST;s->list_at=0;s->page=page;
    s->generation=book_layout_generation();return true;
}
size_t book_notes_page(size_t fallback) {return book_notes_active()?s->page:fallback;}
void book_notes_bind(const char *path,uint32_t chapter,const char *text,size_t length) {
    book_notes_reset();
    s=heap_caps_calloc(1,sizeof(*s),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!s)return;
    s->text=text;s->length=length;
    if(strlen(path)>=sizeof(s->book_path))return;
    strcpy(s->book_path,path);
    char directory[512];
    if(!book_annotation_directory(path,directory,sizeof(directory))||
       snprintf(s->storage_root,sizeof(s->storage_root),"%s/snapshots",directory)>=(int)sizeof(s->storage_root))return;
    s->valid=book_annotations_load(&s->annotations,s->storage_root,"",chapter,text,length);
    /* Prefer RC3..5 sidecar snapshots, then RC2. Old files stay untouched.
     * Preserve the snapshot sequence while committing the migrated copy. */
    if(s->valid&&!s->annotations.sequence) {
        book_annotations_t *legacy=heap_caps_calloc(1,sizeof(*legacy),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
        if(!legacy){s->valid=false;return;}
        char sidecar[512],old_root[540];
        bool ok=book_annotation_sidecar(path,sidecar,sizeof(sidecar))&&
            snprintf(old_root,sizeof(old_root),"%s/kiikoread",sidecar)<(int)sizeof(old_root);
        if(ok)ok=book_annotations_load(legacy,old_root,"",chapter,text,length);
        if(ok&&!legacy->sequence)ok=book_annotations_load(legacy,NOTES_ROOT,path,chapter,text,length);
        if(!ok){free(legacy);s->valid=false;return;}
        if(legacy->sequence) {
            s->annotations.count=legacy->count;memcpy(s->annotations.items,legacy->items,sizeof(legacy->items));
            s->annotations.sequence=legacy->sequence-1;
            s->valid=book_annotations_save(&s->annotations,s->storage_root);
        }
        free(legacy);
    }
    s->committed=s->annotations;
}
void book_notes_bind_links(const html_link_t *links,size_t count,book_notes_footnote_fn load) {
    if(!s)return;
    s->links=links;s->link_count=links?count:0;s->footnote_load=load;
}
static bool hit(EpdRect r,int x,int y){return x>=r.x&&y>=r.y&&x<r.x+r.width&&y<r.y+r.height;}
static ui_icon_t action_icon(const char *label) {
    if(!strcmp(label,"Tra từ"))return UI_ICON_SEARCH;
    if(!strcmp(label,"Tô sáng"))return UI_ICON_HIGHLIGHTER;
    if(!strcmp(label,"Ghi chú"))return UI_ICON_PENCIL;
    if(!strcmp(label,"Lưu ghi chú"))return UI_ICON_SAVE;
    if(!strcmp(label,"Các ghi chú"))return UI_ICON_LIST;
    if(!strcmp(label,"Trước")||!strcmp(label,"Trang trước"))return UI_ICON_CHEVRON_LEFT;
    if(!strcmp(label,"Sau")||!strcmp(label,"Trang sau")||!strcmp(label,"Từ điển kế"))return UI_ICON_CHEVRON_RIGHT;
    return UI_ICON_X;
}
static void button(uint8_t *fb,EpdRect r,const char *label) {
    ui_fill_round_rect(fb,r,12,0xe0);
    ui_draw_icon(fb,r.x+20,r.y+r.height/2,22,action_icon(label),0);
    r.x+=36;r.width-=40;ui_text_box(fb,r,25,label,EPD_DRAW_ALIGN_CENTER,false);
}
static EpdRect menu_action_rect(int action) {
    int y=s->mode==SELECT?s->menu_y:0;
    return (EpdRect){26+action*126,y+4,122,64};
}
typedef struct {int top,bottom;bool found;} selection_bounds_t;
static void selection_bounds(void *ctx,size_t a,size_t b,EpdRect r) {
    selection_bounds_t *bounds=ctx;
    if(a>=s->end||b<=s->start)return;
    if(!bounds->found||r.y<bounds->top)bounds->top=r.y;
    if(!bounds->found||r.y+r.height>bounds->bottom)bounds->bottom=r.y+r.height;
    bounds->found=true;
}
static void place_menu(size_t page,EpdRect body,int px) {
    selection_bounds_t bounds={0};
    book_layout_visit_page(page,body,px,selection_bounds,&bounds);
    s->menu_y=0; /* Reserved status band is always outside body (starts at 80). */
    if(!bounds.found)return;
    if(bounds.bottom+84<=body.y+body.height)s->menu_y=bounds.bottom+8;
    else if(bounds.top-84>=body.y)s->menu_y=bounds.top-80;
}
static size_t previous(size_t at) {if(!at)return 0;--at;while(at&&((unsigned char)s->text[at]&0xc0)==0x80)--at;return at;}
static size_t next(size_t at) {return ui_text_edit_next(s->text,at);}
static uint32_t cp(size_t at) {
    const unsigned char *p=(const unsigned char *)s->text+at;uint32_t c=*p++;
    if(c<128)return c;
    unsigned n=c<0xe0?1:c<0xf0?2:3;c&=n==1?31:n==2?15:7;
    while(n--&&(size_t)(p-(const unsigned char *)s->text)<s->length)c=(c<<6)|(*p++&63);
    return c;
}
static bool word(size_t at) {
    uint32_t c=cp(at);
    return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='\''||
        (c>=0xc0&&c<=0x2af)||(c>=0x300&&c<=0x36f)||(c>=0x1e00&&c<=0x1eff);
}
static void range(size_t at,size_t *a,size_t *b) {
    *a=at;*b=next(at);
    if(!word(at))return;
    while(*a&&word(previous(*a)))*a=previous(*a);
    while(*b<s->length&&word(*b))*b=next(*b);
}
typedef struct {int x,y;size_t at;bool found;} pick_t;
static void pick(void *ctx,size_t a,size_t b,EpdRect r) {
    (void)b;pick_t *p=ctx;if(hit(r,p->x,p->y)){p->at=a;p->found=true;}
}
static bool select_at(size_t page,EpdRect body,int px,int x,int y,size_t *a,size_t *b) {
    pick_t p={.x=x,.y=y};
    if(!book_layout_visit_page(page,body,px,pick,&p)||!p.found||p.at>=s->length)return false;
    range(p.at,a,b);return true;
}
typedef struct {uint8_t *fb;EpdRect clip;} underline_t;
static void underline(void *ctx,size_t a,size_t b,EpdRect r) {
    underline_t *paint=ctx;uint8_t *fb=paint->fb;bool selected=s->mode&&a<s->end&&b>s->start;
    bool saved=false;
    for(uint32_t i=0;i<s->annotations.count;i++)if(a<s->annotations.items[i].end&&b>s->annotations.items[i].start){saved=true;break;}
    if(selected||saved){
        r.y+=r.height-2;r.height=selected?3:2;
        int right=r.x+r.width;
        if(r.x<paint->clip.x)r.x=paint->clip.x;
        if(right>paint->clip.x+paint->clip.width)right=paint->clip.x+paint->clip.width;
        r.width=right-r.x;
        if(r.y+r.height>paint->clip.y+paint->clip.height)r.y=paint->clip.y+paint->clip.height-r.height;
        if(r.width>0&&r.y>=paint->clip.y)epd_fill_rect(r,selected?0x00:0x60,fb);
    }
}
/* Page text by explicit bounded UTF-8 lines, avoiding an unbounded text-box layout.
 * A NULL framebuffer counts every line instead of drawing one page.
 * Wrap at the last space so Vietnamese words stay whole; a spaceless row (CJK, long token) breaks per glyph. */
static unsigned definition_draw(uint8_t *fb,const char *t,int x,int y) {
    size_t at=0;unsigned line=0,first=(unsigned)s->definition_page*DEFINITION_LINES;
    while(t[at]&&(!fb||line<first+DEFINITION_LINES)) {
        char row[192];size_t used=0,cut_used=0,cut_at=0;int width=0;
        while(t[at]&&t[at]!='\n'&&used+5<sizeof(row)) {
            size_t end=ui_text_edit_next(t,at),n=end-at;char glyph[5]={0};
            if(n>4)return line;
            memcpy(glyph,t+at,n);
            int w=ttf_text_width_px(DEFINITION_PX,glyph);
            if(width+w>560&&used) {
                if(t[at]==' ')at=end;
                else if(cut_used){used=cut_used;at=cut_at;}
                break;
            }
            if(t[at]==' '&&used){cut_used=used;cut_at=end;}
            memcpy(row+used,t+at,n);used+=n;at=end;width+=w;
        }
        row[used]=0;if(t[at]=='\n')++at;
        if(fb&&line>=first)ui_text_box(fb,(EpdRect){x,y+(int)(line-first)*DEFINITION_LINE_H,560,DEFINITION_LINE_H},DEFINITION_PX,row,EPD_DRAW_ALIGN_LEFT,false);
        ++line;
    }
    return line;
}
/* ---- 脚注弹窗 / Footnote popup ---- */
static EpdRect footnote_button(int index) {
    EpdRect p=s->footnote_panel;
    return (EpdRect){p.x+12+index*190,p.y+p.height-FOOTNOTE_BUTTON_H-14,index==2?p.width-24-380:178,FOOTNOTE_BUTTON_H};
}
static bool footnote_paged(void) {return s->footnote_lines>DEFINITION_LINES;}
// 引用表按正文顺序；二分找包含该字节的链接。/ References are in text order; binary-search the one containing the byte.
static size_t link_find(size_t at) {
    size_t lo=0,hi=s->link_count;
    while(lo<hi) {
        size_t mid=lo+(hi-lo)/2;
        if(s->links[mid].end<=at)lo=mid+1;
        else if(s->links[mid].start>at)hi=mid;
        else return mid;
    }
    return SIZE_MAX;
}
typedef struct {int x,y,best,glyph_y;size_t link;bool on_body_text;} link_pick_t;
static void link_pick(void *ctx,size_t a,size_t b,EpdRect r) {
    (void)b;link_pick_t *p=ctx;
    size_t i=link_find(a);
    if(i==SIZE_MAX) {
        if(hit(r,p->x,p->y))p->on_body_text=true;
        return;
    }
    int dx=p->x<r.x?r.x-p->x:p->x>=r.x+r.width?p->x-r.x-r.width+1:0;
    int dy=p->y<r.y?r.y-p->y:p->y>=r.y+r.height?p->y-r.y-r.height+1:0;
    if(dx>FOOTNOTE_SLOP||dy>FOOTNOTE_SLOP)return;
    int d=dx*dx+dy*dy;
    if(p->link==SIZE_MAX||d<p->best){p->best=d;p->link=i;p->glyph_y=r.y;}
}
// 弹窗避开被点的引用：引用在上半屏就放下方，否则放上方；高度随行数收缩。
// Keep the popup clear of the tapped reference: below it in the upper half, above otherwise; height follows the lines.
static bool footnote_open(size_t page,EpdRect body,int px,int x,int y) {
    if(!s->link_count||!s->footnote_load||book_layout_vertical())return false;
    const size_t first=book_layout_page_start_offset(page);
    const size_t end=page+1<book_layout_page_count()?book_layout_page_start_offset(page+1):s->length;
    size_t lo=0,hi=s->link_count;
    while(lo<hi) {
        const size_t mid=lo+(hi-lo)/2;
        if(s->links[mid].end<=first)lo=mid+1;
        else hi=mid;
    }
    // Chương có chú thích ở trang khác không cần duyệt lại chữ của trang đang chạm.
    if(lo==s->link_count||s->links[lo].start>=end)return false;
    link_pick_t p={.x=x,.y=y,.link=SIZE_MAX};
    if(!book_layout_visit_page(page,body,px,link_pick,&p)||p.link==SIZE_MAX)return false;
    // Dung sai chỉ áp dụng ở khoảng trống, không lấn vào chữ bên cạnh.
    if(p.best!=0&&p.on_body_text)return false;
    char *text=NULL;size_t length=0;
    if(s->footnote_load(s->links[p.link].href,&text,&length)!=ESP_OK||!text||!length){free(text);return false;}
    free(s->footnote);s->footnote=text;
    s->definition_page=0;
    s->footnote_lines=definition_draw(NULL,text,0,0);
    unsigned lines=s->footnote_lines<DEFINITION_LINES?s->footnote_lines:DEFINITION_LINES;
    if(!lines)lines=1;
    int height=24+(int)lines*DEFINITION_LINE_H+16+FOOTNOTE_BUTTON_H+14;
    int top=p.glyph_y<body.y+body.height/2?body.y+body.height-height-8:body.y+8;
    s->footnote_panel=(EpdRect){36,top,612,height};
    s->start=s->links[p.link].start;s->end=s->links[p.link].end;
    s->mode=FOOTNOTE;s->page=page;s->generation=book_layout_generation();s->message[0]=0;
    return true;
}
static void footnote_draw(uint8_t *fb) {
    EpdRect p=s->footnote_panel;
    ui_fill_round_rect(fb,p,16,0xff);
    ui_draw_control_frame(fb,p,16,0x00);
    definition_draw(fb,s->footnote,FOOTNOTE_TEXT_X,p.y+24);
    if(footnote_paged()){button(fb,footnote_button(0),"Trang trước");button(fb,footnote_button(1),"Trang sau");}
    button(fb,footnote_button(2),"Đóng");
}
static void footnote_gesture(const ui_gesture_event_t *ev,app_redraw_t *redraw) {
    int x=ev->x0,y=ev->y0;
    if(!hit(s->footnote_panel,x,y)||hit(footnote_button(2),x,y)){book_notes_cancel();*redraw=APP_REDRAW_PAGE;return;}
    unsigned pages=(s->footnote_lines+DEFINITION_LINES-1)/DEFINITION_LINES;
    if(footnote_paged()&&hit(footnote_button(0),x,y)&&s->definition_page){s->definition_page--;*redraw=APP_REDRAW_PAGE;}
    else if(footnote_paged()&&hit(footnote_button(1),x,y)&&(unsigned)s->definition_page+1<pages){s->definition_page++;*redraw=APP_REDRAW_PAGE;}
}
/* Preview only: retain the full annotation in storage and the editor. */
static void note_preview(const char *text,char out[128]) {
    size_t at=0,used=0;unsigned words=0;
    while(text[at]&&words<6) {
        while(text[at]&&((unsigned char)text[at]<=32))++at;
        if(!text[at])break;
        if(used)out[used++]=' ';
        while(text[at]&&(unsigned char)text[at]>32) {
            size_t end=ui_text_edit_next(text,at),n=end-at;
            if(used+n>120)goto done;
            memcpy(out+used,text+at,n);used+=n;at=end;
        }
        ++words;
    }
    while(text[at]&&(unsigned char)text[at]<=32)++at;
done:
    if(text[at]){memcpy(out+used,"…",3);used+=3;}
    out[used]=0;
}
void book_notes_draw(uint8_t *fb,size_t page,EpdRect body,int px) {
    if(!s)return;
    underline_t paint={fb,body};
    if(s->annotations.count||s->mode)book_layout_visit_page(page,body,px,underline,&paint);
    if(!s->mode)return;
    if(s->mode==SELECT&&s->dragging)return; /* Never cover text under the held finger. */
    if(s->mode==FOOTNOTE){footnote_draw(fb);return;}
    int menu_y=s->mode==SELECT?s->menu_y:0;
    ui_fill_round_rect(fb,(EpdRect){24,menu_y,636,72},12,0xff);
    ui_draw_control_frame(fb,(EpdRect){24,menu_y,636,72},12,0x50);
    const char *labels[]={"Tra từ","Tô sáng","Ghi chú","Các ghi chú","Đóng"};
    for(int i=0;i<5;i++) {
        EpdRect r=menu_action_rect(i);
        ui_fill_round_rect(fb,r,10,0xe0);
        ui_draw_icon(fb,r.x+r.width/2,r.y+18,24,action_icon(labels[i]),0);
        ui_text_box(fb,(EpdRect){r.x+2,r.y+36,r.width-4,26},23,labels[i],EPD_DRAW_ALIGN_CENTER,false);
    }
    if(s->message[0]) {
        ui_fill_rect_fast((EpdRect){24,1176,636,38},0xff,fb);
        ui_text_box(fb,(EpdRect){32,1178,620,32},18,s->message,EPD_DRAW_ALIGN_LEFT,false);
    }
    if(s->mode==EDIT_NOTE) {
        ui_fill_rect_fast((EpdRect){24,240,636,830},0xff,fb);
        ui_text_input_draw(fb,&s->edit,(EpdRect){36,260,612,68},24,false,NULL);
        button(fb,(EpdRect){36,350,180,58},"Lưu ghi chú");
        ui_keyboard_draw(fb,560);
    } else if(s->mode==LOOKUP) {
        ui_fill_round_rect(fb,dictionary_panel,16,0xff);
        ui_draw_control_frame(fb,dictionary_panel,16,0x00);
        if(s->dictionary.state==4)definition_draw(fb,s->dictionary.text,62,260);
        else ui_text_box(fb,(EpdRect){62,260,560,130},23,s->message,EPD_DRAW_ALIGN_LEFT,false);
        ui_text_box(fb,(EpdRect){62,720,560,60},19,s->dictionary.name,EPD_DRAW_ALIGN_LEFT,false);
        button(fb,(EpdRect){48,810,170,58},"Trang trước");button(fb,(EpdRect){230,810,170,58},"Trang sau");button(fb,(EpdRect){412,810,224,58},"Từ điển kế");
    } else if(s->mode==LIST) {
        ui_fill_round_rect(fb,notes_list_panel,16,0xff);
        ui_draw_control_frame(fb,notes_list_panel,16,0x00);
        for(int row=0;row<NOTES_LIST_ROWS;row++) {
            int i=s->list_at+row;if(i>=(int)s->annotations.count)break;
            book_annotation_t *a=&s->annotations.items[i];
            char quote[128],note[128];note_preview(a->quote,quote);note_preview(a->note,note);
            ui_text_box(fb,(EpdRect){42,252+row*NOTES_LIST_ROW_H,500,36},29,quote,EPD_DRAW_ALIGN_LEFT,false);
            ui_text_box(fb,(EpdRect){42,288+row*NOTES_LIST_ROW_H,500,36},27,note,EPD_DRAW_ALIGN_LEFT,false);
            button(fb,(EpdRect){560,260+row*NOTES_LIST_ROW_H,78,52},"Xóa");
        }
        button(fb,(EpdRect){42,844,180,58},"Trước");button(fb,(EpdRect){458,844,180,58},"Sau");
    }
}
static bool persist(void) {
    if(!s->valid){snprintf(s->message,sizeof(s->message),"Không đọc được ghi chú cũ; chưa ghi đè");return false;}
    if(!book_annotations_save(&s->annotations,s->storage_root)) {
        int error=errno;
        s->annotations=s->committed;
        snprintf(s->message,sizeof(s->message),"Không ghi được thẻ nhớ (lỗi %d). Có thể thử lại.",error);return false;
    }
    s->committed=s->annotations;
    snprintf(s->message,sizeof(s->message),"Đã lưu trên thẻ nhớ");
    return true;
}
static bool save_note(const char *note) {
    if(!s->valid)return persist();
    if(!book_annotations_put(&s->annotations,s->text,s->start,s->end,note)) {
        snprintf(s->message,sizeof(s->message),"Tối đa 32 đoạn/chương, ghi chú 127 byte");return false;
    }
    return persist();
}
static void start_lookup(void) {
    book_dictionary_close(&s->dictionary);memset(&s->dictionary,0,sizeof(s->dictionary));close_dirs();s->unsupported=false;s->definition_page=0;
    size_t n=s->end-s->start;
    if(!n||n>=sizeof(s->query)){snprintf(s->message,sizeof(s->message),"Chọn từ hoặc cụm từ dưới 256 byte");return;}
    memcpy(s->query,s->text+s->start,n);s->query[n]=0;
    s->mode=LOOKUP;s->root=opendir(DICT_ROOT);s->searching=s->root!=NULL;
    snprintf(s->message,sizeof(s->message),s->searching?"Đang tra từ trên thẻ nhớ…":"Chép bộ StarDict vào /dict trên thẻ nhớ");
}
app_redraw_t book_notes_tick(void) {
    if(s&&s->mode&&s->generation!=book_layout_generation()){book_notes_cancel();return APP_REDRAW_PAGE;}
    if(!s||s->mode!=LOOKUP||!s->searching)return APP_REDRAW_NONE;
    if((s->dictionary.state>=1&&s->dictionary.state<=3)||s->dictionary.state==5) {
        int result=book_dictionary_step(&s->dictionary);
        if(result==4){s->searching=false;snprintf(s->message,sizeof(s->message),s->dictionary.truncated?"Đã rút gọn định nghĩa dài":"Tra offline: %s",s->query);return APP_REDRAW_PAGE;}
        if(result==-1)s->unsupported=true;
        return APP_REDRAW_NONE;
    }
    /* Enumerate at most one directory entry per UI tick, one subdirectory deep. */
    DIR *dir=s->child?s->child:s->root;struct dirent *entry=readdir(dir);
    if(!entry) {
        if(s->child){closedir(s->child);s->child=NULL;return APP_REDRAW_NONE;}
        close_dirs();snprintf(s->message,sizeof(s->message),s->unsupported?"Không tìm thấy; kiểm tra đủ bộ .ifo, .idx/.idx.gz, .dict/.dict.dz (mhgxt, 32-bit)":"Không tìm thấy từ trong /dict");return APP_REDRAW_PAGE;
    }
    if(entry->d_name[0]=='.')return APP_REDRAW_NONE;
    char path[512];if(snprintf(path,sizeof(path),"%s/%s",s->child?s->child_path:DICT_ROOT,entry->d_name)>=(int)sizeof(path))return APP_REDRAW_NONE;
    struct stat st;if(stat(path,&st))return APP_REDRAW_NONE;
    if(S_ISDIR(st.st_mode)&&!s->child){snprintf(s->child_path,sizeof(s->child_path),"%s",path);s->child=opendir(path);return APP_REDRAW_NONE;}
    size_t n=strlen(path);
    if(S_ISREG(st.st_mode)&&n>4&&!strcmp(path+n-4,".ifo")) {
        if(!book_dictionary_open(&s->dictionary,path,s->query))s->unsupported=true;
    }
    return APP_REDRAW_NONE;
}
bool book_notes_gesture(app_ctx_t *ctx,const ui_gesture_event_t *ev,size_t page,EpdRect body,int px,app_redraw_t *redraw) {
    *redraw=APP_REDRAW_NONE;if(!s)return false;
    if(s->mode&& (page!=s->page||s->generation!=book_layout_generation()))book_notes_cancel();
    if(!s->mode) {
        if(ev->type==UI_GESTURE_TAP&&hit(body,ev->x0,ev->y0)&&footnote_open(page,body,px,ev->x0,ev->y0)){*redraw=APP_REDRAW_PAGE;return true;}
        if(ev->type!=UI_GESTURE_LONG_PRESS||!hit(body,ev->x0,ev->y0)||book_layout_vertical())return false;
        if(!select_at(page,body,px,ev->x0,ev->y0,&s->start,&s->end))return false;
        s->anchor_start=s->start;s->anchor_end=s->end;s->mode=SELECT;s->page=page;s->generation=book_layout_generation();s->dragging=true;s->message[0]=0;place_menu(page,body,px);ui_gesture_enable_held_drag(true);
        *redraw=APP_REDRAW_PAGE;return true;
    }
    if(s->mode==FOOTNOTE) {
        // 弹窗只吞轻点和按住类事件；滑动关掉弹窗后照常翻页。/ The popup keeps taps and holds; a swipe closes it and turns pages as usual.
        if(ev->type==UI_GESTURE_TAP)footnote_gesture(ev,redraw);
        else if(ev->type!=UI_GESTURE_PRESS&&ev->type!=UI_GESTURE_MOVE&&ev->type!=UI_GESTURE_LONG_PRESS&&ev->type!=UI_GESTURE_CANCEL){book_notes_cancel();return false;}
        return true;
    }
    if(ev->type==UI_GESTURE_CANCEL){if(s->dragging){s->dragging=false;place_menu(page,body,px);*redraw=APP_REDRAW_PAGE;}return true;}
    if(ev->type==UI_GESTURE_MOVE&&s->dragging) {
        size_t a,b;if(select_at(page,body,px,ev->x,ev->y,&a,&b)) {
            size_t start=a<s->anchor_start?a:s->anchor_start,end=b>s->anchor_end?b:s->anchor_end;
            if(start!=s->start||end!=s->end){s->start=start;s->end=end;place_menu(page,body,px);*redraw=APP_REDRAW_PAGE;}
        }return true;
    }
    if(ev->type!=UI_GESTURE_TAP&&ev->type!=UI_GESTURE_LONG_PRESS)return true;
    int x=ev->x,y=ev->y;s->dragging=false;
    if((s->mode==LIST||s->mode==LOOKUP)&&ev->type==UI_GESTURE_TAP&&
       !hit(s->mode==LOOKUP?dictionary_panel:notes_list_panel,x,y)&&
       !hit((EpdRect){24,0,636,72},x,y)) {
        book_notes_cancel();*redraw=APP_REDRAW_PAGE;return true;
    }
    if(ev->type==UI_GESTURE_TAP&&hit(menu_action_rect(4),x,y)){book_notes_cancel();*redraw=APP_REDRAW_PAGE;return true;}
    if(s->mode==EDIT_NOTE) {
        if(hit((EpdRect){36,350,180,58},x,y)) {if(!ui_keyboard_pending()&&save_note(s->draft)){ui_keyboard_end();s->mode=SELECT;place_menu(page,body,px);}}
        else {
            ui_text_input_tap(&s->edit,(EpdRect){36,260,612,68},24,false,NULL,x,y);
            if(ui_keyboard_tap(x,y,560,ctx->now_ms)==UI_KEYBOARD_DONE&&
               !ui_keyboard_pending()&&save_note(s->draft))book_notes_cancel();
        }
        *redraw=APP_REDRAW_PAGE;return true;
    }
    if(ev->type==UI_GESTURE_TAP&&hit(menu_action_rect(0),x,y))start_lookup();
    else if(ev->type==UI_GESTURE_TAP&&hit(menu_action_rect(1),x,y)) {
        const char *note="";
        for(uint32_t i=0;i<s->annotations.count;i++)if(s->annotations.items[i].start==s->start&&s->annotations.items[i].end==s->end)note=s->annotations.items[i].note;
        /* put clears its destination, so copy an existing note before updating. */
        snprintf(s->draft,sizeof(s->draft),"%s",note);if(save_note(s->draft))book_notes_cancel();
    }
    else if(ev->type==UI_GESTURE_TAP&&hit(menu_action_rect(2),x,y)) {
        s->draft[0]=0;
        for(uint32_t i=0;i<s->annotations.count;i++)if(s->annotations.items[i].start==s->start&&s->annotations.items[i].end==s->end)snprintf(s->draft,sizeof(s->draft),"%s",s->annotations.items[i].note);
        book_dictionary_close(&s->dictionary);close_dirs();s->mode=EDIT_NOTE;ui_text_edit_init(&s->edit,s->draft,sizeof(s->draft));ui_keyboard_begin(&s->edit,false);
    } else if(ev->type==UI_GESTURE_TAP&&hit(menu_action_rect(3),x,y)){book_dictionary_close(&s->dictionary);close_dirs();s->mode=LIST;s->list_at=0;}
    else if(s->mode==LOOKUP) {
        if(hit((EpdRect){48,810,170,58},x,y)&&s->definition_page)s->definition_page--;
        else if(hit((EpdRect){230,810,170,58},x,y)&&s->definition_page<50)s->definition_page++;
        else if(hit((EpdRect){412,810,224,58},x,y)){s->searching=s->root!=NULL;s->dictionary.state=0;s->definition_page=0;snprintf(s->message,sizeof(s->message),s->searching?"Đang tra từ điển kế…":"Đã hết các từ điển trong /dict");}
    } else if(s->mode==LIST) {
        if(hit((EpdRect){42,844,180,58},x,y)&&s->list_at>=NOTES_LIST_ROWS)s->list_at-=NOTES_LIST_ROWS;
        else if(hit((EpdRect){458,844,180,58},x,y)&&s->list_at+NOTES_LIST_ROWS<(int)s->annotations.count)s->list_at+=NOTES_LIST_ROWS;
        else for(int row=0;row<NOTES_LIST_ROWS;row++) {
            int i=s->list_at+row;if(i>=(int)s->annotations.count)break;
            if(hit((EpdRect){560,260+row*NOTES_LIST_ROW_H,78,52},x,y)&&s->valid) {
                memmove(&s->annotations.items[i],&s->annotations.items[i+1],(s->annotations.count-(unsigned)i-1)*sizeof(s->annotations.items[0]));--s->annotations.count;persist();break;
            }
            if(hit((EpdRect){42,252+row*NOTES_LIST_ROW_H,500,72},x,y)) {
                s->start=s->annotations.items[i].start;s->end=s->annotations.items[i].end;s->mode=SELECT;
                s->page=book_layout_page_for_offset(s->start);place_menu(s->page,body,px);
                snprintf(s->message,sizeof(s->message),"%s",s->annotations.items[i].note);break;
            }
        }
    } else if(hit(body,x,y)) {
        size_t a,b;if(select_at(page,body,px,x,y,&a,&b)) {
            if(ev->type==UI_GESTURE_LONG_PRESS){s->anchor_start=a;s->anchor_end=b;s->dragging=true;ui_gesture_enable_held_drag(true);}
            s->start=a;s->end=b;place_menu(page,body,px);
        }
    }
    *redraw=APP_REDRAW_PAGE;return true;
}
