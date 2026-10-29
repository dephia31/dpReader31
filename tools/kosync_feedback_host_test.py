"""中文：先推同步提示；验证真实HTTP请求的成功/失败资源回收。/ English: Present sync feedback first; verify real HTTP success/failure cleanup."""
from pathlib import Path
import subprocess,tempfile
from home_cover_cache_host_test import function
root=Path(__file__).resolve().parents[1];book=root/'main/apps/app_book.c';client=root/'main/book/kosync_client.c'
notice=r'''
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
typedef struct{int x,y,width,height;}EpdRect;
typedef struct{void*fb;void*hl;}app_ctx_t;
static char s_reader_notice[64];static int64_t s_reader_notice_until;static int step;
#define EPD_DRAW_ALIGN_CENTER 0
#define MODE_GL16 1
static int E0470_WAVEFORM;
static void copy_text(char*d,size_t n,const char*s){snprintf(d,n,"%s",s);}
static void lock_draw(void){assert(step++==0);}
static void ui_fill_round_rect(void*f,EpdRect r,int radius,int gray){assert(step++==1);}
static void ui_text_vc(void*f,int x,int y,int px,const char*t,int align,int bold){assert(step++==2);assert(!strcmp(t,"Đang đồng bộ"));}
static void unlock_draw(void){assert(step++==3);}
static int update_display_area_diff_with(void*h,void*w,int mode,EpdRect r){assert(step++==4);assert(r.x==218&&r.y==160&&r.width==248&&r.height==48);return 0;}
static void guard_draw_result(void*h,int err){assert(step++==5);}
'''+function('reader_show_sync_notice',book)+r'''
int main(void){app_ctx_t ctx={(void*)1,(void*)2};reader_show_sync_notice(&ctx);assert(step==6&&s_reader_notice_until==INT64_MAX);puts("PASS: exact sync text painted and presented immediately with no heap allocation");}
'''
request=r'''
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef int esp_err_t;typedef int esp_http_client_method_t;typedef void*esp_http_client_handle_t;
typedef struct{char*buf;size_t len,cap;}http_resp_t;
typedef struct{const char*url;int method,timeout_ms;void*crt_bundle_attach,*event_handler,*user_data;int buffer_size,buffer_size_tx;}esp_http_client_config_t;
#define ESP_OK 0
#define ESP_ERR_INVALID_STATE 1
#define ESP_ERR_NO_MEM 2
#define DPREADER_DEFAULT_KOSYNC_URL "https://example.test"
#define esp_crt_bundle_attach NULL
#define kosync_http_event NULL
#define ESP_LOGW(...) ((void)0)
static bool own,fail_init;static int perform_error,live,cleanups,releases;static void*allocation;
static const char*app_settings_kosync_url(void){return DPREADER_DEFAULT_KOSYNC_URL;}
static const char*app_settings_kosync_user(void){return "user";}
static const char*app_settings_kosync_key(void){return "key";}
#undef strlcpy
#define strlcpy checked_copy
static size_t checked_copy(char*d,const char*s,size_t n){snprintf(d,n,"%s",s);return strlen(s);}
static int kosync_acquire_network(bool*started,char*m,size_t n){*started=own;return 0;}
static void kosync_release_network(bool started){assert(!live);assert(started==own);releases++;}
static void*esp_http_client_init(const esp_http_client_config_t*c){assert(!live);if(fail_init)return NULL;allocation=malloc(c->buffer_size+c->buffer_size_tx);assert(allocation);live=1;return allocation;}
static void esp_http_client_set_header(void*c,const char*k,const char*v){}
static void esp_http_client_set_post_field(void*c,const char*b,int n){}
static int esp_http_client_perform(void*c){assert(live);return perform_error;}
static int esp_http_client_get_status_code(void*c){return 200;}
static void esp_http_client_cleanup(void*c){assert(live&&c==allocation);free(allocation);allocation=NULL;live=0;cleanups++;}
'''+function('kosync_request',client)+r'''
int main(void){char msg[96],response[64];int status;
 for(int owned=0;owned<2;++owned)for(int failure=0;failure<3;++failure){
  own=owned;fail_init=failure==1;perform_error=failure==2?9:0;int previous=cleanups;
  int err=kosync_request(0,"/sync",NULL,&status,response,sizeof(response),msg,sizeof(msg));
  assert(err==(fail_init?ESP_ERR_NO_MEM:perform_error));assert(!live&&!allocation);
  assert(cleanups==previous+!fail_init);
 }
 assert(releases==6);puts("PASS: actual HTTP request frees its buffers before releasing owned network on success/perform error/init OOM; existing WiFi ownership preserved");
}
'''
s=book.read_text();call=function('app_book_request_kosync',book);assert call.index('reader_show_sync_notice(ctx);')<call.index('reader_prepare_network();')<call.index('kosync_sync_book(')
control=s[s.index('        if(control==8){'):];assert control.index('reader_show_sync_notice(ctx);')<control.index('reader_prepare_network();')<control.index('kosync_push_book_progress(')
assert 'app_book_request_kosync(ctx);' in function('reader_key_action',book)
with tempfile.TemporaryDirectory() as d:
 p=Path(d)
 for name,unit in [('notice',notice),('http',request)]:
  (p/(name+'.c')).write_text(unit)
  subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-parameter',str(p/(name+'.c')),'-o',str(p/name)],check=True)
  subprocess.run([str(p/name)],check=True)
