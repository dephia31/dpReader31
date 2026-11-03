#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Execute real KoSync JSON producers/consumers with real EPUB position conversion, no network."""
from pathlib import Path
import tempfile,subprocess,ast,zipfile,os
root=Path(__file__).resolve().parents[1]
ns={'SOURCE':root/'main/book/kosync_client.c'}
tree=ast.parse((root/'tools/kosync_progress_host_test.py').read_text());node=next(n for n in tree.body if isinstance(n,ast.FunctionDef) and n.name=='function');exec(compile(ast.Module(body=[node],type_ignores=[]),'extract','exec'),ns)
prefix=r'''
#define TRANSFER_NETWORK_GUARD_OR_RETURN(...) ((void)0)

#include "kosync_client.h"
#include "book_sync_position.h"
#include "book_epub.h"
#include "kosync_state.h"
#include "cJSON.h"
#include "esp_heap_caps.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <math.h>
#include <errno.h>
#include <assert.h>
#include <time.h>
#define HTTP_METHOD_PUT 1
#define HTTP_METHOD_GET 2
#define ESP_ERR_INVALID_STATE 0x103
#define ESP_MAC_WIFI_STA 0
static char response[2048],captured[2048];static int puts_count,get_status=200;static int64_t local_time,clock_now=1800000000;
static int ttf_unused;
int ttf_font_open_mem(unsigned char*p,size_t n,const char*l){(void)n;(void)l;free(p);ttf_unused++;return -1;}void ttf_font_close_embedded(void){ttf_unused++;}
static size_t safe_copy(char*d,const char*s,size_t n){size_t len=strlen(s);if(n){size_t k=len<n-1?len:n-1;memcpy(d,s,k);d[k]=0;}return len;}
#undef strlcpy
#define strlcpy safe_copy
int kosync_compute_doc_md5(const char*p,char out[33]){(void)p;strcpy(out,"11111111111111111111111111111111");return ESP_OK;}
static int esp_read_mac(unsigned char*p,int n){(void)n;memset(p,1,6);return 0;}
static const char*app_settings_device_name(void){return "dpReader test";}
static const char*app_settings_kosync_user(void){return "test";}
static int kosync_acquire_network(bool*b,char*m,size_t n){(void)m;(void)n;*b=false;return ESP_OK;}
static void kosync_release_network(bool b){(void)b;}
void kosync_state_commit(const char*p){(void)p;}
void kosync_state_accept(const char*p,uint16_t ch,uint32_t off,int64_t t){(void)p;(void)ch;(void)off;local_time=t;}
int64_t kosync_state_timestamp(const char*p,uint16_t ch,uint32_t off){(void)p;(void)ch;(void)off;return local_time;}
bool kosync_state_remote_newer(int64_t l,int64_t r,bool*k){*k=l>=1700000000&&r>=1700000000&&r<=clock_now+300&&l!=r;return *k&&r>l;}
static int kosync_request(int method,const char*p,const char*body,int*status,char*out,size_t n,char*m,size_t cap){
 (void)p;(void)m;(void)cap;
 if(method==HTTP_METHOD_PUT){puts_count++;safe_copy(captured,body,sizeof(captured));*status=200;safe_copy(out,"{}",n);}else{*status=get_status;safe_copy(out,response,n);}return ESP_OK;
}
'''
functions='\n'.join(ns['function'](n) for n in ['kosync_push_book_progress_impl','kosync_parse_pointer','kosync_remote_wins','kosync_should_pull','kosync_pull_book_progress_impl'])
wrappers=r'''
esp_err_t kosync_push_book_progress(const char*p,const book_progress_t*v,double r,char*m,size_t n){return kosync_push_book_progress_impl(p,v,r,m,n);}
esp_err_t kosync_pull_book_progress(const char*p,kosync_progress_t*r,uint16_t*ch,uint32_t*o,bool*ex,char*m,size_t n){return kosync_pull_book_progress_impl(p,r,ch,o,ex,m,n);}
'''
main=r'''
int main(int argc,char**argv){assert(argc==3);book_progress_t local={.chapter=1,.byte_off=5432,.pct=42};char message[160];
 book_epub_t*book=NULL;html_text_t text={0};assert(book_epub_open(argv[1],&book)==ESP_OK);assert(book_epub_load(book,1,&text)==ESP_OK);local.byte_off=(uint32_t)(strstr(text.utf8,"P0500")-text.utf8);html_text_free(&text);book_epub_close(book);
 assert(kosync_push_book_progress_impl(argv[1],&local,.42,message,sizeof(message))==ESP_OK&&puts_count==1);
 cJSON*json=cJSON_Parse(captured);assert(json);cJSON*j=cJSON_GetObjectItemCaseSensitive(json,"progress"),*pct=cJSON_GetObjectItemCaseSensitive(json,"percentage");
 assert(cJSON_IsString(j)&&strstr(j->valuestring,"/text()["));assert(cJSON_IsNumber(pct)&&fabs(pct->valuedouble*100-round(pct->valuedouble*100))>0.000001);
 safe_copy(response,captured,sizeof(response));cJSON_Delete(json);
 kosync_progress_t remote;uint16_t ch=0;uint32_t off=0;bool exact=false;
 assert(kosync_pull_book_progress_impl(argv[1],&remote,&ch,&off,&exact,message,sizeof(message))==ESP_OK);
 assert(remote.position_valid&&exact&&ch==local.chapter&&off==local.byte_off);
 local_time=clock_now;remote.timestamp=clock_now-30;assert(!kosync_should_pull(argv[1],0,0,.01,&remote,1,9000,true));
 local_time=clock_now-60;remote.timestamp=clock_now-30;assert(kosync_should_pull(argv[1],1,9000,.9,&remote,1,100,true));
 safe_copy(response,"{\"percentage\":2,\"progress\":\"bad\"}",sizeof(response));bool pulled=false;uint8_t percent=42;ch=1;off=5432;int before=puts_count;
 assert(kosync_sync_book_impl(argv[1],&ch,&off,&percent,.4299,&pulled,message,sizeof(message))==ESP_ERR_INVALID_RESPONSE&&puts_count==before);
 get_status=500;assert(kosync_sync_book_impl(argv[1],&ch,&off,&percent,.4299,&pulled,message,sizeof(message))==ESP_ERR_INVALID_RESPONSE&&puts_count==before);
 get_status=200;local.byte_off+=7;
 double fallback_ratio=-1;char fallback[BOOK_XPOINTER_CAP];bool precise=true;
 assert(book_sync_export(argv[2],local.chapter,local.byte_off,fallback,sizeof(fallback),&fallback_ratio,&precise)==ESP_OK&&!precise);
 assert(strstr(fallback,"/p[501]")&&!strstr(fallback,"text()"));
 assert(kosync_push_book_progress_impl(argv[2],&local,.42,message,sizeof(message))==ESP_OK&&strstr(message,"ước lượng"));
 safe_copy(response,captured,sizeof(response));
 assert(kosync_pull_book_progress_impl(argv[2],&remote,&ch,&off,&exact,message,sizeof(message))==ESP_OK&&remote.position_valid&&ch==1&&off==local.byte_off-7);
 assert(!ttf_unused);puts("Actual KoSync wire: full fractional JSON, real EPUB XPointer push/pull roundtrip, latest backward state, stale ahead state and no overwrite on corrupt/server-error GET PASS");
}
'''
cjson=Path(os.environ.get('CJSON_DIR',root/'build/boards/metalio47/source/managed_components/espressif__cjson/cJSON'))
if (cjson/'cJSON.c').is_file():json_flags=['-I'+str(cjson)];json_sources=[str(cjson/'cJSON.c')];json_libs=[]
else:json_flags=['-I/usr/include/cjson'];json_sources=[];json_libs=['-lcjson']
with tempfile.TemporaryDirectory() as d:
 p=Path(d);book=p/'long.epub'
 with zipfile.ZipFile(book,'w',zipfile.ZIP_DEFLATED) as z:
  z.writestr('META-INF/container.xml','<container><rootfiles><rootfile full-path="book.opf"/></rootfiles></container>')
  z.writestr('book.opf','<package><manifest><item id="a" href="a.xhtml" media-type="application/xhtml+xml"/><item id="b" href="b.xhtml" media-type="application/xhtml+xml"/></manifest><spine><itemref idref="a"/><itemref idref="b"/></spine></package>')
  for n in ['a','b']:z.writestr(n+'.xhtml','<html><body><div>'+''.join(f'<p>P{i:04d} Tiếng Việt có dấu và đoạn dài</p>' for i in range(1000))+'</div></body></html>')
 fallback=p/'fallback.epub'
 with zipfile.ZipFile(book) as src,zipfile.ZipFile(fallback,'w',zipfile.ZIP_DEFLATED) as out:
  for n in src.namelist():
   data=src.read(n)
   if n=='b.xhtml':data=data.replace(b'<html><body>',b'<html><head><link rel="stylesheet" href="style.css"/></head><body>').replace(b'<p>P0500',b'<p>  P0500')
   out.writestr(n,data)
  out.writestr('style.css','.unused {white-space: pre-wrap;}')
 (p/'wire.c').write_text(prefix+functions+wrappers+ns['function']('kosync_sync_book_impl')+main)
 sources=['main/book/book_sync_position.c','main/book/book_xpointer.c','main/book/book_epub.c','main/book/book_index_cache.c','main/book/book_image_header.c','main/book/zip_reader.c','main/book/html_text.c']
 subprocess.run(['cc','-std=c11','-D_DEFAULT_SOURCE','-Wall','-Wextra','-Werror','-fsanitize=undefined','-Itools/book_epub_stubs','-Itools/zip_host_stubs','-Imain/book',*json_flags,str(p/'wire.c'),*sources,*json_sources,*json_libs,'-lz','-lm','-o',str(p/'test')],cwd=root,check=True)
 subprocess.run([str(p/'test'),str(book),str(fallback)],cwd=root,check=True)
