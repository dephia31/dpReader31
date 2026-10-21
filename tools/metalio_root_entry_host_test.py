#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
# 中文：执行真实文件页入场，确认已完成扫描不重复排队，挂载与分配失败仍可重试。
# English: Execute real file-page entry, avoiding duplicate completed scans while retaining mount/allocation retries.
from pathlib import Path
import subprocess
import tempfile
from home_cover_cache_host_test import function

root = Path(__file__).resolve().parents[1]
unit = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_NOT_FINISHED 2
#define FILE_VIEW_LIST 0
#define UI_PAGER_NONE -1
typedef int esp_err_t;
typedef struct {int64_t now_ms;} app_ctx_t;
typedef struct {bool mounted;} sd_info_t;
static sd_info_t s_sd;
static int s_pager_pressed,s_requested_folder=-1,s_folder,s_view,s_page,s_counts[3];
static bool s_pager_paint_pending,s_delete_confirm,s_scan_pending,s_sd_format_confirm;
static int64_t s_space_poll_ms;
static char s_dir[288],s_sd_notice[64];
static void *s_files;
static int status,probes,refreshes,counts,scans,folder;
static bool mounted;
static void read_pico_sd_start_probe(void){++probes;}
static int read_pico_sd_get_info(sd_info_t *info){info->mounted=mounted;return status;}
static int read_pico_sd_refresh_space(void){++refreshes;return 0;}
static int count_root(int i){assert(i>=0&&i<3);++counts;return i+1;}
static void scan_folder(int i){folder=i;++scans;}
'''
unit += function('on_enter', root / 'main/apps/app_files.c')
unit += r'''
static void reset(int error,bool ready,bool allocated){
 status=error;mounted=ready;s_files=allocated?(void*)1:NULL;
 probes=refreshes=counts=scans=0;s_requested_folder=-1;
 s_pager_pressed=4;s_pager_paint_pending=true;
}
int main(void){
 app_ctx_t ctx={.now_ms=12345};
 reset(ESP_OK,true,true);on_enter(&ctx);
 assert(!s_scan_pending&&probes==1&&refreshes==1&&counts==3&&scans==1&&folder==3);
 assert(s_space_poll_ms==12345&&!s_pager_paint_pending&&s_pager_pressed==UI_PAGER_NONE);
 reset(ESP_ERR_NOT_FINISHED,false,true);on_enter(&ctx);assert(s_scan_pending&&!refreshes);
 reset(ESP_OK,true,false);on_enter(&ctx);assert(s_scan_pending);
 reset(ESP_FAIL,false,true);on_enter(&ctx);assert(!s_scan_pending&&!refreshes);
 reset(ESP_OK,true,true);s_requested_folder=2;on_enter(&ctx);
 assert(!s_scan_pending&&folder==2&&s_requested_folder==-1&&counts==3&&scans==1);
 puts("PASS: actual S31 file entry performs one completed scan, no queued duplicate page, unchanged pager/root routing, mount/allocation retries retained");
}
'''
with tempfile.TemporaryDirectory(prefix='metalio-root-entry-') as directory:
    directory = Path(directory)
    source, binary = directory / 'test.c', directory / 'test'
    source.write_text(unit)
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', str(source), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
