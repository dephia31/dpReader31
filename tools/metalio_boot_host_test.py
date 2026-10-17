"""Fault-inject the actual S31 host-triggered download session and cancellation."""
from pathlib import Path
import subprocess,tempfile
from home_cover_cache_host_test import function
root=Path(__file__).resolve().parents[1]
unit=r'''#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#define ESP_OK 0
#define ESP_ERR_NOT_FINISHED -1
#define ESP_ERR_INVALID_STATE -9
typedef int esp_err_t;
static bool book_cover_jobs_quiesce(unsigned ms){assert(ms==2000);return true;}
static bool ui_avatar_quiesce(unsigned ms){assert(ms==2000);return true;}
static bool s_download_active;
static bool transfer_busy,radio_paused,route_retained;
static bool usb_active;
static bool usb_storage_active(void){return usb_active;}
static int ble_error,route_error,request_error,requests,wifi_restores,ble_restores,time_saves;
static bool read_pico_transfer_pause_for_sleep(void){return !transfer_busy;}
static void read_pico_transfer_resume_after_sleep(void){++wifi_restores;}
static int ble_pt_suspend_for_sleep(void){radio_paused=!ble_error;return ble_error;}
static void ble_pt_resume_after_sleep(void){radio_paused=false;++ble_restores;}
static int metalio_download_prepare(void){assert(radio_paused);route_retained=!route_error;return route_error;}
static void metalio_download_cancel(void){assert(route_retained);route_retained=false;}
static int app_sync_time_to_pmu(void){assert(route_retained);++time_saves;return 0;}
static int metalio_request_download(void){assert(route_retained&&radio_paused);++requests;return request_error;}
static void download_trace(const char *stage,bool first){assert(stage&&radio_paused);assert(first||route_retained);}
// These operations would make the cancellable UI unsafe; fail the test if production calls them.
#define app_font_begin_lock(...) _Static_assert(0,"must retain fonts")
#define ttf_font_open_builtin(...) _Static_assert(0,"must retain fonts")
#define read_pico_sd_sync(...) _Static_assert(0,"must retain SD/CNNT")
#define epd_lcd_deinit(...) _Static_assert(0,"must retain display")
'''
for name in ['app_download_active','app_cancel_download','app_enter_download']:
 unit+=function(name,root/'main/sleep.c')+'\n'
unit+=r'''int main(void){
 app_cancel_download();assert(!wifi_restores&&!ble_restores&&!app_download_active());
 usb_active=true;assert(app_enter_download()==ESP_ERR_INVALID_STATE&&!radio_paused&&!route_retained&&!requests);usb_active=false;
 transfer_busy=true;assert(app_enter_download()==ESP_ERR_NOT_FINISHED&&!radio_paused&&!route_retained&&!requests);
 transfer_busy=false;ble_error=-2;assert(app_enter_download()==-2&&wifi_restores==1&&!route_retained&&!requests);
 ble_error=0;route_error=-3;assert(app_enter_download()==-3&&!radio_paused&&ble_restores==1&&wifi_restores==2&&!requests);
 route_error=0;request_error=-4;assert(app_enter_download()==-4&&!radio_paused&&!route_retained&&ble_restores==2&&wifi_restores==3&&!app_download_active());
 request_error=0;assert(app_enter_download()==ESP_OK&&app_download_active()&&route_retained&&radio_paused);
 assert(app_enter_download()==ESP_OK&&app_download_active()&&time_saves==2); // retry cannot re-pause/re-save
 app_cancel_download();assert(!app_download_active()&&!route_retained&&!radio_paused&&wifi_restores==4&&ble_restores==3);
 app_cancel_download();assert(wifi_restores==4&&ble_restores==3);
 assert(app_enter_download()==ESP_OK&&app_download_active());app_cancel_download();
 assert(app_enter_download()==ESP_OK&&app_download_active());
 int wifi_before=wifi_restores,ble_before=ble_restores;
 request_error=-8;assert(app_enter_download()==-8&&!app_download_active()&&!route_retained&&!radio_paused);
 assert(wifi_restores==wifi_before+1&&ble_restores==ble_before+1);
 puts("PASS: real download session returns with UI/fonts/TF alive; transfer/BLE/SEL/request failures roll back, repeat preparation is idempotent, retry and cancel restore exact radio ownership without software reset");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(unit)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
