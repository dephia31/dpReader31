#!/usr/bin/env python3
"""Run the actual notes controller with fake geometry/UI and real persistence/dictionary.
SPDX-License-Identifier: Apache-2.0
"""
from pathlib import Path
import subprocess
from notes_test_support import cjson_flags
from home_cover_cache_host_test import function
root=Path(__file__).resolve().parents[1]
out=root/'build/notes-ui-test';out.mkdir(parents=True,exist_ok=True)
header=r'''
#pragma once
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "book_annotations.h"
#include "book_dictionary.h"
#include "ui_text_edit.h"
typedef struct {int x,y,width,height;} EpdRect;
typedef struct {int64_t now_ms;uint8_t *fb;} app_ctx_t;
typedef enum {APP_REDRAW_NONE,APP_REDRAW_PAGE,APP_REDRAW_AREA} app_redraw_t;
typedef enum {UI_GESTURE_PRESS,UI_GESTURE_MOVE,UI_GESTURE_LONG_PRESS,UI_GESTURE_TAP,UI_GESTURE_CANCEL} gesture_type;
typedef struct {gesture_type type;int x,y,x0,y0;} ui_gesture_event_t;
enum EpdFontFlags {EPD_DRAW_ALIGN_LEFT,EPD_DRAW_ALIGN_CENTER};
typedef void (*book_layout_glyph_fn)(void*,size_t,size_t,EpdRect);
static bool held,vertical;
static unsigned generation=1;
static int writes,icon_calls,panel_frames,definition_lines;
static int definition_ys[10];
static int glyph_y=300;
typedef enum {UI_ICON_SEARCH,UI_ICON_HIGHLIGHTER,UI_ICON_PENCIL,UI_ICON_SAVE,UI_ICON_LIST,UI_ICON_CHEVRON_LEFT,UI_ICON_CHEVRON_RIGHT,UI_ICON_X,UI_ICON_REFRESH_CW} ui_icon_t;
static void ui_draw_icon(uint8_t *fb,int x,int y,int size,ui_icon_t icon,uint8_t color){(void)fb;(void)x;(void)y;(void)size;(void)icon;(void)color;++icon_calls;}
static const char *test_text="apple banana carrot";
static bool book_layout_vertical(void){return vertical;}
static uint32_t book_layout_generation(void){return generation;}
static size_t book_layout_page_for_offset(size_t off){(void)off;return 0;}
static bool book_layout_visit_page(size_t page,EpdRect body,int px,book_layout_glyph_fn fn,void *ctx){(void)page;(void)body;(void)px;for(size_t i=0;i<strlen(test_text);i++)fn(ctx,i,i+1,(EpdRect){(int)i*20+40,glyph_y,20,30});return true;}
static void ui_gesture_enable_held_drag(bool on){held=on;}
static void ui_keyboard_end(void){}
static void ui_keyboard_begin(ui_text_edit_t *e,bool a){(void)e;(void)a;}
static bool ui_keyboard_pending(void){return false;}
enum {UI_KEYBOARD_NONE,UI_KEYBOARD_DONE};
static int keyboard_result;
static int ui_keyboard_tap(int x,int y,int top,int64_t ms){(void)x;(void)y;(void)top;(void)ms;return keyboard_result;}
static void ui_keyboard_draw(uint8_t *f,int y){(void)f;(void)y;}
static void ui_text_input_draw(uint8_t *f,ui_text_edit_t *e,EpdRect r,int p,bool m,void *s){(void)f;(void)e;(void)r;(void)p;(void)m;(void)s;}
static void ui_text_input_tap(ui_text_edit_t *e,EpdRect r,int p,bool m,void *s,int x,int y){(void)e;(void)r;(void)p;(void)m;(void)s;(void)x;(void)y;}
static void ui_fill_round_rect(uint8_t *f,EpdRect r,int radius,int c){(void)f;(void)r;(void)radius;(void)c;}
static void ui_draw_control_frame(uint8_t *f,EpdRect r,int radius,int c){(void)f;if(r.y==240){assert(radius==16&&c==0&&((r.x==36&&r.width==612&&r.height==650)||(r.x==24&&r.width==636&&r.height==700)));++panel_frames;}}
static void ui_text_box(uint8_t *f,EpdRect r,int px,const char *t,enum EpdFontFlags a,bool inv){(void)f;(void)t;(void)a;(void)inv;if(px==32){assert(r.x==62&&r.width==560&&r.height==44&&r.y+r.height<=720);assert(definition_lines<10);definition_ys[definition_lines++]=r.y;}}
static void ui_fill_rect_fast(EpdRect r,int c,uint8_t *f){(void)f;(void)r;(void)c;}
static void epd_fill_rect(EpdRect r,int c,uint8_t *f){(void)f;(void)r;(void)c;writes++;}
static int ttf_text_width_px(int p,const char *t){(void)p;(void)t;return 12;}
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
static void *heap_caps_calloc(size_t n,size_t size,int caps){(void)caps;return calloc(n,size);}
void book_notes_cancel(void);
'''
(out/'book_notes_ui.h').write_text(header)
for name in ['ui_keyboard.h','ui_kit.h','esp_heap_caps.h','ttf_font.h']:(out/name).write_text('#include "book_notes_ui.h"\n')
(out/'controller.c').write_text((root/'main/book/book_notes_ui.c').read_text())
adapter=r'''
#include "controller.c"
#define UI_LOCK_WIDTH 684
#define UI_LOCK_HEIGHT 1216
static int s_view=1,s_reader_panel=0,s_px=24;
enum {READING=1,READER_PANEL_NONE=0};
static const char *s_text="bound";
static bool s_toolbar,s_clear_confirm,s_zoom_active,s_notes_frame_pending;
static struct {bool pending;} s_reader_tap;
static size_t s_page;
static EpdRect s_area;
static EpdRect body_rect(void){return (EpdRect){20,80,640,1080};}
static void lock_draw(void){}
static void unlock_draw(void){}
static void invalidate_prep(void){}
static void render(app_ctx_t *ctx,uint8_t *fb){(void)ctx;book_notes_draw(fb,s_page,body_rect(),s_px);}
'''
adapter+=function('reader_notes_gesture',root/'main/apps/app_book.c')+'\n'
unit=adapter+r'''
#include <assert.h>
#include <unistd.h>
static app_ctx_t ctx;
static EpdRect body={20,240,640,800};
static app_redraw_t redraw;
static bool event(gesture_type t,int x,int y){ui_gesture_event_t e={.type=t,.x=x,.y=y,.x0=x,.y0=y};(void)body;return reader_notes_gesture(&ctx,&e,&redraw);}
static bool action(int i){EpdRect r=menu_action_rect(i);return event(UI_GESTURE_TAP,r.x+r.width/2,r.y+r.height/2);}
int main(void){
 char preview[128];
 note_preview("một hai ba bốn năm sáu bảy tám",preview);assert(!strcmp(preview,"một hai ba bốn năm sáu…"));
 note_preview("một hai",preview);assert(!strcmp(preview,"một hai"));
 note_preview("  một\n hai  ba ",preview);assert(!strcmp(preview,"một hai ba"));
 char root[]="/tmp/kiiko-ui-XXXXXX";assert(mkdtemp(root));assert(chdir(root)==0);
 book_notes_bind("book.txt",0,test_text,strlen(test_text));assert(s&&s->valid);
 assert(book_notes_open_list(0)&&s->mode==LIST&&!s->dragging);
 book_notes_cancel();
 assert(event(UI_GESTURE_LONG_PRESS,60,310));assert(s->start==0&&s->end==5&&held);assert(redraw==APP_REDRAW_AREA&&s_notes_frame_pending);
 assert(icon_calls==0);
 assert(event(UI_GESTURE_MOVE,240,310));assert(s->start==0&&s->end==12);
 assert(event(UI_GESTURE_CANCEL,240,310));assert(!s->dragging&&book_notes_active());
 book_notes_draw(NULL,0,body_rect(),24);assert(icon_calls>=5);
 assert(action(1));assert(s->annotations.count==1&&s->annotations.sequence==1&&!book_notes_active());
 int painted=writes;book_notes_draw(NULL,0,body_rect(),24);assert(writes>painted);
 assert(event(UI_GESTURE_LONG_PRESS,60,310));assert(event(UI_GESTURE_MOVE,240,310));assert(event(UI_GESTURE_CANCEL,240,310));
 assert(action(2));assert(s->mode==EDIT_NOTE);
 assert(ui_text_edit_insert(&s->edit,"A note"));assert(event(UI_GESTURE_TAP,60,370));
 assert(!strcmp(s->annotations.items[0].note,"A note"));
 assert(action(2));keyboard_result=UI_KEYBOARD_DONE;assert(event(UI_GESTURE_TAP,590,1020));
 assert(!book_notes_active()&&!strcmp(s->annotations.items[0].note,"A note"));keyboard_result=UI_KEYBOARD_NONE;
 assert(event(UI_GESTURE_LONG_PRESS,60,310));assert(event(UI_GESTURE_MOVE,240,310));assert(event(UI_GESTURE_CANCEL,240,310));
 assert(action(2));assert(ui_text_edit_insert(&s->edit," retry"));
 char blocked[700];snprintf(blocked,sizeof(blocked),"%s/%016llx-%u-%016llx.%u.ann.tmp",s->storage_root,
    (unsigned long long)s->annotations.path_hash,s->annotations.chapter,
    (unsigned long long)s->annotations.text_hash,(s->annotations.sequence+1)&1);
 assert(!mkdir(blocked,0775));assert(event(UI_GESTURE_TAP,60,370));
 assert(s->mode==EDIT_NOTE&&s->valid&&!strcmp(s->annotations.items[0].note,"A note"));
 assert(!strcmp(s->draft,"A note retry"));assert(!rmdir(blocked));
 assert(event(UI_GESTURE_TAP,60,370));assert(s->mode==SELECT&&!strcmp(s->annotations.items[0].note,"A note retry"));
 assert(action(2));s->draft[6]=0;assert(event(UI_GESTURE_TAP,60,370));
 assert(action(1));assert(!strcmp(s->annotations.items[0].note,"A note"));
 book_notes_cancel();assert(!held&&!book_notes_active());
 book_notes_bind("book.txt",0,test_text,strlen(test_text));assert(s->annotations.count==1&&!strcmp(s->annotations.items[0].note,"A note"));
 assert(event(UI_GESTURE_LONG_PRESS,60,310));assert(event(UI_GESTURE_MOVE,240,310));assert(event(UI_GESTURE_CANCEL,240,310));
 assert(action(3));assert(s->mode==LIST);
 panel_frames=0;book_notes_draw(NULL,0,body_rect(),24);assert(panel_frames==1);
 assert(event(UI_GESTURE_TAP,100,180));assert(!book_notes_active());
 assert(book_notes_open_list(0));
 assert(event(UI_GESTURE_TAP,580,290));assert(!s->annotations.count);
 book_notes_bind("book.txt",0,test_text,strlen(test_text));assert(!s->annotations.count);
 vertical=true;assert(!event(UI_GESTURE_LONG_PRESS,60,310));vertical=false;
 assert(event(UI_GESTURE_LONG_PRESS,60,310));generation++;
 assert(!event(UI_GESTURE_TAP,60,310));assert(!book_notes_active());
 assert(event(UI_GESTURE_LONG_PRESS,60,310));assert(event(UI_GESTURE_MOVE,240,310));assert(event(UI_GESTURE_CANCEL,240,310));
 assert(action(0));assert(s->mode==LOOKUP&&!s->searching);
 s->dictionary.state=4;strcpy(s->dictionary.text,"one\ntwo\nthree\nfour\nfive\nsix\nseven\neight\nnine\nten\neleven");
 panel_frames=definition_lines=0;book_notes_draw(NULL,0,body_rect(),24);assert(panel_frames==1&&definition_lines==10);
 for(int i=0;i<10;++i)assert(definition_ys[i]==260+i*44);
 s->definition_page=1;definition_lines=0;book_notes_draw(NULL,0,body_rect(),24);assert(definition_lines==1&&definition_ys[0]==260);
 panel_frames=0;book_notes_draw(NULL,0,body_rect(),24);assert(panel_frames==1);
 assert(event(UI_GESTURE_TAP,100,400)&&s->mode==LOOKUP);
 assert(event(UI_GESTURE_TAP,240,830)&&s->mode==LOOKUP); // Page control stays usable.
 assert(event(UI_GESTURE_TAP,20,400)&&!book_notes_active());
 assert(!s->dictionary.data&&!s->dictionary.index&&!s->root&&!s->child);
 // Outside taps cancel even while directory enumeration/lookup is pending.
 assert(!mkdir("dict",0775));
 const int outside[][2]={{100,180},{670,400},{100,1000}};
 for(unsigned i=0;i<3;++i){
  s->mode=SELECT;assert(action(0)&&s->mode==LOOKUP&&s->searching&&s->root);
  assert(event(UI_GESTURE_TAP,outside[i][0],outside[i][1])&&!book_notes_active());
  assert(redraw==APP_REDRAW_PAGE&&!s->searching&&!s->root&&!s->child&&!s->dictionary.index&&!s->dictionary.data);
 }
 assert(!rmdir("dict"));
 book_notes_cancel();glyph_y=80;
 assert(event(UI_GESTURE_LONG_PRESS,60,90));assert(s->dragging);
 assert(s->menu_y>=110);assert(event(UI_GESTURE_CANCEL,60,90));
 EpdRect menu=menu_action_rect(0);assert(menu.y>=110);
 book_notes_cancel();glyph_y=1100;
 assert(event(UI_GESTURE_LONG_PRESS,60,1110));assert(event(UI_GESTURE_CANCEL,60,1110));
 menu=menu_action_rect(0);assert(menu.y+menu.height<1100);
 book_notes_bind("compact.txt",0,test_text,strlen(test_text));
 assert(book_annotations_put(&s->annotations,test_text,0,5,"first"));
 assert(book_annotations_put(&s->annotations,test_text,6,12,"second"));
 assert(book_notes_open_list(0));assert(event(UI_GESTURE_TAP,100,340));assert(s->mode==SELECT&&s->start==6);
 assert(book_notes_open_list(0));assert(event(UI_GESTURE_TAP,580,350));assert(s->annotations.count==1&&s->annotations.items[0].start==0);
 book_notes_reset();assert(!s);
 /* RC3..5 migration: copy old snapshots once, preserve originals and generation. */
 assert(!mkdir("legacy.sdr",0775));
 book_annotations_t legacy;
 assert(book_annotations_load(&legacy,"legacy.sdr/kiikoread","",0,test_text,strlen(test_text)));
 assert(book_annotations_put(&legacy,test_text,0,5,"Old sidecar note"));
 assert(book_annotations_save(&legacy,"legacy.sdr/kiikoread"));
 assert(book_annotations_save(&legacy,"legacy.sdr/kiikoread"));
 book_notes_bind("legacy.txt",0,test_text,strlen(test_text));
 assert(s&&s->valid&&s->annotations.count==1&&s->annotations.sequence==legacy.sequence);
 assert(!strcmp(s->storage_root,"dpReader_note/legacy.txt/snapshots"));
 assert(!strcmp(s->annotations.items[0].note,"Old sidecar note"));
 assert(book_annotations_put(&s->annotations,test_text,0,5,"New root note"));assert(persist());
 book_notes_bind("legacy.txt",0,test_text,strlen(test_text));assert(!strcmp(s->annotations.items[0].note,"New root note"));
 assert(book_annotations_load(&legacy,"legacy.sdr/kiikoread","",0,test_text,strlen(test_text)));
 assert(!strcmp(legacy.items[0].note,"Old sidecar note"));
 book_notes_reset();
 /* RC2 migration without creating a sidecar. */
 book_annotations_t rc2;
 assert(book_annotations_load(&rc2,NOTES_ROOT,"old.txt",0,test_text,strlen(test_text)));
 assert(book_annotations_put(&rc2,test_text,0,5,"RC2 note"));assert(book_annotations_save(&rc2,NOTES_ROOT));
 book_notes_bind("old.txt",0,test_text,strlen(test_text));assert(s->valid&&s->annotations.count==1&&!strcmp(s->annotations.items[0].note,"RC2 note"));
 struct stat absent;assert(stat("old.sdr",&absent)&&errno==ENOENT);
 book_notes_reset();assert(!s);puts("notes UI selection/save/edit/delete/reflow/cancel: PASS");return 0;
}
'''
(out/'test.c').write_text(unit)
subprocess.run(['cc','-std=c11','-D_DEFAULT_SOURCE','-D_DARWIN_C_SOURCE','-Wall','-Wextra','-Werror','-fsanitize=undefined','-DNOTES_ROOT="notes"','-DDICT_ROOT="dict"','-I'+str(out),'-I'+str(root/'main/book'),'-I'+str(root/'main/ui'),str(out/'test.c'),str(root/'main/book/book_annotations.c'),str(root/'main/book/book_dictionary.c'),str(root/'main/book/book_dict_file.c'),'-lz',str(root/'main/book/book_annotation_exchange.c'),*cjson_flags(),str(root/'main/ui/ui_text_edit.c'),'-o',str(out/'test')],check=True)
subprocess.run([str(out/'test')],check=True)
