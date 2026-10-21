#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
"""Real excerpt extraction and atomic cache IO, with /sdcard mapped into a temporary fixture."""
from pathlib import Path
import subprocess,tempfile
from home_cover_cache_host_test import function
ROOT=Path(__file__).resolve().parents[1]
unit=r'''
#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include "book_excerpt.h"
#include "book_index_cache.h"
static unsigned writes;
static const char *mapped(const char *path,char *out){
 assert(!strncmp(path,"/sdcard",7));assert(strlen(path)+strlen(TEST_ROOT)<1024);
 snprintf(out,1024,"%s%s",TEST_ROOT,path+7);return out;
}
static FILE *test_fopen(const char *path,const char *mode){char p[1024];if(mode[0]=='w')++writes;return fopen(mapped(path,p),mode);}
static int test_stat(const char *path,struct stat *st){char p[1024];return stat(mapped(path,p),st);}
static int test_mkdir(const char *path,mode_t mode){char p[1024];return mkdir(mapped(path,p),mode);}
static int test_remove(const char *path){char p[1024];return remove(mapped(path,p));}
static int test_rename(const char *from,const char *to){char p[1024],q[1024];return rename(mapped(from,p),mapped(to,q));}
#define fopen(path,mode) test_fopen(path,mode)
#define stat(path,out) test_stat(path,out)
#define mkdir(path,mode) test_mkdir(path,mode)
#define remove(path) test_remove(path)
#define rename(from,to) test_rename(from,to)
'''
unit+='\n#include "'+str(ROOT/'main/book/book_index_cache.c')+'"\n'
unit+='\n#include "'+str(ROOT/'main/book/book_excerpt.c')+'"\n'
unit+=r'''
typedef struct {char path[288];bool has_progress;struct {uint16_t chapter;}progress;}home_book_t;
static home_book_t current;
static char s_excerpt[BOOK_EXCERPT_CAP];
static home_book_t *featured(void){return &current;}
'''
unit+=function('refresh_excerpt',ROOT/'main/apps/app_dashboard.c')+'\n'
unit+=r'''
static void offer(unsigned chapter,const char *s){html_text_t t={.utf8=(char*)s,.len=strlen(s)};book_excerpt_offer("/sdcard/book.epub",chapter,&t);}
static void read_quote(unsigned chapter,const char *expected){char out[BOOK_EXCERPT_CAP];memset(out,0xaa,sizeof(out));bool found=book_excerpt_load("/sdcard/book.epub",chapter,out);assert(found==(expected!=NULL));assert(expected?!strcmp(out,expected):out[0]==0);}
int main(void){
 assert(!mkdir("/sdcard",0777)||errno==EEXIST);FILE *book=fopen("/sdcard/book.epub","wb");assert(book);fputs("test book source",book);fclose(book);
 const char *first="Chapter one has a quiet morning.",*second="Chapter two follows a different path.";
 read_quote(0,NULL);offer(0,first);read_quote(0,first);read_quote(1,NULL);
 unsigned before=writes;offer(0,first);assert(writes==before);
 offer(1,second);read_quote(1,second);read_quote(0,NULL);
 strcpy(current.path,"/sdcard/book.epub");current.has_progress=true;current.progress.chapter=1;
 refresh_excerpt();assert(!strcmp(s_excerpt,second));
 current.progress.chapter=0;refresh_excerpt();assert(!s_excerpt[0]);
 current.progress.chapter=1;refresh_excerpt();assert(!strcmp(s_excerpt,second));
 current.has_progress=false;refresh_excerpt();assert(!s_excerpt[0]);
 // 没有完整句子的章节不能继续显示上一章；回跳重新取该章。
 // Chapters without a complete sentence cannot reuse a previous chapter; jumping back re-offers its text.
 offer(2,"unfinished text without punctuation");read_quote(2,NULL);read_quote(1,second);
 offer(0,first);read_quote(0,first);read_quote(1,NULL);
 // 标题、目录链接、图片与辅助文字均不作为正文书摘。/ Skip headings, navigation links, images and auxiliary text.
 char blocks_text[]="A heading is not a quotation.A linked line is not a quote.An auxiliary line is not a quote.An image caption is not a quote.当前章节里的这句话才是书摘。";
 const char *parts[]={"A heading is not a quotation.","A linked line is not a quote.","An auxiliary line is not a quote.","An image caption is not a quote.","当前章节里的这句话才是书摘。"};
 blk_t b[5]={0};size_t offset=0;for(unsigned i=0;i<5;++i){b[i].offset=offset;b[i].len=strlen(parts[i]);b[i].image=-1;offset+=b[i].len;}
 b[0].heading=true;b[1].linked=true;b[2].auxiliary=true;b[3].image=0;
 html_text_t t={.utf8=blocks_text,.len=strlen(blocks_text),.blocks=b,.count=5};book_excerpt_offer("/sdcard/book.epub",3,&t);read_quote(3,parts[4]);
 // 章节号也受校验保护；截断和旧格式均拒绝，不回放整本书旧句子。
 // Protect the chapter field as well as text; reject truncation and legacy book-wide caches.
 char path[160],temp[176];book_index_cache_header_t key;assert(book_index_cache_prepare("/sdcard/book.epub","quote",2,path,sizeof(path),&key));
 FILE *cache=fopen(path,"rb+");assert(cache);fseek(cache,sizeof(key),SEEK_SET);uint32_t changed=4;assert(fwrite(&changed,1,4,cache)==4);fclose(cache);read_quote(4,NULL);
 offer(4,second);read_quote(4,second);cache=fopen(path,"wb");assert(cache);fclose(cache);read_quote(4,NULL);
 assert(book_index_cache_prepare("/sdcard/book.epub","quote",1,path,sizeof(path),&key));cache=book_index_cache_open_write(path,&key,temp,sizeof(temp));assert(cache);assert(book_index_cache_finish_write(cache,temp,path,true));read_quote(0,NULL);
 offer(5,first);read_quote(5,first);book=fopen("/sdcard/book.epub","ab");assert(book);fputc('x',book);fclose(book);read_quote(5,NULL);
 assert(!book_excerpt_load(NULL,0,NULL));
 puts("PASS: actual chapter-scoped excerpt extraction/cache, same-chapter reuse without writes, next/back jumps, empty chapter, skipped headings/images/links, UTF-8, payload corruption, old version and changed source");
}
'''
with tempfile.TemporaryDirectory() as folder:
    p=Path(folder);(p/'test.c').write_text(unit)
    subprocess.run(['cc','-std=c11','-D_DEFAULT_SOURCE','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-DTEST_ROOT="'+str(p/'card')+'"','-I'+str(ROOT/'main/book'),'-I'+str(ROOT/'tools/html_text_stubs'),str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
