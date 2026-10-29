"""中文：KoSync冷启动、内存回收、BLE互斥和连接等待。/ English: KoSync cold start, cache reclamation, BLE exclusion and connection waits."""
from pathlib import Path
import subprocess,tempfile
from home_cover_cache_host_test import function
root=Path(__file__).resolve().parents[1]
book=root/'main/apps/app_book.c';transfer=root/'components/read_pico_transfer/read_pico_transfer.c'
unit=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdatomic.h>
#define ESP_OK 0
#define ESP_ERR_INVALID_STATE 1
#define ESP_ERR_NOT_FOUND 2
#define ESP_ERR_NO_MEM 3
#define ESP_ERR_TIMEOUT 4
#define ESP_ERR_INVALID_ARG 5
#define MALLOC_CAP_INTERNAL 1
#define MALLOC_CAP_8BIT 2
#define MALLOC_CAP_SPIRAM 4
#define KIIKO_BOARD_METALIO47 1
#define READ_PICO_TRANSFER_MODE_AP 0
#define READ_PICO_TRANSFER_MODE_STA 1
#define READ_PICO_TRANSFER_STOPPED 0
#define READ_PICO_TRANSFER_STARTING 1
#define READ_PICO_TRANSFER_UPLOADING 2
#define READ_PICO_TRANSFER_ERROR 3
#define TAG "test"
#define pdMS_TO_TICKS(n) (n)
typedef int esp_err_t;
typedef struct {int state,mode;bool network_ready;}read_pico_transfer_status_t;
static read_pico_transfer_status_t status;
static int preempt_error,preempts,stops,polls,syncs;
static int64_t now,ready_at;
static void read_pico_transfer_get_status(read_pico_transfer_status_t *s){*s=status;}
static int read_pico_transfer_preempt_for_network(void){++preempts;return preempt_error;}
static void read_pico_transfer_stop(void){++stops;status.network_ready=false;}
static int64_t esp_timer_get_time(void){return now;}
static void vTaskDelay(int ms){now+=(int64_t)ms*1000;}
static void read_pico_transfer_service_poll(void){++polls;if(ready_at>=0&&now>=ready_at)status.network_ready=true;}
static int read_pico_transfer_sync_time_online(uint32_t *utc){(void)utc;++syncs;return ESP_OK;}
static const char *esp_err_to_name(int e){return e==ESP_ERR_NO_MEM?"ESP_ERR_NO_MEM":"ERROR";}
static void quiet_log(const char *tag,const char *fmt,...){(void)tag;(void)fmt;}
#define ESP_LOGI(...) quiet_log(__VA_ARGS__)
static int step;
static atomic_bool s_prep_stop;
static void *s_prep_task;
static void vTaskDelete(void *task){assert(task&&step++==2);}
static void lock_draw(void){assert(step++==0);}
static void pause_reader_prep(void){assert(step++==1);}
static void reader_page_cache_clear(void){assert(step++==3);}
static void release_page_images(void){assert(step++==4);}
static size_t ttf_font_cache_clear_all(void){assert(step++==5);return 100;}
static void unlock_draw(void){assert(step++==6);}
static unsigned heap_caps_get_free_size(int caps){(void)caps;return 1000;}
'''+function('kosync_acquire_network',root/'main/book/kosync_client.c')+'\n'+function('kosync_release_network',root/'main/book/kosync_client.c')+'\n'+function('reader_prepare_network',book)+r'''
static void reset(void){status=(read_pico_transfer_status_t){.mode=READ_PICO_TRANSFER_MODE_STA};preempt_error=preempts=stops=polls=syncs=step=0;now=0;ready_at=0;s_prep_task=(void*)1;atomic_store(&s_prep_stop,false);}
int main(void){
 char msg[96];bool owned;
 reset();status.network_ready=true;reader_prepare_network();assert(!step);
 assert(kosync_acquire_network(&owned,msg,sizeof(msg))==ESP_OK&&!owned&&!preempts);kosync_release_network(owned);assert(!stops);
 reset();reader_prepare_network();assert(step==7&&!s_prep_task&&atomic_load(&s_prep_stop));ready_at=15000000;
 assert(kosync_acquire_network(&owned,msg,sizeof(msg))==ESP_OK&&owned&&now>=15000000&&preempts==1&&syncs==1);
 kosync_release_network(owned);assert(stops==1);
 reset();preempt_error=ESP_ERR_NO_MEM;
 assert(kosync_acquire_network(&owned,msg,sizeof(msg))==ESP_ERR_NO_MEM&&!owned&&strstr(msg,"ESP_ERR_NO_MEM"));
 reset();preempt_error=ESP_ERR_NOT_FOUND;
 assert(kosync_acquire_network(&owned,msg,sizeof(msg))==ESP_ERR_NOT_FOUND&&!owned&&strstr(msg,"Chưa cấu hình"));
 reset();preempt_error=ESP_ERR_INVALID_STATE;status.state=READ_PICO_TRANSFER_UPLOADING;
 assert(kosync_acquire_network(&owned,msg,sizeof(msg))==ESP_ERR_INVALID_STATE&&!owned&&strstr(msg,"Đang nhận tệp"));
 reset();ready_at=-1;
 assert(kosync_acquire_network(&owned,msg,sizeof(msg))==ESP_ERR_TIMEOUT&&!owned&&stops==1&&now==35000000);
 puts("PASS: actual KoSync reuses connected WiFi, reclaims reader caches for cold start, accepts >12s connect, propagates OOM/config/upload errors and releases owned radio");
}
'''
# 中文：执行真实S31启动前置段，保证仅联网也先回收BLE。/ English: Run the actual S31 startup preamble; network-only must reclaim BLE first.
start=function('read_pico_transfer_start',transfer);prefix=start[:start.index('    s_cfg = *cfg;')]
radio=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_ERR_INVALID_ARG 1
#define ESP_ERR_INVALID_STATE 2
#define READ_PICO_TRANSFER_MODE_STA 1
#define READ_PICO_TRANSFER_MODE_AP 0
typedef struct {int mode;bool network_only,exclusive_radio;const char *root_dir;void *free_bytes_cb;}read_pico_transfer_cfg_t;
static bool cancel_ok=true,claim_ok=true,locked,s_wifi,s_netif,s_http,s_ble_network_reserved;
char s_root[160];
static int reservations,reserve_error;
static bool cancel_time_sync(bool automatic){assert(automatic);return cancel_ok;}
static bool claim_config(void){if(claim_ok)locked=true;return claim_ok;}
static void release_config(void){locked=false;}
static int ble_pt_network_acquire(void){assert(locked);++reservations;return reserve_error;}
'''+prefix+r'''
 return ESP_OK;
}
int main(void){
 for(int network=0;network<2;++network)for(int exclusive=0;exclusive<2;++exclusive){
  reservations=0;s_ble_network_reserved=false;
  read_pico_transfer_cfg_t cfg={.mode=READ_PICO_TRANSFER_MODE_STA,.network_only=network,.exclusive_radio=exclusive,.root_dir="/sdcard",.free_bytes_cb=(void*)1};
  bool reserved=!network||exclusive;
  assert(read_pico_transfer_start(&cfg)==ESP_OK&&reservations==(int)reserved&&s_ble_network_reserved==reserved&&locked);release_config();
  s_ble_network_reserved=false;reserve_error=9;
  if(reserved)assert(read_pico_transfer_start(&cfg)==9&&!s_ble_network_reserved&&!locked);
  else {assert(read_pico_transfer_start(&cfg)==ESP_OK&&!s_ble_network_reserved);release_config();}
  reserve_error=0;
 }
 puts("PASS: actual transfer startup reserves BLE before allocation for exclusive syncs and uploads, retaining ordinary STA coexistence, unwinding reservation failures");
}
'''
assert 'reader_prepare_network();' in function('app_book_request_kosync',book)
assert 'reader_prepare_network();' in function('reader_chapter_jump_action',book)
assert 'app_book_request_kosync(ctx);' in function('reader_key_action',book)
with tempfile.TemporaryDirectory() as tmp:
 for name,src in [('kosync',unit),('radio',radio)]:
  p=Path(tmp)/(name+'.c');p.write_text(src);binary=p.with_suffix('')
  subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror',str(p),'-o',str(binary)],check=True)
  subprocess.run([str(binary)],check=True)
