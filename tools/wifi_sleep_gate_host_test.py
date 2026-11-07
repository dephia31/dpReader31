from pathlib import Path
import subprocess,tempfile
from home_cover_cache_host_test import function
r=Path(__file__).resolve().parents[1];src=r/'components/read_pico_transfer/read_pico_transfer.c'
u=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "transfer_network_activity.h"
enum{ESP_OK,ESP_FAIL,ESP_ERR_INVALID_STATE,ESP_ERR_WIFI_NOT_INIT,ESP_ERR_WIFI_NOT_STARTED};
enum{READ_PICO_TRANSFER_STOPPED,READ_PICO_TRANSFER_READY,READ_PICO_TRANSFER_UPLOADING,READ_PICO_TRANSFER_ERROR};
enum{WIFI_EVENT,IP_EVENT,ESP_EVENT_ANY_ID,WIFI_EVENT_STA_DISCONNECTED,IP_EVENT_STA_GOT_IP};typedef int esp_err_t;
#define portENTER_CRITICAL(p) ((void)(p))
#define portEXIT_CRITICAL(p) ((void)(p))
#define ESP_LOGW(...) ((void)0)
#define ESP_LOGI(...) ((void)0)
static int s_lock,task=1,owner,depth,admissions;
bool read_pico_transfer_network_begin(void){if(owner&&owner!=task)return false;owner=task;++depth;++admissions;return true;}
void read_pico_transfer_network_end(void){assert(owner==task&&depth);if(!--depth)owner=0;}
void read_pico_transfer_network_request_stop(void){assert(0);}
static bool s_sleep_blocked,s_sleep_resume_requested,s_shutdown_only;
static int64_t s_sleep_resume_retry,now;
typedef struct {const char *root_dir;bool network_only;int mode;}read_pico_transfer_cfg_t;
typedef struct{int state,last_error;unsigned sta_count;bool network_ready;char url[64];}read_pico_transfer_status_t;
static read_pico_transfer_cfg_t s_cfg;static read_pico_transfer_status_t s_status;
static bool s_started,s_wifi,s_loop_owned,s_stopping,s_upload_active,s_ble_network_reserved;
static void *s_http,*s_events,*s_ip_events,*s_netif;static char *s_buffer;static unsigned s_buffer_capacity;static int s_connection;
static int stop_error,deinit_error,http_error,stop_calls,deinit_calls,http_calls,starts,start_error;static bool cancel_ok=true;
static bool cancel_time_sync(bool b){(void)b;return cancel_ok;}
static void read_pico_transfer_get_status(read_pico_transfer_status_t *s){*s=s_status;}
static bool admission_stop(bool *stopping,bool uploading){if(uploading)return false;*stopping=true;return true;}
static void set_error(int e){assert(e);s_status.state=READ_PICO_TRANSFER_ERROR;}
static int httpd_stop(void*p){assert(p);++http_calls;return http_error;}
static int esp_wifi_stop(void){++stop_calls;return stop_error;}
static int esp_wifi_deinit(void){++deinit_calls;return deinit_error;}
static void esp_event_handler_instance_unregister(int a,int b,void*p){(void)a;(void)b;assert(p);}
static void esp_netif_destroy_default_wifi(void*p){assert(p);}
static void esp_event_loop_delete_default(void){}
static void ble_pt_network_release(void){}
static int64_t esp_timer_get_time(void){return now;}

#undef strlcpy
#define strlcpy test_copy
void read_pico_transfer_stop(void);
'''
for n in ['read_pico_transfer_sleep_blocked','read_pico_transfer_stop','read_pico_transfer_try_stop_if_idle','read_pico_transfer_pause_for_sleep','read_pico_transfer_resume_after_sleep','read_pico_transfer_resume_service']:u+=function(n,src)+'\n'
u+=r'''
static void *s_time_events;
static void vEventGroupDelete(void*p){(void)p;}
static void clear_secret(void*p,size_t n){memset(p,0,n);}
static void release_config(void){}
#define pdMS_TO_TICKS(n) (n)
static void vTaskDelay(int n){(void)n;}
static int temp_time_cleanup(void){bool wifi_started=true,wifi_initialized=true,loop_owned=true;void *netif=(void*)1,*ip_handler=NULL,*wifi_handler=NULL;int err=ESP_OK;char wifi[4];
'''
timecode=function('sync_time_impl',src);u+=timecode[timecode.index('    bool retained_driver=false;'):timecode.rfind('}')]+ '}\n'
u+='static int temp_scan_cleanup(void){bool initialized=true,started=true,loop_owned=true;void *scan_netif=(void*)1;int err=ESP_OK;unsigned count=1;char out[4];\n#define READ_PICO_TRANSFER_SCAN_MAX 1\n'
scan=function('read_pico_transfer_scan_wifi',src);a=scan.rindex('    if (initialized) {');code=scan[a:scan.rfind('}')].replace('*count','count').replace('    return err;','    (void)count;return err;');u+=code+'}\n'
u+=r'''
#include <stdatomic.h>
typedef int wifi_ps_type_t;
enum{WIFI_PS_MIN_MODEM,PICO_UPDATE_FAILED,PICO_UPDATE_CANCELLED};
static atomic_bool s_cancel;static bool s_download;static int s_mutex,s_done,checks,downloads;static struct{bool busy;unsigned revision;}ota_status;
static void state(int st,const char*m){(void)st;(void)m;}
static bool online(bool *owned){*owned=false;return true;}
static int esp_wifi_get_ps(int *ps){*ps=0;return ESP_OK;}
static int esp_wifi_set_ps(int ps){(void)ps;return ESP_OK;}
#define WIFI_PS_NONE 2
static bool memory_available(unsigned a,unsigned b,unsigned c){(void)a;(void)b;(void)c;return true;}
static int protected_operation(void){assert(owner==2&&depth>0);task=1;int stops=stop_calls;assert(!read_pico_transfer_pause_for_sleep()&&!s_sleep_blocked&&s_wifi&&stops==stop_calls);read_pico_transfer_resume_service();assert(stops==stop_calls);task=2;return ESP_OK;}
static int check_feed(void){++checks;return protected_operation();}
static int download(void){++downloads;return protected_operation();}
static void xSemaphoreTake(int a,int b){(void)a;(void)b;}
static void xSemaphoreGive(int a){(void)a;}
static void vTaskDelete(void*p){(void)p;}
#define portMAX_DELAY 0
'''
worker=function('worker',r/'boards/metalio47/src/main/ota_online.c').replace('s_status->','ota_status.')
u+=worker+'\n'
sleep=r/'boards/metalio47/src/main/sleep.c'
u+=r'''
enum {APP_SLEEP_OFF,APP_SLEEP_DEEP,ESP_ERR_NOT_FINISHED,ESP_ERR_INVALID_ARG};
typedef int app_sleep_mode_t;
static void *s_lock_sleep_touch;
static bool s_download_active;
static bool pico_online_busy(void){return false;}
static bool book_cover_jobs_quiesce(int n){return n==2000;}
static bool ui_avatar_quiesce(int n){return n==2000;}
static bool metalio_usb_blocks_sleep(void){return false;}
static bool usb_storage_active(void){return false;}
static void download_trace(const char *name,bool first){(void)name;(void)first;}
static int metalio_request_download(void){return ESP_OK;}
static void app_cancel_download(void){assert(0);}
'''
for name in ('app_enter_host_sleep','app_enter_download'):
 code=function(name,sleep)
 u+=code[:code.index('    esp_err_t err', code.index('if (!read_pico_transfer_pause_for_sleep()'))]
 u+='    return ESP_OK;\n}\n'
u+=r'''
static void reset(void){s_sleep_blocked=s_sleep_resume_requested=s_shutdown_only=false;s_started=s_wifi=true;s_status.state=READ_PICO_TRANSFER_READY;s_cfg=(read_pico_transfer_cfg_t){"/books",true,1};s_netif=(void*)1;s_http=s_events=s_ip_events=NULL;s_upload_active=s_stopping=false;now=s_sleep_resume_retry=0;owner=depth=0;task=1;stop_error=deinit_error=http_error=start_error=stop_calls=deinit_calls=http_calls=starts=0;cancel_ok=true;}
int main(void){
 reset();admissions=0;read_pico_transfer_resume_service();assert(!admissions);
 reset();owner=2;depth=1;assert(!read_pico_transfer_pause_for_sleep()&&!stop_calls&&!s_sleep_blocked&&s_wifi); // OTA owns network
 s_sleep_resume_requested=true;read_pico_transfer_resume_service();assert(!stop_calls&&!starts);owner=depth=0;
 reset();s_status.state=READ_PICO_TRANSFER_UPLOADING;s_upload_active=true;assert(!read_pico_transfer_pause_for_sleep()&&!stop_calls&&!s_sleep_blocked);
 reset();assert(read_pico_transfer_pause_for_sleep()&&s_sleep_blocked&&!s_wifi&&stop_calls==1&&deinit_calls==1);
 assert(read_pico_transfer_pause_for_sleep()&&s_sleep_blocked);
 read_pico_transfer_resume_after_sleep();assert(!starts&&s_sleep_blocked);read_pico_transfer_resume_service();assert(!starts&&!s_sleep_blocked&&!s_sleep_resume_requested);read_pico_transfer_resume_service();assert(!starts);
 reset();stop_error=ESP_FAIL;assert(!read_pico_transfer_pause_for_sleep()&&s_started&&s_wifi&&s_netif&&s_sleep_blocked&&!deinit_calls);stop_error=0;assert(read_pico_transfer_pause_for_sleep()&&!s_wifi);
 reset();deinit_error=ESP_FAIL;assert(!read_pico_transfer_pause_for_sleep()&&!s_started&&s_wifi&&s_netif&&s_sleep_blocked);deinit_error=0;assert(read_pico_transfer_pause_for_sleep()&&!s_wifi&&!s_netif);
 reset();s_http=(void*)2;http_error=ESP_FAIL;assert(!read_pico_transfer_pause_for_sleep()&&s_http&&s_wifi&&!stop_calls);http_error=0;assert(read_pico_transfer_pause_for_sleep()&&!s_http&&!s_wifi);
 reset();stop_error=ESP_FAIL;
 assert(app_enter_download()==ESP_ERR_NOT_FINISHED&&s_sleep_blocked&&s_sleep_resume_requested);
 stop_error=ESP_OK;read_pico_transfer_resume_service();assert(!s_sleep_blocked&&!s_wifi);
 reset();stop_error=ESP_FAIL;
 assert(app_enter_host_sleep(APP_SLEEP_DEEP)==ESP_ERR_NOT_FINISHED&&s_sleep_blocked&&s_sleep_resume_requested);
 stop_error=ESP_OK;read_pico_transfer_resume_service();assert(!s_sleep_blocked&&!s_wifi);
 reset();stop_error=ESP_FAIL;s_lock_sleep_touch=(void*)1;
 assert(app_enter_host_sleep(APP_SLEEP_OFF)==ESP_ERR_NOT_FINISHED&&s_sleep_blocked&&!s_sleep_resume_requested);
 s_lock_sleep_touch=NULL;
 reset();assert(read_pico_transfer_pause_for_sleep());read_pico_transfer_resume_after_sleep();read_pico_transfer_stop();read_pico_transfer_resume_service();assert(!starts&&!s_sleep_blocked); // close OPDS before restore cancels ownership
 reset();assert(read_pico_transfer_pause_for_sleep());read_pico_transfer_resume_after_sleep();read_pico_transfer_resume_service();assert(!starts&&!s_sleep_blocked);
 int admitted=admissions;now+=60000000;read_pico_transfer_resume_service();assert(!starts&&admissions==admitted); // no reconnect or repeated background work
 s_wifi=s_started=true;read_pico_transfer_resume_service();assert(s_wifi&&!starts); // explicit new session retained

 reset();task=2;ota_status.busy=true;s_download=false;worker(NULL);assert(checks==1&&!stop_calls&&!ota_status.busy&&s_wifi&&!owner);
 reset();task=2;ota_status.busy=true;s_download=true;worker(NULL);assert(downloads==1&&!stop_calls&&!ota_status.busy&&s_wifi&&!owner);
 reset();s_started=s_wifi=false;s_netif=NULL;deinit_error=ESP_FAIL;assert(temp_time_cleanup()==ESP_FAIL&&s_wifi&&s_netif&&s_shutdown_only&&s_ble_network_reserved);deinit_error=0;assert(read_pico_transfer_pause_for_sleep()&&!s_wifi);
 reset();s_started=s_wifi=false;s_netif=NULL;deinit_error=ESP_FAIL;assert(temp_scan_cleanup()==ESP_FAIL&&s_wifi&&s_netif&&s_shutdown_only&&s_ble_network_reserved);deinit_error=0;assert(read_pico_transfer_pause_for_sleep()&&!s_wifi);
 puts("PASS temporary NTP/scan failed deinit retains driver/netif/BLE lease until confirmed stop, without restoring a temporary session");
 puts("PASS actual OTA check/download workers retain ownership against concurrent Sleep/restore requests");
 puts("PASS WiFi Sleep: OTA owner/upload exclusion, unlock without reconnect, HTTP/stop/deinit failures retry without false OFF, no reconnect polling and newer-session protection");}
'''
assert 'if(pico_online_busy())' in function('enter_lock_and_sleep',sleep)
ota_start=function('start',r/'boards/metalio47/src/main/ota_online.c');assert ota_start.index('s_status->busy = true')<ota_start.index('xTaskCreate(worker')
loop=(r/'main/app/app_loop.c').read_text();assert 'if(radio_ready)read_pico_transfer_resume_service();' in loop
for n in ['read_pico_transfer_start','read_pico_transfer_scan_wifi','sync_time_run','read_pico_transfer_preempt_for_network']:assert 'read_pico_transfer_sleep_blocked()' in function(n,src)
with tempfile.TemporaryDirectory() as t:
 p=Path(t);(p/'test.c').write_text(u);subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined','-I'+str(r/'components/read_pico_transfer/include'),str(p/'test.c'),'-o',str(p/'test')],check=True);subprocess.run([str(p/'test')],check=True)
