#!/usr/bin/env python3
"""Exercise the actual S31 wake admission, raster failure and frame retry code."""
from pathlib import Path
import subprocess
import tempfile
from home_cover_cache_host_test import function

source = Path(__file__).resolve().parents[1] / 'main/apps/app_book.c'
unit = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define ESP_LOGW(...) ((void)0)
enum {READING, SHELF};
typedef enum {APP_REDRAW_NONE, APP_REDRAW_AREA, APP_REDRAW_PAGE, APP_REDRAW_FULL, APP_REDRAW_DONE} app_redraw_t;
typedef struct {uint8_t *back_fb;} display_t;
typedef struct {display_t *hl; uint8_t *fb; bool request_app; int64_t now_ms;} app_ctx_t;
static int s_view=READING;
static const char *s_text="book";
static bool s_reader_resume_pending, s_reader_raster_failed, s_reader_display_ready;
static bool s_reader_text_frame, s_reader_image_frame, media_ready, changed_face;
static int64_t s_reader_raster_retry_ms;
static unsigned font_loads, reflows;
static int64_t esp_timer_get_time(void){return 400000;}
static size_t fb_bytes(void){return 4;}
static bool app_sleep_media_ready(void){return media_ready;}
static void app_font_activate_reading(void){assert(media_ready);++font_loads;}
static bool reader_reflow_font(void){++reflows;return changed_face;}
'''
for name in ('reader_preserve_display','reader_defer_raster_retry'):
    unit += function(name,source) + '\n'
unit += r'''
typedef struct {int unused;} EpdRect;
typedef struct {uint32_t generation;} cursor_t;
static uint8_t prep_pixels[4], *s_next_fb=prep_pixels;
static int s_prep_page=2, s_next_page=-1, s_px=74;
static uint32_t glyph_failures, s_next_generation;
static unsigned cancellations, finished;
static bool inject_failure, face_ready=true;
static cursor_t s_prep_cursor;
static uint32_t ttf_font_glyph_failures(void){return glyph_failures;}
static bool ttf_font_ready(void){return face_ready;}
static EpdRect body_rect(void){return (EpdRect){0};}
static bool text_contrast;
static bool app_settings_book_text_contrast(void){return true;}
static bool ttf_set_text_contrast(bool enabled){bool old=text_contrast;text_contrast=enabled;return old;}
static int book_layout_draw_slice(uint8_t *fb,size_t page,EpdRect r,int px,cursor_t *cursor){
    assert(text_contrast);
    (void)r;assert(fb==prep_pixels&&page==2&&px==74);
    if(inject_failure)++glyph_failures;
    cursor->generation=7;
    return 1;
}
static void pause_reader_prep(void){++cancellations;s_next_page=-1;s_next_fb=NULL;}
static void invalidate_prep(void){assert(false);}
static uint32_t book_layout_generation(void){return 7;}
static void reader_page_cache_finish(size_t page,uint32_t generation){assert(page==2&&generation==7);++finished;}
'''
unit += function('reader_prefetch_step',source) + '\n'
present = function('present', source)
start = present.index('    if(s_view==READING && s_text')
end = present.index('    if((s_reader_choice',start)
unit += 'static bool admission(app_ctx_t *ctx,app_redraw_t *next){app_redraw_t redraw=*next;\n'
unit += present[start:end].replace('#endif','') + '*next=redraw;return false;}\n'
tick = function('on_tick',source)
start = tick.index('    if(s_view==READING && s_text)')
end = tick.index('    const bool ble_enabled',start)
unit += 'static app_redraw_t wake_tick(app_ctx_t *ctx){\n'+tick[start:end].replace('#endif','')
unit += 'return APP_REDRAW_NONE;}\n'
unit += r'''
int main(void) {
    uint8_t lock[]={0xa1,0xb2,0xc3,0xd4}, target[4]={0};
    display_t display={lock};app_ctx_t ctx={.hl=&display,.fb=target};
    app_redraw_t redraw=APP_REDRAW_PAGE;
    s_reader_resume_pending=true;
    assert(admission(&ctx,&redraw)&&!memcmp(lock,target,4));
    assert(s_reader_resume_pending&&!font_loads&&!reflows&&wake_tick(&ctx)==APP_REDRAW_NONE);
    ctx.request_app=true;assert(!admission(&ctx,&redraw)&&!font_loads);ctx.request_app=false;
    media_ready=true;
    assert(wake_tick(&ctx)==APP_REDRAW_PAGE);
    assert(!admission(&ctx,&redraw)&&redraw==APP_REDRAW_FULL);
    assert(!s_reader_resume_pending&&font_loads==1&&reflows==1);
    redraw=APP_REDRAW_PAGE;assert(!admission(&ctx,&redraw)&&font_loads==1&&reflows==2);
    // A late font replacement must reflow before painting a full refresh.
    changed_face=true;redraw=APP_REDRAW_FULL;
    assert(!admission(&ctx,&redraw)&&redraw==APP_REDRAW_FULL&&reflows==3);
    // Preserve the last pushed frame and retry an incomplete body on the idle tick.
    memset(target,0,4);s_reader_display_ready=s_reader_text_frame=s_reader_image_frame=s_reader_raster_failed=true;
    reader_defer_raster_retry(&ctx);
    assert(!s_reader_display_ready&&!s_reader_text_frame&&!s_reader_image_frame&&!memcmp(lock,target,4));
    ctx.now_ms=1399;assert(wake_tick(&ctx)==APP_REDRAW_NONE);
    ctx.now_ms=1400;assert(wake_tick(&ctx)==APP_REDRAW_PAGE);
    s_reader_raster_failed=false;assert(wake_tick(&ctx)==APP_REDRAW_NONE);
    inject_failure=true;
    assert(reader_prefetch_step()==-1&&!text_contrast&&cancellations==1&&!finished&&s_next_page==-1);
    inject_failure=false;s_next_fb=prep_pixels;
    assert(reader_prefetch_step()==1&&!text_contrast&&finished==1&&s_next_page==2&&s_next_generation==7);
    puts("PASS: slow mount keeps lock pixels; font restores before first frame; changed face reflows; failed raster preserves visible frame and retries; failed prefetch is never promoted");
}
'''
with tempfile.TemporaryDirectory() as tmp:
    path = Path(tmp) / 'test.c'
    path.write_text(unit)
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined','-DDPREADER_BOARD_METALIO47=1',
                    str(path),'-o',str(path.with_suffix(''))],check=True)
    subprocess.run([str(path.with_suffix(''))],check=True)
