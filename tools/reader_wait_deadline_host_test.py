#!/usr/bin/env python3
"""Exercise the real reader wait admission and deadline planner without IO."""
from pathlib import Path
import subprocess,tempfile
from home_cover_cache_host_test import function
r=Path(__file__).resolve().parents[1]
u=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include "app_wait.h"
typedef struct{int64_t now_ms;}app_ctx_t;
enum{READING=1,SHELF,READER_PANEL_NONE=0};
static int s_view=READING,s_presented_view=READING,s_reader_panel,s_turn_direction=1;
static const char *s_text="book",*s_font_path="font";
static bool s_reader_display_ready=true,s_toolbar,s_clear_confirm,s_scan_pending,s_fonts_reload_pending,s_background_prepare_pending;
static bool s_font_choice_pending,s_font_recovery_pending,s_font_restart_pending,s_save_failed,s_ticket_cover_done=true,s_chapter_warm_done;
static bool s_zoom_active,s_reader_clock_pending,notes,auto_sync,background,images,lock_available=true;
static char s_requested_open[8],s_reader_notice[8];
static struct{bool open;}s_reader_choice;
static struct{bool pending;}s_reader_tap;
static struct{bool active;}s_indent_hold;
static struct{bool valid;}s_chapter_jump_undo;
static size_t s_page=4,s_chapter=0,pages=20,chapters=2;
static int64_t s_stats_activity_ms;
static unsigned s_unsaved;
static uint32_t s_stats_pending_ms;
static bool book_notes_active(void){return notes;}
static bool app_settings_kosync_auto(void){return auto_sync;}
static bool reader_layout_font_changed(void){return strcmp(s_font_path,"font")!=0;}
static const char *app_settings_book_background_path(void){return background?"bg":"";}
static unsigned app_settings_lock_style(void){return 0;}
static unsigned book_layout_page_image_count(size_t p){(void)p;return images;}
static size_t book_layout_page_count(void){return pages;}
static size_t book_chapter_count(void){return chapters;}
static unsigned book_layout_generation(void){return 10;}
static size_t fb_bytes(void){return 100;}
#define pdTRUE 1
static void *s_draw_lock=(void*)1;static unsigned locks,unlocks,cache_mask=7;
static int xSemaphoreTake(void*p,int wait){assert(p&&wait==0);++locks;return lock_available;}
static void xSemaphoreGive(void*p){assert(p);++unlocks;}
static const void *reader_page_cache_find(size_t p,unsigned g,size_t bytes){assert(g==10&&bytes==100);unsigned bit=p==s_page+1?1:p==s_page+2?2:4;return cache_mask&bit?(void*)1:NULL;}
static int fake_gettimeofday(struct timeval*t,void*z){(void)z;t->tv_sec=1800000059;t->tv_usec=900000;return 0;}
#define gettimeofday fake_gettimeofday
'''+function('reader_next_tick_ms',r/'main/apps/app_book.c')+r'''
int main(void){app_ctx_t ctx={10000};
 int64_t next=reader_next_tick_ms(&ctx);assert(next==10100&&locks==unlocks);
 int64_t due[]={next,10200,10500,10750};assert(app_wait_ms(10000,40,1,0,due,4)==100);
 assert(app_wait_ms(10000,40,1,1,due,4)==0);assert(app_wait_ms(10000,8,0,0,due,4)==8);
 assert(app_wait_ms(10120,40,1,0,due,4)==0); // Rendering time cannot push an expired deadline later.
 int64_t far[]={20000};assert(app_wait_ms(10000,40,1,0,far,1)==200);
 bool *gates[]={&s_toolbar,&s_clear_confirm,&s_scan_pending,&s_fonts_reload_pending,&s_background_prepare_pending,
 &s_font_choice_pending,&s_font_recovery_pending,&s_font_restart_pending,&s_save_failed,&s_reader_choice.open,
 &s_reader_tap.pending,&s_chapter_jump_undo.valid,&s_indent_hold.active,&s_zoom_active,&s_reader_clock_pending,&notes,&auto_sync,&background,&images};
 for(unsigned i=0;i<sizeof(gates)/sizeof(*gates);i++){*gates[i]=true;assert(!reader_next_tick_ms(&ctx));*gates[i]=false;}
 s_reader_display_ready=false;assert(!reader_next_tick_ms(&ctx));s_reader_display_ready=true;
 s_requested_open[0]='b';assert(!reader_next_tick_ms(&ctx));s_requested_open[0]=0;
 s_reader_notice[0]='x';assert(!reader_next_tick_ms(&ctx));s_reader_notice[0]=0;
 s_ticket_cover_done=false;assert(!reader_next_tick_ms(&ctx));s_ticket_cover_done=true;
 for(cache_mask=0;cache_mask<7;cache_mask++)assert(!reader_next_tick_ms(&ctx));cache_mask=7;
 lock_available=false;assert(!reader_next_tick_ms(&ctx));lock_available=true;
 s_page=18;assert(!reader_next_tick_ms(&ctx));s_chapter_warm_done=true;assert(reader_next_tick_ms(&ctx));s_page=4;
 s_stats_pending_ms=29999;assert(reader_next_tick_ms(&ctx)==10001);
 s_unsaved=8;assert(!reader_next_tick_ms(&ctx));s_unsaved=0;
 puts("PASS actual reader admission/planner: pending taps/holds/open/layout/frame/cache/worker failures keep short waits; ready cache only, zero-wait input, minute/stat deadlines, elapsed render time and200ms ceiling");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(u)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined','-I'+str(r/'main/app'),str(p/'test.c'),'-o',str(p/'test')],check=True);subprocess.run([str(p/'test')],check=True)
