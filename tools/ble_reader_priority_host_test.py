from pathlib import Path
import subprocess,tempfile
from home_cover_cache_host_test import function
root=Path(__file__).resolve().parents[1];book=root/'main/apps/app_book.c';loop=(root/'main/app/app_loop.c').read_text();ble=root/'components/ble_page_turner/src/ble_page_turner.c'
fn=function('app_book_background_radio_ready',book)
touch_gate=function('reader_touch_owns_input',book)
tick=function('on_tick',book)
assert tick.index('if (ble_enabled && touch_owns_input) ble_pt_discard_input();') < tick.index('book_reader_tap_tick(')
assert 'if (ble_enabled && !touch_owns_input) {\n        ble_pt_action_t action = book_remote_action(' in tick
code='''#include <assert.h>
#include <stdbool.h>
enum{READING=1,SHELF};static char s_requested_open[20];static bool s_scan_pending,s_fonts_reload_pending,s_reader_display_ready;static const char *s_text;static int s_view,s_presented_view;
typedef struct {bool touched;} touch_t;
typedef struct {bool pressed,released,consumed; const touch_t *touch;} app_ctx_t;
static struct {bool pending;} s_reader_tap;
'''+fn+'\n'+touch_gate+'''
int main(void){s_view=READING;s_text="book";s_presented_view=READING;assert(!app_book_background_radio_ready());s_reader_display_ready=true;assert(app_book_background_radio_ready());
s_requested_open[0]='a';assert(!app_book_background_radio_ready());s_requested_open[0]=0;
s_scan_pending=true;assert(!app_book_background_radio_ready());s_scan_pending=false;
s_fonts_reload_pending=true;assert(!app_book_background_radio_ready());s_fonts_reload_pending=false;
s_text=0;assert(!app_book_background_radio_ready());s_text="book";s_presented_view=SHELF;assert(!app_book_background_radio_ready());s_view=SHELF;assert(app_book_background_radio_ready());
touch_t touch={.touched=true};app_ctx_t ctx={.touch=&touch};
assert(!reader_touch_owns_input(&ctx)); // Full shelf keeps its own remote behavior.
s_view=READING;assert(reader_touch_owns_input(&ctx)); // Held contact even without a new press edge.
touch.touched=false;assert(!reader_touch_owns_input(&ctx));
ctx.pressed=true;assert(reader_touch_owns_input(&ctx));ctx.pressed=false;
ctx.released=true;assert(reader_touch_owns_input(&ctx));ctx.released=false;
ctx.consumed=true;assert(reader_touch_owns_input(&ctx));ctx.consumed=false;
s_reader_tap.pending=true;assert(reader_touch_owns_input(&ctx));
s_reader_tap.pending=false;assert(!reader_touch_owns_input(&ctx));}
'''
start=function('start_locked',ble);assert start.index('profiles_load();')<start.index('s_running = true;')<start.index('atomic_store(&s_recent_requested,true)')
poll=function('poll_locked',ble);assert poll.index('if(!atomic_load(&s_host_ready))')<poll.index('reconnect_recent_requested();')
run=loop[loop.index('void app_loop_run('):];assert run.index('current->on_tick(&ctx)')<run.index('bool radio_ready=')<run.index('ble_pt_sync(app_settings_ble_turner());')
assert run.index('switch_to(&ctx, &current, next)')<run.index('bool radio_ready=')
assert 'if(!app_settings_ble_turner() || radio_ready)' in run
assert 's_reader_display_ready=false;' in function('before_lock',book)
assert 's_reader_display_ready=err==EPD_DRAW_SUCCESS' in function('present',book)
with tempfile.TemporaryDirectory() as t:
 p=Path(t);(p/'test.c').write_text(code);subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(p/'test.c'),'-o',str(p/'test')],check=True);subprocess.run([str(p/'test')],check=True)
print('PASS reader radio admission: pending open/layout/media/frame-failure block; completed frame admits; book tick/switch/display precede BLE start; host-ready gates saved-peer request; Sleep invalidates frame')
