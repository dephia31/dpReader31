#!/usr/bin/env python3
"""Run real OPDS fallback allocation + HTTP callback with a payload exceeding 32KB."""
from pathlib import Path
import subprocess,tempfile
from home_cover_cache_host_test import function
ROOT=Path(__file__).resolve().parents[1];SOURCE=ROOT/'main/book/opds_client.c'
unit=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "transfer_network_activity.h"
enum{ESP_OK,ESP_FAIL,ESP_ERR_INVALID_STATE,ESP_ERR_INVALID_ARG,ESP_ERR_NO_MEM};
enum{HTTP_EVENT_ON_DATA,HTTP_METHOD_GET};
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
#define DPREADER_DEFAULT_OPDS_URL "https://example.test/opds"
#define TAG "test"
#define ESP_LOGW(...) ((void)0)
typedef int esp_err_t;
typedef struct{unsigned count;char feed_title[96];}opds_feed_t;
typedef struct{char *buf;size_t len,cap;}opds_xml_buf_t;
typedef struct{int event_id;void *user_data;const char *data;int data_len;}esp_http_client_event_t;
typedef struct{const char *url;int method,timeout_ms;void *crt_bundle_attach;int(*event_handler)(esp_http_client_event_t*);void *user_data;int buffer_size,buffer_size_tx,max_redirection_count;}esp_http_client_config_t;
typedef void *esp_http_client_handle_t;
static void *esp_crt_bundle_attach;
static int stops,releases,cleanups;
static bool held;
static void(*s_network_observer)(void*);static void *s_network_context;
bool read_pico_transfer_network_begin(void){assert(!held);held=true;return true;}
void read_pico_transfer_network_end(void){assert(held);held=false;++releases;}
static int opds_acquire_network(bool *owned,char *msg,size_t cap){(void)msg;(void)cap;assert(held);*owned=true;return ESP_OK;}
static void opds_release_network(bool owned){assert(held&&owned);++stops;}
static const char *app_settings_opds_url(void){return DPREADER_DEFAULT_OPDS_URL;}
#define esp_err_to_name(e) "injected"
static unsigned char *buffer;static size_t allocated;
static void *heap_caps_malloc(size_t n,int caps){assert(n==131072&&caps==3);return NULL;}
static void *fallback_malloc(size_t n){allocated=n;buffer=malloc(n+64);assert(buffer);memset(buffer+n,0xa5,64);return buffer;}
static void fallback_free(void *p){assert(p==buffer);for(unsigned i=0;i<64;++i)assert(buffer[allocated+i]==0xa5);free(buffer);buffer=NULL;}
static esp_http_client_config_t config;
static void *esp_http_client_init(const esp_http_client_config_t *c){config=*c;return &config;}
static void esp_http_client_set_header(void *p,const char *k,const char *v){(void)p;(void)k;(void)v;}
static void opds_attach_auth(void *p){(void)p;}
static int esp_http_client_perform(void *p){assert(p==&config);opds_xml_buf_t *rx=config.user_data;
 assert(rx->cap==allocated&&allocated==32768);
 static char payload[40000];memset(payload,'x',sizeof(payload));
 esp_http_client_event_t event={HTTP_EVENT_ON_DATA,rx,payload,sizeof(payload)};
 assert(config.event_handler(&event)==ESP_OK&&rx->len==32767&&rx->buf[32767]==0);
 assert(config.event_handler(&event)==ESP_OK&&rx->len==32767);
 return ESP_FAIL;
}
static int esp_http_client_get_status_code(void *p){(void)p;return 503;}
static void esp_http_client_cleanup(void *p){(void)p;++cleanups;}
static void parse_opds_xml(const char *url,const char *xml,size_t len,opds_feed_t *out){(void)url;(void)xml;(void)len;(void)out;assert(false);}
#define malloc fallback_malloc
#define free fallback_free
'''+function('opds_feed_http_event',SOURCE)+'\n'+function('opds_fetch_feed',SOURCE)+r'''
int main(void){opds_feed_t feed={0};char message[128];
 assert(opds_fetch_feed(NULL,&feed,message,sizeof(message))==ESP_FAIL);
 assert(!held&&!buffer&&stops==1&&releases==1&&cleanups==1);
 puts("PASS actual OPDS 32KB fallback: 40KB events bounded to32767B, guard bytes intact, HTTP/WiFi/activity cleaned after error");
}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp);(p/'test.c').write_text(unit)
 subprocess.run(['cc','-std=gnu11','-Wall','-Wextra','-Werror','-fsanitize=undefined','-I'+str(ROOT/'components/read_pico_transfer/include'),str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
