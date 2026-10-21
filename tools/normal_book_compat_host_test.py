#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 mindreset
# SPDX-License-Identifier: Apache-2.0
"""中文：普通版拒绝历史中的非本地书源，继续阅读/开书不被无效清单占用。
English: Unsupported history sources must not occupy ordinary shelf, Home or open requests.
"""
from pathlib import Path
import subprocess
import sys
import tempfile
from home_cover_cache_host_test import function

ROOT = Path(__file__).resolve().parents[1]
reader = ROOT / 'main/apps/app_book.c'
home = ROOT / 'main/apps/app_dashboard.c'
unit = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include "book_file_path.h"
#define BOOK_STORE_PATH_MAX 288
#define NVS_READONLY 0
#define SHELF_HIDDEN_NS "rp_shelf_hide"
#define ESP_OK 0
#define ESP_FAIL (-1)
typedef int nvs_handle_t;
typedef struct {int leaf;} app_ctx_t;
typedef struct {uint32_t file_size;uint16_t chapter;uint32_t byte_off;uint8_t px,pct;uint32_t last_open_s;} book_progress_t;
typedef bool (*book_progress_visit_fn)(const char*,const book_progress_t*,void*);
typedef struct {char path[288];book_progress_t progress;} saved_t;
static saved_t records[5];
static char last[288],books_root[288];
static unsigned revision=1,s_progress_revision;
static int reserves,sorts;
static int nvs_open(const char *ns,int mode,nvs_handle_t *h){(void)ns;assert(mode==0);*h=1;return ESP_OK;}
static void nvs_close(nvs_handle_t h){assert(h==1);}
static bool shelf_hidden_read_handle(nvs_handle_t h,const char *p){assert(h==1);(void)p;return false;}
static bool favorite_read_handle(nvs_handle_t h,const char *p){assert(h==1);(void)p;return false;}
static int book_progress_list(book_progress_visit_fn visit,void *ctx){for(int i=0;i<5;++i)if(!visit(records[i].path,&records[i].progress,ctx))break;return ESP_OK;}
static bool book_progress_last_path(char *out,size_t cap){snprintf(out,cap,"%s",last);return *last;}
static bool book_progress_load(const char *p,uint32_t size,book_progress_t *out){
 for(int i=0;i<5;++i)if(!strcmp(p,records[i].path)&&records[i].progress.file_size==size){*out=records[i].progress;return true;}return false;
}
static unsigned book_progress_revision(void){return revision;}
static void copy_text(char *out,size_t cap,const char *s){snprintf(out,cap,"%s",s);}
static bool book_title_from_path(const char *p,char *out,size_t cap){const char *base=strrchr(p,'/');copy_text(out,cap,base?base+1:p);return true;}
static void clean_filename(char *out,size_t cap,const char *s){copy_text(out,cap,s);}
static bool pico_boot_asset_allowed(const char *p){(void)p;return true;}
static bool read_pico_search_match(const char *name,const char *query){(void)name;(void)query;return true;}
static int book_epub_metadata_cached(const char *p,char *title,size_t tcap,char *author,size_t acap){assert(book_file_path_supported(p));title[0]=author[0]=0;(void)tcap;(void)acap;return ESP_FAIL;}
static int book_epub_metadata(const char *p,char *title,size_t tcap,char *author,size_t acap){return book_epub_metadata_cached(p,title,tcap,author,acap);}
'''
text = reader.read_text()
unit += text[text.index('typedef struct {\n    char name[256];'):text.index('static const char* TAG = "book";')]
unit += r'''
static shelf_entry_t s_shelf[8];
static int s_count,s_visible_count,s_view=0;
enum {SHELF};
static char s_query[65],s_latest_path[288],s_requested_open[288];
static bool s_recent_sort=true,s_requested_open_home,s_requested_fullscreen;
typedef struct {nvs_handle_t favorites,hidden;bool truncated;} shelf_backfill_t;
static bool shelf_reserve(void){++reserves;return s_count<8;}
static void shelf_category_restore(app_ctx_t *ctx){(void)ctx;}
static void shelf_categories_recount(void){}
static void sort_shelf(app_ctx_t *ctx){(void)ctx;++sorts;s_visible_count=s_count;}
'''
for name in ('app_book_request_open','app_book_request_open_from_home','app_book_request_resume',
             'shelf_backfill_visit','shelf_backfill_read_books','refresh_cached_progress'):
    unit += function(name, reader) + '\n'
unit += r'''
typedef struct {char path[288],title[256],author[128];time_t modified;bool has_progress;book_progress_t progress;} home_book_t;
static home_book_t s_current,s_recent[6];
static home_book_t *book_at(int index){return index==0?&s_current:&s_recent[index-1];}
static const char *app_settings_books_dir(void){return books_root;}
static bool book_store_flash_ready(void){return false;}
typedef struct {bool mounted;} read_pico_sd_info_t;
static void read_pico_sd_get_info(read_pico_sd_info_t *out){out->mounted=true;}
'''
for name in ('is_book','copy_title','title_for','consider_book','scan_root','recent_visit','scan_books'):
    unit += function(name, home) + '\n'
unit += r'''
static void seed(const char *dir){
 const char *names[]={"books/first.TXT","books/second.EpUb","cache/catalog.wread","cache/catolog.weread","cache/other.bin"};
 for(int i=0;i<5;++i){snprintf(records[i].path,288,"%s/%s",dir,names[i]);records[i].progress=(book_progress_t){.file_size=1,.chapter=2,.byte_off=17,.px=48,.pct=42,.last_open_s=(uint32_t)i+1};}
 snprintf(books_root,sizeof(books_root),"%s/books",dir);strcpy(last,records[2].path);
}
int main(int argc,char **argv){
 assert(argc==3);seed(argv[1]);saved_t before[5];memcpy(before,records,sizeof(before));char oldlast[288];strcpy(oldlast,last);app_ctx_t ctx={0};
 if(!strcmp(argv[2],"shelf")){
  bool truncated=false;assert(shelf_backfill_read_books(&truncated));assert(!truncated&&s_count==2&&reserves==2);
  for(int repeat=0;repeat<5;++repeat){++revision;refresh_cached_progress(&ctx);assert(s_count==2);}
  for(int i=0;i<s_count;++i)assert(book_file_path_supported(s_shelf[i].path)&&s_shelf[i].has_progress&&s_shelf[i].pct==42);
  assert(s_shelf[0].size==1&&s_shelf[1].size==1);
 }else if(!strcmp(argv[2],"home")){
  for(int i=2;i<5;++i){
   memset(&s_current,0,sizeof(s_current));memset(s_recent,0,sizeof(s_recent));strcpy(last,records[i].path);scan_books();
   assert(!s_current.path[0]);assert(!strcmp(s_recent[0].path,records[1].path));
   for(int j=0;j<6;++j)assert(!s_recent[j].path[0]||book_file_path_supported(s_recent[j].path));
  }
  memset(&s_current,0,sizeof(s_current));memset(s_recent,0,sizeof(s_recent));strcpy(last,records[0].path);scan_books();assert(!strcmp(s_current.path,records[0].path));
  strcpy(last,oldlast);
 }else{
  assert(app_book_request_open_from_home(records[0].path)&&s_requested_open_home);
  for(int i=2;i<5;++i){assert(!app_book_request_open(records[i].path));assert(!app_book_request_resume(records[i].path,true));assert(!strcmp(s_requested_open,records[0].path));}
  assert(!app_book_request_open(NULL)&&!app_book_request_open(""));
  assert(app_book_request_resume(records[1].path,false)&&!s_requested_fullscreen&&!strcmp(s_requested_open,records[1].path));
 }
 assert(!memcmp(before,records,sizeof(before))&&!strcmp(last,oldlast));
 printf("PASS: %s ignores unsupported shared records, accepts real local paths and preserves history\n",argv[2]);
}
'''
with tempfile.TemporaryDirectory(prefix='normal-book-compat-') as directory:
    tmp = Path(directory)
    names = ['books/first.TXT','books/second.EpUb','cache/catalog.wread','cache/catolog.weread','cache/other.bin']
    for name in names:
        p = tmp / name
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_bytes(b'x')
    c, exe = tmp/'test.c', tmp/'test'
    c.write_text(unit)
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-variable','-Wno-unused-function','-Wno-sign-compare',
                    '-fsanitize=address,undefined','-I'+str(ROOT/'main/book'),str(c),'-o',str(exe)],check=True)
    for case in ['shelf','home','request']:
        result = subprocess.run([str(exe),str(tmp),case],capture_output=True,text=True)
        if '--expect-fail' in sys.argv:
            assert result.returncode != 0 and 'Assertion' in result.stderr, result.stdout+result.stderr
            print('REPRODUCED before fix:',case)
        else:
            assert result.returncode == 0, result.stdout+result.stderr
            print(result.stdout.strip())
    assert all((tmp/name).read_bytes() == b'x' for name in names)
