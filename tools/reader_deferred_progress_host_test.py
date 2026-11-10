#!/usr/bin/env python3
"""Run production reader maintenance with burst input, pending IRQs and failed storage."""
from pathlib import Path
import subprocess
import tempfile
from home_cover_cache_host_test import function

root = Path(__file__).resolve().parents[1]
book = root / 'main/apps/app_book.c'
present = function('present', book)
turn = function('turn_page', book)
tick = function('on_tick', book)
assert 'save_progress();' not in present
assert 'save_progress();' not in turn
assert tick.index('ble_pt_action_t action = book_remote_action(') < tick.index('if (s_unsaved>=8')
assert tick.index('ble_pt_action_t action = book_remote_action(') < tick.index('kosync_auto_take_pull(')
assert 'flush_ticket_stats();' not in function('track_ticket_stats', book)
assert 'save_progress();' in function('before_lock', book)
assert 'save_progress();' in function('book_on_exit', book)
start = tick.index('    if (s_view==READING && s_text) {', tick.index('ble_pt_action_t action = book_remote_action('))
end = tick.index('    if (s_chapter_jump_undo.valid', start)
maintenance = tick[start:end]
unit = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
enum {READING=1,SHELF,APP_REDRAW_NONE};
enum {METALIO_UI_WAKE_TOUCH=1,METALIO_UI_WAKE_KEY=2,METALIO_UI_WAKE_BLE=4,METALIO_UI_WAKE_PREP=8};
#define ESP_LOGI(tag,...) do { if(0)printf(__VA_ARGS__); } while(0)
#define TAG "test"
typedef struct {bool touched;} touch_t;
typedef struct {int64_t now_ms;bool pressed,released,consumed,request_app,request_menu,request_return;touch_t *touch;} app_ctx_t;
static int s_view=READING,s_unsaved;
static const char *s_text="book";
static struct {bool pending;} s_reader_tap;
static bool s_save_failed,fail_save,fail_stats;
static int64_t s_last_turn_ms,s_stats_activity_ms,s_stats_retry_ms;
static unsigned pending_input,s_stats_pending_ms,saves,stats,prefetches;
static unsigned metalio_ui_wake_pending(void){return pending_input;}
static int64_t esp_timer_get_time(void){return 100000;}
static void reader_prefetch_idle(app_ctx_t *ctx){(void)ctx;++prefetches;}
static void save_progress(void){++saves;if(fail_save)s_save_failed=true;else s_unsaved=0;}
static void flush_ticket_stats(void){++stats;if(!fail_stats)s_stats_pending_ms=0;}
'''
unit += function('reader_maintenance_ready', book)
unit += '\nstatic int maintenance(app_ctx_t *ctx) {\n' + maintenance + '\nreturn 1;\n}\n'
unit += r'''
int main(void){
 touch_t touch={0};app_ctx_t ctx={.touch=&touch};
 s_unsaved=8;s_stats_pending_ms=30000;
 // Hundreds of consecutive turns do not write flash or lose the pending save.
 for(unsigned i=0;i<500;++i){ctx.now_ms+=400;s_last_turn_ms=ctx.now_ms;maintenance(&ctx);assert(!saves&&!stats&&s_unsaved==8);}
 ctx.now_ms+=1200;
 bool *gates[]={&ctx.pressed,&ctx.released,&ctx.consumed,&ctx.request_app,&ctx.request_menu,&ctx.request_return,&touch.touched,&s_reader_tap.pending};
 for(unsigned i=0;i<sizeof(gates)/sizeof(*gates);++i){*gates[i]=true;maintenance(&ctx);assert(!saves&&!stats);*gates[i]=false;}
 for(unsigned input=1;input<=4;input<<=1){pending_input=input;maintenance(&ctx);assert(!saves&&!stats);}
 pending_input=METALIO_UI_WAKE_PREP;
 maintenance(&ctx);assert(saves==1&&!s_unsaved&&!stats);
 maintenance(&ctx);assert(stats==1&&!s_stats_pending_ms);
 s_unsaved=8;fail_save=true;maintenance(&ctx);assert(saves==2&&s_save_failed);
 maintenance(&ctx);assert(saves==2);
 s_stats_pending_ms=30000;fail_stats=true;ctx.now_ms+=15000;maintenance(&ctx);assert(stats==2);
 for(unsigned i=0;i<1000;++i)maintenance(&ctx);assert(stats==2);
 ctx.now_ms+=15000;maintenance(&ctx);assert(stats==3);
 s_stats_activity_ms=ctx.now_ms;assert(!reader_maintenance_ready(&ctx));
 ctx.now_ms--;assert(!reader_maintenance_ready(&ctx));
 assert(prefetches>=500);
 puts("PASS 500 rapid turns, 1200ms post-frame pause, all input/IRQ gates, separate checkpoint/statistics, failed-storage backoff, exit/lock checkpoints retained");
}
'''
with tempfile.TemporaryDirectory() as folder:
    c = Path(folder) / 'reader.c'
    c.write_text(unit)
    exe = c.with_suffix('')
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-fsanitize=undefined',
                    str(c), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
