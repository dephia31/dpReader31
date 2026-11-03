#!/usr/bin/env python3
"""Run actual OPDS-entry painter/fetch ordering, including OOM and network failure."""
from pathlib import Path
import subprocess,tempfile
from home_cover_cache_host_test import function
ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'main/apps/app_book.c'
unit=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
typedef struct{int x,y,width,height;}EpdRect;
typedef struct{uint8_t *fb;void *hl;}app_ctx_t;
typedef struct{unsigned count;}opds_feed_t;
enum{UI_GRAY_WHITE=255,UI_GRAY_BLACK=0,EPD_DRAW_ALIGN_CENTER=1,MODE_GL16=16,OPDS=2,ESP_OK=0};
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
static int E0470_WAVEFORM,stage,s_view,s_opds_page,s_opds_depth,fetches,draws;
static bool simple,oom,network_fail;
static opds_feed_t *s_opds_feed;
static char s_opds_url[256],s_opds_status[128];
static bool app_settings_simple_ui(void){return simple;}
static void ui_fill_round_rect(uint8_t *fb,EpdRect r,int radius,int gray){assert(fb&&r.width==540&&r.height==176&&radius==24&&gray==255);stage=1;}
static void ui_draw_control_frame(uint8_t *fb,EpdRect r,int radius,int gray){assert(fb&&r.y==464&&radius==24&&gray==0&&stage==1);stage=2;}
static void ui_text_box(uint8_t *fb,EpdRect r,int px,const char *s,int align,bool inverted){assert(fb&&r.x==96&&r.width==492&&r.height==88&&px==30&&align==1&&!inverted&&!strcmp(s,"Đang mở thư viện")&&stage==2);stage=3;}
static int update_display_area_diff_with(void *hl,int *wave,int mode,EpdRect r){assert(hl&&wave==&E0470_WAVEFORM&&mode==MODE_GL16&&r.y==464&&stage==3);stage=4;++draws;return 0;}
static void guard_draw_result(void *hl,int result){assert(hl&&!result&&stage==4);stage=5;}
static void *heap_caps_calloc(size_t n,size_t z,int caps){assert(caps==3);if(simple)assert(stage==5);return oom?NULL:calloc(n,z);}
static void copy_text(char *out,size_t capacity,const char *text){snprintf(out,capacity,"%s",text);}
static const char *app_settings_opds_url(void){return "https://example.test/opds";}
static int opds_fetch_feed(const char *url,opds_feed_t *feed,char *status,size_t capacity){
 assert(!strcmp(url,"https://example.test/opds"));
 assert(stage==(simple?5:0));++fetches;stage=6;
 snprintf(status,capacity,"%s",network_fail?"Network unavailable":"OK");feed->count=network_fail?0:3;
 return network_fail?-1:ESP_OK;
}
'''
unit+=function('opds_open_notice',SOURCE)+'\n'+function('opds_open_catalog',SOURCE)+r'''
int main(void){
 uint8_t pixel=0;app_ctx_t ctx={&pixel,&pixel};
 for(unsigned mode=0;mode<2;++mode)for(unsigned failure=0;failure<2;++failure){
  simple=mode;network_fail=failure;stage=0;int before=fetches,shown=draws;
  opds_open_catalog(&ctx);assert(fetches==before+1&&draws==shown+(int)mode&&stage==6&&s_view==OPDS);
  if(failure)assert(!strcmp(s_opds_status,"Network unavailable"));
  free(s_opds_feed);s_opds_feed=NULL;
 }
 simple=true;oom=true;stage=0;int before=fetches;opds_open_catalog(&ctx);
 assert(stage==5&&fetches==before&&!s_opds_feed&&!strcmp(s_opds_status,"Không đủ bộ nhớ PSRAM"));
 puts("PASS OPDS notice: physically presented before fetch in simple mode, normal entry unchanged, network/OOM errors retained");
}
'''
with tempfile.TemporaryDirectory() as tmp:
 c=Path(tmp)/'notice.c';exe=Path(tmp)/'notice';c.write_text(unit)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(c),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
