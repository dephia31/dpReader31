#!/usr/bin/env python3
"""Exercise real audio-page gestures and exit/lock/media cancellation, paint purity."""
from pathlib import Path
import subprocess,tempfile
from home_cover_cache_host_test import function
R=Path(__file__).resolve().parents[1];src=R/'main/apps/app_audio.c'
unit=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
typedef struct{int x,y,width,height;}EpdRect;
typedef struct{bool request_return;}app_ctx_t;
typedef enum{APP_REDRAW_NONE,APP_REDRAW_PAGE}app_redraw_t;
enum{UI_KEY_1,UI_KEY_2,UI_KEY_3,UI_GESTURE_TAP,UI_GESTURE_MOVE};
enum{EPD_DRAW_ALIGN_CENTER,EPD_DRAW_ALIGN_LEFT,ESP_OK};
typedef struct{int type,x0,y0;}ui_gesture_event_t;
typedef enum{METALIO_AUDIO_IDLE,METALIO_AUDIO_STARTING,METALIO_AUDIO_PLAYING,METALIO_AUDIO_DONE,METALIO_AUDIO_ERROR}metalio_audio_state_t;
static char requested[320],selected[320];static metalio_audio_state_t shown,phase;static bool suspended,bulk;static unsigned stops,starts,draws;
static metalio_audio_state_t metalio_audio_state(void){return phase;}
static const char *metalio_audio_message(void){return "test";}
static void metalio_audio_stop(void){++stops;phase=METALIO_AUDIO_IDLE;}
static int metalio_audio_play_wav(const char *path){assert(!strcmp(path,"/sdcard/test.wav"));++starts;phase=METALIO_AUDIO_STARTING;return ESP_OK;}
static void display_set_bulk_io(bool b){bulk=b;}
static bool ui_rect_hit(EpdRect r,int x,int y){return x>=r.x&&x<r.x+r.width&&y>=r.y&&y<r.y+r.height;}
static void ui_clear_page(uint8_t *p){(void)p;++draws;}
static void ui_text(uint8_t *fb,int x,int y,int px,const char*t,int a,bool i){(void)fb;(void)px;(void)a;(void)i;assert(x>=0&&x<684&&y>=0&&y<1100&&t);++draws;}
static void ui_text_box(uint8_t *fb,EpdRect r,int px,const char*t,int a,bool i){ui_text(fb,r.x,r.y,px,t,a,i);assert(r.x+r.width<=684&&r.y+r.height<=1100);}
static void ui_draw_round_rect(uint8_t*p,EpdRect r,int radius,int c){(void)p;(void)r;(void)radius;(void)c;++draws;}
'''
for n in ('app_audio_request_open','enter','stop','active','render','toggle','key','gesture','tick'):
 unit+=function(n,src)+'\n'
unit+=r'''
int main(void){
 app_ctx_t ctx={0};uint8_t fb=0;
 assert(!app_audio_request_open(NULL)&&!app_audio_request_open("/bad"));
 assert(app_audio_request_open("/sdcard/test.wav"));enter(&ctx);assert(!strcmp(selected,"/sdcard/test.wav")&&!requested[0]&&!starts);
 unsigned oldstops=stops,oldstarts=starts;render(&ctx,&fb);assert(draws&&stops==oldstops&&starts==oldstarts);
 ui_gesture_event_t ev={UI_GESTURE_TAP,100,550};assert(gesture(&ctx,&ev)==APP_REDRAW_PAGE&&starts==1&&bulk);
 phase=METALIO_AUDIO_PLAYING;assert(tick(&ctx)==APP_REDRAW_NONE);
 oldstops=stops;stop(&ctx);assert(stops==oldstops+1&&!bulk&&suspended&&!ctx.request_return);
 for(int scenario=0;scenario<3;++scenario){phase=METALIO_AUDIO_PLAYING;bulk=true;oldstops=stops;stop(&ctx);assert(stops==oldstops+1&&!bulk);}
 assert(key(&ctx,UI_KEY_2)==APP_REDRAW_PAGE&&starts==2&&bulk);
 phase=METALIO_AUDIO_DONE;assert(tick(&ctx)==APP_REDRAW_PAGE&&!bulk);assert(tick(&ctx)==APP_REDRAW_NONE);
 assert(key(&ctx,UI_KEY_1)==APP_REDRAW_NONE&&ctx.request_return&&!bulk);
 ctx.request_return=false;assert(key(&ctx,UI_KEY_3)==APP_REDRAW_NONE&&ctx.request_return);
 ev.type=UI_GESTURE_MOVE;assert(gesture(&ctx,&ev)==APP_REDRAW_NONE);
 enter(&ctx);assert(!selected[0]);oldstarts=starts;assert(toggle(&ctx)==APP_REDRAW_NONE&&starts==oldstarts);
 puts("PASS actual audio page: explicitstart, TAP/keys, no streaming redraw, paintpurity, exit/lock/mediajoin, bulkIOrestore, finish, returnguards");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(unit)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(p/'test.c'),'-o',str(p/'test')],check=True);subprocess.run([str(p/'test')],check=True)
