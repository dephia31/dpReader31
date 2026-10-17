"""Exercise actual HTTPS time retrieval with simulated HTTP/TLS boundaries."""
from pathlib import Path
import re,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
def function(path,name):
    s=(root/path).read_text();m=re.search(r'^(?:static )?[^\n]+\b'+name+r'\([^;{}]*\)\s*\{',s,re.M)
    assert m,name
    at=m.end();depth=1
    while depth:
        depth+=(s[at]=='{')-(s[at]=='}');at+=1
    return s[m.start():at]+'\n'
code=r'''
#define _DEFAULT_SOURCE
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>
typedef int esp_err_t;
enum {ESP_OK,ESP_ERR_INVALID_ARG,ESP_ERR_NO_MEM,ESP_ERR_TIMEOUT,ESP_ERR_INVALID_RESPONSE,ESP_FAIL};
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
enum {HTTP_METHOD_HEAD,HTTP_EVENT_ON_HEADER,HTTP_ADDR_TYPE_INET};
typedef struct {int event_id;char *header_key,*header_value;void *user_data;} esp_http_client_event_t;
typedef struct {const char *url;int method,timeout_ms,addr_type;esp_err_t (*crt_bundle_attach)(void*);bool disable_auto_redirect;int buffer_size,buffer_size_tx;esp_err_t(*event_handler)(esp_http_client_event_t*);void *user_data;} esp_http_client_config_t;
typedef void *esp_http_client_handle_t;
static esp_err_t esp_crt_bundle_attach(void *cfg){(void)cfg;return ESP_OK;}
static esp_http_client_config_t current;
static int attempts,cleanups,oom;
static int results[2],statuses[2];static const char *dates[2],*ages[2];static uint32_t system_utc;
static esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t *c){assert(c->addr_type==HTTP_ADDR_TYPE_INET&&c->method==HTTP_METHOD_HEAD&&c->crt_bundle_attach==esp_crt_bundle_attach&&c->disable_auto_redirect&&c->timeout_ms<=5000&&!strncmp(c->url,"https://",8));current=*c;return oom?NULL:(void*)1;}
static esp_err_t esp_http_client_perform(esp_http_client_handle_t h){assert(h);int i=attempts++;esp_http_client_event_t e={.event_id=HTTP_EVENT_ON_HEADER,.user_data=current.user_data};if(dates[i]){e.header_key="dAtE";e.header_value=(char*)dates[i];current.event_handler(&e);}if(ages[i]){e.header_key="Age";e.header_value=(char*)ages[i];current.event_handler(&e);}return results[i];}
static int esp_http_client_get_status_code(esp_http_client_handle_t h){assert(h);return statuses[attempts-1];}
static void esp_http_client_cleanup(esp_http_client_handle_t h){assert(h);++cleanups;}
static int fake_settimeofday(const struct timeval *tv,const void *tz){(void)tz;assert(tv->tv_usec==0);system_utc=tv->tv_sec;return 0;}
#define settimeofday fake_settimeofday
'''
code+=function('components/read_pico_transfer/transfer_http_time.h','transfer_http_date')
code+=re.sub(r'^#include[^\n]*\n','',(root/'components/read_pico_transfer/transfer_http_time.c').read_text(),flags=re.M)
code+=r'''
static void reset(void){attempts=cleanups=oom=0;system_utc=0;for(int i=0;i<2;i++){results[i]=ESP_OK;statuses[i]=200;dates[i]="Sat, 10 Oct 2026 12:49:49 GMT";ages[i]=NULL;}}
int main(void){
 uint32_t utc=0,expected;assert(transfer_http_date("Sat, 10 Oct 2026 12:49:49 GMT",&expected));
 const char *bad[]={"Mon, 10 Oct 2026 12:49:49 GMT","Sat, 31 Feb 2026 12:49:49 GMT","Sat, 10 Oct 2026 25:49:49 GMT","Sat, 10 Oct 2026 12:49:49 UTC","Sat, 10 Foo 2026 12:49:49 GMT","Sat, 10 Oct 2026 12:49:49 GMTjunk","Sun, 01 Jan 2023 00:00:00 GMT"};
 for(unsigned i=0;i<sizeof(bad)/sizeof(*bad);i++)assert(!transfer_http_date(bad[i],&utc));
 assert(transfer_http_date("Tue, 29 Feb 2028 12:49:49 GMT",&utc));
 reset();assert(transfer_sync_time_https(&utc)==ESP_OK&&attempts==1&&cleanups==1&&utc==expected&&system_utc==utc);
 reset();results[0]=ESP_ERR_TIMEOUT;statuses[1]=501;ages[1]="17";
 assert(transfer_sync_time_https(&utc)==ESP_OK&&attempts==2&&cleanups==2&&utc==expected+17&&system_utc==utc);
 reset();ages[0]="301";dates[1]=NULL;
 assert(transfer_sync_time_https(&utc)==ESP_ERR_INVALID_RESPONSE&&attempts==2&&cleanups==2&&!system_utc);
 reset();results[0]=results[1]=ESP_FAIL;
 assert(transfer_sync_time_https(&utc)==ESP_FAIL&&cleanups==2&&!system_utc); // Never use headers from a failed TLS/HTTP exchange.
 reset();oom=1;assert(transfer_sync_time_https(&utc)==ESP_ERR_NO_MEM&&!attempts&&!cleanups);
 assert(transfer_sync_time_https(NULL)==ESP_ERR_INVALID_ARG);
 puts("PASS: actual HTTPS IPv4/certificate/HEAD configuration, UTC calendar/leap day, fallback incl. HEAD 501, cache Age, invalid/no Date, TLS failure cleanup, system clock update and OOM");
}
'''
with tempfile.TemporaryDirectory() as tmp:
    src=Path(tmp)/'test.c';src.write_text(code);exe=Path(tmp)/'test'
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-g','-fsanitize=address,undefined',str(src),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
