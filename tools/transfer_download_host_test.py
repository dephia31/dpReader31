#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Exercise the actual shared streaming handler with HTTP and filesystem faults."""
from pathlib import Path
import subprocess
import tempfile
from home_cover_cache_host_test import function
root = Path(__file__).resolve().parents[1]
source = root/'components/read_pico_transfer/read_pico_transfer.c'
unit = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_NOT_FOUND -2
#define portENTER_CRITICAL(x) ((void)0)
#define portEXIT_CRITICAL(x) ((void)0)
typedef struct {int content_len;} httpd_req_t;
static struct {bool is_flash;} s_cfg;
static bool s_upload_active,s_stopping,authorized=true;
static char buffer[17],*s_buffer=buffer;
static size_t s_buffer_capacity=sizeof(buffer),sent;
static int status,finished,send_calls,fail_send,fail_read;
static char request_path[800],disposition[800],received[10000];
static bool management_request(httpd_req_t*r){(void)r;return authorized;}
static int wifi_response(httpd_req_t*r,const char*s,const char*b){(void)r;(void)b;status=atoi(s);return ESP_OK;}
static int respond_error(httpd_req_t*r,int code){(void)r;status=code;return ESP_FAIL;}
static int httpd_req_get_url_query_str(httpd_req_t*r,char*out,size_t n){(void)r;snprintf(out,n,"path=%s",request_path);return 0;}
static int httpd_query_key_value(const char*q,const char*k,char*out,size_t n){(void)k;if(strlen(q+5)>=n)return -1;strcpy(out,q+5);return 0;}
static void httpd_resp_set_type(httpd_req_t*r,const char*t){(void)r;assert(!strcmp(t,"application/octet-stream"));}
static void httpd_resp_set_hdr(httpd_req_t*r,const char*k,const char*v){(void)r;if(!strcmp(k,"Content-Disposition"))strcpy(disposition,v);}
static int httpd_resp_send_chunk(httpd_req_t*r,const char*b,size_t n){(void)r;if(++send_calls==fail_send)return ESP_FAIL;if(!n){++finished;return 0;}assert(n<=sizeof(buffer)&&sent+n<=sizeof(received));memcpy(received+sent,b,n);sent+=n;return 0;}
static int hex_value(char c){if(c>='0'&&c<='9')return c-'0';if(c>='A'&&c<='F')return c-'A'+10;if(c>='a'&&c<='f')return c-'a'+10;return -1;}
static bool sd_absolute(const char*r,char*out,size_t n){return snprintf(out,n,"%s",r)<(int)n;}
static bool admission_begin(bool stopping,bool*active){if(stopping||*active)return false;*active=true;return true;}
static size_t read_test(void*p,size_t s,size_t n,FILE*f){return fail_read?0:fread(p,s,n,f);}
static int error_test(FILE*f){return fail_read||ferror(f);}
#define fread read_test
#define ferror error_test
'''
unit += function('sd_relative_valid', source) + '\n' + function('sd_decode_relative', source)
unit += '\n#include "transfer_download_handler.h"\n'
unit += r'''
static void reset(const char*p){strcpy(request_path,p);status=200;finished=send_calls=sent=fail_send=fail_read=0;s_stopping=s_upload_active=s_cfg.is_flash=false;authorized=true;disposition[0]=0;}
int main(void){
 httpd_req_t req={0};unsigned char original[4097];for(size_t i=0;i<sizeof(original);++i)original[i]=(unsigned char)i;
 FILE*f=fopen("log tést.txt","wb");assert(f&&fwrite(original,1,sizeof(original),f)==sizeof(original)&&!fclose(f));
 reset("log%20t%C3%A9st.txt");assert(file_download_handler(&req)==0&&finished==1&&sent==sizeof(original)&&!memcmp(original,received,sent)&&!s_upload_active);assert(strstr(disposition,"filename*=UTF-8''%6C%6F%67%20%74%C3%A9%73%74%2E%74%78%74"));
 f=fopen("empty.txt","wb");assert(f&&!fclose(f));reset("empty.txt");assert(!file_download_handler(&req)&&finished==1&&!sent);
 const char*invalid[]={"../log.txt","%2E%2E%2Flog.txt","%00","a%0D%0Ab","/log.txt","a\\b","%ZZ",""};
 for(size_t i=0;i<sizeof(invalid)/sizeof(*invalid);++i){reset(invalid[i]);assert(file_download_handler(&req)==ESP_FAIL&&status==400&&!sent&&!finished&&!s_upload_active);}
 reset("missing.txt");assert(!file_download_handler(&req)&&status==404&&!finished&&!s_upload_active);
 assert(!mkdir("folder",0700));reset("folder");assert(!file_download_handler(&req)&&status==404&&!finished&&!s_upload_active);
 char longest[239];memset(longest,'x',238);longest[238]=0;f=fopen(longest,"wb");assert(f&&!fclose(f));reset(longest);assert(!file_download_handler(&req)&&finished==1&&strlen(disposition)<800);
 reset("empty.txt");authorized=false;assert(!file_download_handler(&req)&&status==403);
 reset("empty.txt");s_cfg.is_flash=true;assert(!file_download_handler(&req)&&status==400);
 reset("empty.txt");s_stopping=true;assert(!file_download_handler(&req)&&status==409&&!s_upload_active);
 reset("empty.txt");s_upload_active=true;assert(!file_download_handler(&req)&&status==409&&s_upload_active);
 reset("log%20t%C3%A9st.txt");fail_send=2;assert(file_download_handler(&req)==ESP_FAIL&&!finished&&!s_upload_active);
 reset("empty.txt");fail_read=1;assert(file_download_handler(&req)==ESP_FAIL&&!finished&&!s_upload_active);
 puts("PASS: download byte-exact multi-chunk/empty/UTF-8, traversal/header injection, missing/directory, origin/media/busy guards, read/send failures release admission");
}
'''
with tempfile.TemporaryDirectory(prefix='download-test-') as folder:
    p=Path(folder);(p/'test.c').write_text(unit)
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined','-I'+str(root/'components/read_pico_transfer'),str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],cwd=p,check=True)
# Both independently compiled board backends must register the new endpoint.
s=source.read_text()
assert s.count('#include "transfer_download_handler.h"')==2
assert s.count('.uri = "/download", .method = HTTP_GET')==2
assert s.count('http.max_uri_handlers = 21')==2
