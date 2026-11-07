#!/usr/bin/env python3
"""Exercise footer swipe admission and verify book notice placement."""
from pathlib import Path
import subprocess
import tempfile
from home_cover_cache_host_test import function
from board_source_view import select_source

root = Path(__file__).resolve().parents[1]
book = root / "main/apps/app_book.c"
source = book.read_text()
gesture = function("gesture_event", book)
assert gesture.index("reader_footer_home_swipe(ctx, ev)") < gesture.index("reader_notes_gesture(ctx")
assert gesture.index("s_reader_return_home = true;") < gesture.index("return reader_return(ctx);")
opening = source[source.index("static bool open_book("):source.index("static bool open_requested_book(")]
assert opening.index("book_busy_notice(ctx") < opening.index("open_book_impl(ctx")
for name in ("reader_return", "book_on_exit"):
    close = select_source(function(name, book), "metalio47")
    assert close.index('book_busy_notice(ctx, "Đang đóng sách…")') < close.index("save_progress();")

unit = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#define UI_LOCK_HEIGHT 1216
#define READER_FOOTER_TOP 1120
#define UI_SWIPE_MIN_PX 60
enum {READING, SHELF, READER_PANEL_NONE=0, READER_PANEL_TOOLS,
      UI_GESTURE_MOVE, UI_GESTURE_SWIPE_U, UI_GESTURE_SWIPE_R, UI_GESTURE_TAP};
typedef struct {bool consumed;} app_ctx_t;
typedef struct {int type;uint16_t x,y,x0,y0;} ui_gesture_event_t;
static int s_view=READING,s_reader_panel=READER_PANEL_NONE;
static const char *s_text="book";
static struct {bool open;} s_reader_choice;
static bool s_toolbar,s_clear_confirm,s_scan_pending,s_zoom_active,notes,enabled=true;
static bool book_notes_active(void){return notes;}
static bool app_settings_reader_swipe_bottom_home(void){return enabled;}
'''
unit += function("reader_footer_home_swipe", book) + r'''
int main(void) {
    app_ctx_t ctx={0};
    ui_gesture_event_t ev={UI_GESTURE_MOVE,150,1060,100,1120};
    assert(!reader_footer_home_swipe(&ctx,&ev)); // Drag never changes screens before release.
    ev.type=UI_GESTURE_SWIPE_U;
    assert(reader_footer_home_swipe(&ctx,&ev));
    ev.y=1061;assert(!reader_footer_home_swipe(&ctx,&ev));ev.y=1060;
    ev.y0=1119;ev.y=1040;assert(!reader_footer_home_swipe(&ctx,&ev));ev.y0=1120;
    ev.x=200;assert(!reader_footer_home_swipe(&ctx,&ev));ev.x=150;
    ev.type=UI_GESTURE_TAP;assert(!reader_footer_home_swipe(&ctx,&ev));ev.type=UI_GESTURE_SWIPE_U;
    s_reader_panel=READER_PANEL_TOOLS;assert(!reader_footer_home_swipe(&ctx,&ev));s_reader_panel=READER_PANEL_NONE;
    bool *guards[]={&ctx.consumed,&s_reader_choice.open,&s_toolbar,&s_clear_confirm,&s_scan_pending,&s_zoom_active,&notes};
    for(size_t i=0;i<sizeof(guards)/sizeof(*guards);++i){
        *guards[i]=true;assert(!reader_footer_home_swipe(&ctx,&ev));*guards[i]=false;
    }
    enabled=false;assert(!reader_footer_home_swipe(&ctx,&ev));

}
'''
with tempfile.TemporaryDirectory() as folder:
    c = Path(folder) / "footer.c"
    exe = Path(folder) / "footer"
    c.write_text(unit)
    subprocess.run(["cc", "-std=c11", "-Wall", "-Wextra", "-Werror", str(c), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print("PASS: footer swipe admission, book close notice before save and release-only navigation")
