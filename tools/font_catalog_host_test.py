#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""中文：真实字体扫描的128款边界、双目录去重与索引内存。/ English: Real font scanning at128 entries, cross-directory deduplication and metadata memory."""
from pathlib import Path
import subprocess
import tempfile
from home_cover_cache_host_test import function

root = Path(__file__).resolve().parents[1]
source = root / 'main/font/ttf_font.c'
unit = r'''
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <sys/stat.h>
#include "font_filename.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include "ttf_font.h"
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
static size_t allocated;static unsigned allocations;
static void* heap_caps_calloc(size_t n,size_t bytes,unsigned caps){
 assert(caps==(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));allocated=n*bytes;++allocations;return calloc(n,bytes);
}
static const char *k_font_dirs[2],*user_dir;
static const char *app_settings_fonts_dir(void){return user_dir;}
static ttf_font_item_t *s_catalog;static int s_catalog_n;
'''
for name in ('font_basename', 'is_junk_name', 'is_ttf_name', 'font_stem', 'font_item_cmp',
             'catalog_has_name', 'catalog_alloc', 'ttf_font_scan', 'ttf_font_count', 'ttf_font_item'):
    unit += function(name, source) + '\n'
unit += r'''
static void put(const char *dir,const char *name){
 char path[512];assert(snprintf(path,sizeof(path),"%s/%s",dir,name)<(int)sizeof(path));
 FILE *f=fopen(path,"wb");assert(f);assert(!fclose(f));
}
static void clear(const char *dir){
 DIR *d=opendir(dir);assert(d);struct dirent *ent;
 while((ent=readdir(d))){if(ent->d_name[0]=='.'&&(!ent->d_name[1]||!strcmp(ent->d_name,"..")))continue;
  char path[512];snprintf(path,sizeof(path),"%s/%s",dir,ent->d_name);assert(!unlink(path));}
 assert(!closedir(d));
}
int main(int argc,char **argv){
 assert(argc==3&&TTF_FONT_MAX==128&&sizeof(ttf_font_item_t)==224);assert(!strcmp(font_basename("/fonts/name.ttf"),"name.ttf"));
 k_font_dirs[0]=argv[1];k_font_dirs[1]=NULL;user_dir=argv[2];
 const int amounts[]={0,1,24,25,127,128,129,160};
 for(unsigned test=0;test<sizeof(amounts)/sizeof(amounts[0]);++test){
  clear(user_dir);int n=amounts[test];
  for(int i=0;i<n;++i){char name[40];snprintf(name,sizeof(name),"font-%03d%s",i,i%2?".TTF":".ttf");put(user_dir,name);}
  int expected=n>128?128:n;assert(ttf_font_scan()==expected&&ttf_font_count()==expected);
  for(int i=0;i<expected;++i){const ttf_font_item_t *p=ttf_font_item(i);assert(p&&strstr(p->path,user_dir));
   if(i)assert(strcasecmp(ttf_font_item(i-1)->name,p->name)<0);}
  assert(!ttf_font_item(-1)&&!ttf_font_item(expected));
  if(n==128)for(int i=0;i<n;++i){char name[40];snprintf(name,sizeof(name),"font-%03d",i);assert(!strcmp(ttf_font_item(i)->name,name));}
 }
 clear(user_dir);put(k_font_dirs[0],"Same.ttf");put(user_dir,"same.TTF");put(user_dir,"Other.ttf");
 put(user_dir,"._Ignored.ttf");put(user_dir,".hidden.ttf");put(user_dir,"readme.txt");
 #if TEST_METALIO
 assert(ttf_font_scan()==5);
 assert(strcmp(ttf_font_item(3)->path,ttf_font_item(4)->path));
#else
 assert(ttf_font_scan()==2);
 assert(!strcmp(ttf_font_item(0)->name,"Other")&&!strcmp(ttf_font_item(1)->name,"Same"));
 assert(strstr(ttf_font_item(1)->path,k_font_dirs[0]));
#endif
 clear(k_font_dirs[0]);clear(user_dir);
 for(int i=0;i<65;++i){char name[40];snprintf(name,sizeof(name),"asset-%03d.ttf",i);put(k_font_dirs[0],name);}
 for(int i=0;i<63;++i){char name[40];snprintf(name,sizeof(name),"user-%03d.ttf",i);put(user_dir,name);}
 assert(ttf_font_scan()==128&&!strcmp(ttf_font_item(127)->name,"user-062"));
 assert(allocations==1&&allocated==28672);
 free(s_catalog);s_catalog=NULL;clear(k_font_dirs[0]);clear(user_dir);
 puts("PASS: actual catalogue0/1/24/25/127/128/129/160 bounds,all128 reachable,sorted metadata,shared two-directory limit/casefold dedup/hidden-sidecar filtering;one28672B PSRAM allocation,no font-face loading");
}
'''
with tempfile.TemporaryDirectory(prefix='font-catalog-') as folder:
    work = Path(folder)
    (work / 'assets').mkdir()
    (work / 'fonts').mkdir()
    (work / 'esp_err.h').write_text('#pragma once\ntypedef int esp_err_t;\n')
    (work / 'epdiy.h').write_text('#pragma once\ntypedef struct{int x,y,width,height;} EpdRect;\nenum EpdFontFlags{EPD_DRAW_ALIGN_LEFT,EPD_DRAW_ALIGN_CENTER,EPD_DRAW_ALIGN_RIGHT};\n')
    (work / 'test.c').write_text(unit)
    subprocess.run(['cc', '-DTEST_METALIO=' + ('1' if 'font_filename_make' in source.read_text() else '0'), '-std=gnu11', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', '-I' + str(work), '-I' + str(root / 'main/font'),
                    str(work / 'test.c'), '-o', str(work / 'test')], check=True)
    subprocess.run([str(work / 'test'), str(work / 'assets'), str(work / 'fonts')], check=True)
