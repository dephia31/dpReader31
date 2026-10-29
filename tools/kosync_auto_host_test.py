#!/usr/bin/env python3
"""中文：实际自动同步调度器的队列、任务失败与过期结果回归。/ English: Actual auto-sync queue, task failure and stale-result regression."""
from pathlib import Path
import subprocess, tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'main/book/kosync_client.c').read_text().split('/* ---- 非阻塞自动同步 / Nonblocking automatic sync ---- */')[1]
prefix=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define CPU_STATE_RAM
#define BOOK_STORE_PATH_MAX 288
#define KIIKO_BOARD_METALIO47 1
#define portMUX_INITIALIZER_UNLOCKED 0
#define ESP_OK 0
#define ESP_ERR_INVALID_STATE 1
#define pdPASS 1
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
typedef int esp_err_t;
typedef int portMUX_TYPE;
typedef void *TaskHandle_t;
typedef struct {uint32_t file_size,byte_off;uint16_t chapter;uint8_t px,pct;} book_progress_t;
typedef struct {double percentage;} kosync_progress_t;
static int depth, calls, starts, creates_fail;
static bool enabled=true, ota_busy;
static bool pico_online_update_busy(void){return ota_busy;}
static int64_t now;
static TaskHandle_t task=(void*)1;
static void (*worker)(void*);
static char last_path[256];
#define portENTER_CRITICAL(p) ((void)(p),++depth)
#define portEXIT_CRITICAL(p) ((void)(p),--depth)

#undef strlcpy
#define strlcpy test_strlcpy
static size_t test_strlcpy(char *d,const char *s,size_t n){size_t len=strlen(s);snprintf(d,n,"%s",s);return len;}
static TaskHandle_t xTaskGetCurrentTaskHandle(void){return task;}
static void vTaskDelete(void *p){assert(p==NULL);}
static int64_t esp_timer_get_time(void){return now;}
static bool app_settings_kosync_auto(void){return enabled;}
static const char *app_settings_kosync_user(void){return "test";}
static const char *esp_err_to_name(int e){(void)e;return "test";}
static int xTaskCreate(void (*fn)(void*),const char *name,int stack,void *arg,int priority,void *out){
 (void)name;(void)stack;(void)arg;(void)priority;(void)out;assert(!depth);++starts;
 if(creates_fail)return 0;worker=fn;return pdPASS;
}
static esp_err_t kosync_verify_account_impl(bool r,char *m,size_t c){(void)r;(void)m;(void)c;assert(!depth);++calls;return ESP_OK;}
static esp_err_t kosync_push_book_progress_impl(const char *p,const book_progress_t *v,double r,char *m,size_t c){
 (void)v;(void)r;(void)m;(void)c;assert(!depth);++calls;strlcpy(last_path,p,sizeof(last_path));return ESP_OK;
}
static esp_err_t kosync_pull_book_progress_impl(const char *p,kosync_progress_t *r,uint16_t *ch,uint32_t *off,bool *ex,char *m,size_t c){
 (void)m;(void)c;assert(!depth);++calls;strlcpy(last_path,p,sizeof(last_path));r->percentage=.8;*ch=8;*off=80;*ex=true;return ESP_OK;
}
static esp_err_t kosync_sync_book_impl(const char *p,uint16_t *ch,uint32_t *off,uint8_t *pct,bool *pull,char *m,size_t c){
 (void)ch;(void)off;(void)pct;(void)pull;(void)m;(void)c;assert(!depth);++calls;strlcpy(last_path,p,sizeof(last_path));return ESP_OK;
}
'''
suffix=r'''
static void run(void){assert(worker);task=(void*)2;worker(NULL);task=(void*)1;worker=NULL;assert(!s_sync_active&&!depth);}
int main(void){
 book_progress_t p={.chapter=1,.byte_off=20,.pct=10};
 kosync_auto_submit("old",&p,.1,false);kosync_auto_submit("latest",&p,.1,false);
 kosync_auto_submit("read",&p,.1,true);assert(!calls&&!starts);
 ota_busy=true;kosync_auto_poll();assert(!starts&&!calls);ota_busy=false;
 kosync_auto_poll();assert(starts==1&&kosync_busy()&&!calls);
 assert(kosync_verify_account(false,NULL,0)==ESP_ERR_INVALID_STATE);
 kosync_auto_poll();assert(starts==1);run();assert(calls==1&&!strcmp(last_path,"latest"));
 kosync_auto_poll();run();assert(calls==2&&!strcmp(last_path,"read"));
 kosync_progress_t r={0};uint16_t ch=0;uint32_t off=0;bool exact=false;
 book_progress_t moved=p;moved.byte_off++;
 assert(!kosync_auto_take_pull("read",&moved,&r,&ch,&off,&exact));
 assert(!kosync_auto_take_pull("read",&p,&r,&ch,&off,&exact));
 kosync_auto_submit("read",&p,.1,true);kosync_auto_poll();run();
 assert(kosync_auto_take_pull("read",&p,&r,&ch,&off,&exact)&&ch==8&&off==80&&exact&&r.percentage==.8);
 kosync_auto_submit("closed",&p,.1,true);kosync_auto_submit("closed",&p,.1,false);
 kosync_auto_poll();run();int old=calls;kosync_auto_poll();assert(calls==old&&!worker);
 creates_fail=1;kosync_auto_submit("retry",&p,.1,false);kosync_auto_poll();assert(!s_sync_active);
 int attempts=starts;kosync_auto_poll();assert(starts==attempts);now+=5000001;creates_fail=0;kosync_auto_poll();run();assert(!strcmp(last_path,"retry"));
 enabled=false;kosync_auto_submit("disabled",&p,.1,false);kosync_auto_poll();assert(!worker&&!kosync_busy());
 assert(sync_begin());assert(sync_begin());assert(kosync_busy());sync_end();assert(kosync_busy());sync_end();assert(!kosync_busy());
 puts("PASS: actual automatic sync never networks on submit/poll, coalesces, serializes manual sync, drops moved results, retries task allocation and cancels disabled work");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(prefix+s+suffix)
 subprocess.run(['cc','-std=gnu11','-Wall','-Wextra','-Werror','-Wno-unused-function',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
# Transition paths must not call synchronous network APIs.
book=(root/'main/apps/app_book.c').read_text()
for start,end in [('static bool open_book_impl(', 'static bool open_book('),('static app_redraw_t reader_return(', 'static app_redraw_t select_reading_font_item'),('static void book_on_exit(', 'static void book_on_media_lost')]:
 chunk=book[book.index(start):book.index(end,book.index(start)+len(start))]
 assert 'kosync_push_book_progress(' not in chunk and 'kosync_pull_book_progress(' not in chunk
assert 'if(!open_requested_book(ctx))refresh_cached_progress(ctx);' in book
print('PASS: open, return and exit have no synchronous KoSync requests; direct resume skips shelf scan')
