#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Exercise actual reader edge drag routing, independent controls and cancellation."""
from pathlib import Path
import subprocess,tempfile
from home_cover_cache_host_test import function
R=Path(__file__).resolve().parents[1]
src=R/'main/apps/app_book.c'
u=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#define KIIKO_BOARD_METALIO47 1
#define UI_CONTENT_TOP 180
#define READER_FOOTER_TOP 1140
#define UI_LOCK_WIDTH 684
#define METALIO_FRONTLIGHT_RANGE 240
#define READING 1
#define READER_PANEL_NONE 0
#define UI_GESTURE_PRESS 0
#define UI_GESTURE_MOVE 1
#define UI_GESTURE_SWIPE_U 2
#define UI_GESTURE_SWIPE_D 3
#define UI_GESTURE_SWIPE_L 4
#define UI_GESTURE_CANCEL 5
#define UI_GESTURE_TAP 6
typedef struct {int type;uint16_t x0,y0,x,y;}ui_gesture_event_t;
static int s_view=READING,s_reader_panel;static char *s_text="text";
static bool s_toolbar,s_clear_confirm,s_zoom_active,enabled,left=true,right=true;
static struct{bool pending;}s_reader_tap;
static struct{bool candidate,active,warm;int base;}s_edge_light;
static unsigned brightness,warmth=120,commits,adjusts,pauses;
static bool app_settings_reader_edge_brightness(void){return left;}
static bool app_settings_reader_edge_warmth(void){return right;}
static bool metalio_frontlight_enabled(void){return enabled;}
static unsigned metalio_frontlight_position(bool warm){return warm?warmth:brightness;}
static void metalio_frontlight_commit(void){++commits;}
static void metalio_frontlight_adjust_position(bool warm,unsigned value){assert(value<=240);++adjusts;if(warm)warmth=value;else{brightness=value;if(value)enabled=true;}}
static void pause_reader_prep(void){++pauses;}
'''+function('reader_edge_light_cancel',src)+function('reader_edge_light_gesture',src)+r'''
static bool event(int t,int x0,int y0,int x,int y){ui_gesture_event_t e={t,x0,y0,x,y};return reader_edge_light_gesture(&e);}
int main(void){
 assert(!event(UI_GESTURE_PRESS,20,600,20,600));assert(event(UI_GESTURE_MOVE,20,600,20,450));assert(enabled&&brightness==60&&warmth==120&&commits==0&&pauses==1);
 assert(event(UI_GESTURE_SWIPE_U,20,600,20,450));assert(commits==1&&!s_edge_light.candidate);
 enabled=false;assert(!event(UI_GESTURE_PRESS,660,600,660,600));assert(event(UI_GESTURE_SWIPE_D,660,600,660,750));assert(warmth==60&&!enabled&&commits==2);
 assert(!event(UI_GESTURE_PRESS,20,600,20,600));assert(event(UI_GESTURE_SWIPE_U,20,600,20,300));assert(brightness==120&&enabled);
 assert(!event(UI_GESTURE_PRESS,20,600,20,600));assert(!event(UI_GESTURE_MOVE,20,600,90,590));assert(!event(UI_GESTURE_SWIPE_L,20,600,90,590));
 left=false;unsigned old=adjusts;assert(!event(UI_GESTURE_PRESS,20,600,20,600));assert(!event(UI_GESTURE_SWIPE_U,20,600,20,300));assert(adjusts==old);
 assert(!event(UI_GESTURE_PRESS,660,600,660,600));assert(event(UI_GESTURE_MOVE,660,600,660,300));assert(warmth==180);assert(event(UI_GESTURE_CANCEL,660,600,660,300));assert(!s_edge_light.active);
 left=true;right=false;old=adjusts;assert(!event(UI_GESTURE_PRESS,660,600,660,600));assert(!event(UI_GESTURE_SWIPE_U,660,600,660,300));assert(adjusts==old);
 int ys[]={30,1120};for(unsigned i=0;i<2;++i){assert(!event(UI_GESTURE_PRESS,20,ys[i],20,ys[i]));assert(!s_edge_light.candidate);}
 s_toolbar=true;assert(!event(UI_GESTURE_PRESS,20,600,20,600)&&!s_edge_light.candidate);s_toolbar=false;
 s_zoom_active=true;assert(!event(UI_GESTURE_PRESS,20,600,20,600)&&!s_edge_light.candidate);s_zoom_active=false;
 brightness=230;enabled=true;event(UI_GESTURE_PRESS,20,1000,20,1000);assert(event(UI_GESTURE_SWIPE_U,20,1000,20,200)&&brightness==240);
 event(UI_GESTURE_PRESS,20,200,20,200);assert(event(UI_GESTURE_MOVE,20,200,20,1000)&&brightness==0);unsigned c=commits;reader_edge_light_cancel();assert(commits==c+1);reader_edge_light_cancel();assert(commits==c+1);
 puts("PASS actual reader edges: live+fast swipes, direction/clamps, off brightness enable, warmth switch preservation, independent toggles, horizontal/top/bottom/overlay/zoom guards, cancel/exit one commit");}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(u)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
