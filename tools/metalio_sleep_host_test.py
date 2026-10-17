"""Run the actual retained S31 lock wait; model scheduler progress and long idle."""
from pathlib import Path
import subprocess,tempfile
from home_cover_cache_host_test import function
root=Path(__file__).resolve().parents[1]
unit=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
typedef int app_wake_source_t;
enum {APP_WAKE_NONE,APP_WAKE_KEY,APP_WAKE_PICKUP,APP_WAKE_TIMEOUT};
typedef void *sc7a20h_handle_t;
typedef struct {int x_mg,y_mg,z_mg;unsigned status;} sc7a20h_sample_t;
#define ESP_OK 0
#define ESP_LOGW(...) ((void)0)
#define APP_SLEEP_DEEP 1
static void *s_lock_sleep_touch;
static bool s_lock_deep_ready,s_lock_frame_cached;
static bool pin;
static unsigned cached_marks;
static int cached_error;
static bool lock_pin_enabled(void){return pin;}
static int pico_boot_mark_frame_cached(void){++cached_marks;return cached_error;}
static unsigned lp_mode,light_calls,deep_calls;
static bool light_key,light_fail;
static unsigned app_settings_lock_sleep(void){return lp_mode;}
static int lock_light_sleep_once(void*a,void*t,bool*k){(void)a;(void)t;light_calls++;*k=light_key;return light_fail?-1:0;}
static int book_suspend_io(void){return 0;}
static int book_resume_io(void){return 0;}
static int app_enter_host_sleep(int m){(void)m;deep_calls++;return -1;}
typedef int esp_err_t;
static bool book_cover_jobs_quiesce(unsigned ms){assert(ms==2000);return true;}
static bool ui_avatar_quiesce(unsigned ms){assert(ms==2000);return true;}
#define pdMS_TO_TICKS(ms) (ms)
#define ESP_LOGI(...) ((void)0)
static int64_t clock_us,key_at,plug_at=-1;
static int frontlight_calls,motion_reads,sensor_on,sensor_off,reconnects,scheduler_yields;
static unsigned last_wake;
static bool pickup,usb,powered,stale_motion;
static int64_t esp_timer_get_time(void){return clock_us;}
static bool app_settings_pickup_wake(void){return pickup;}
static int sc7a20h_power_up(void *a){assert(a);sensor_on++;return ESP_OK;}
static int sc7a20h_shared_irq_idle(void *a){assert(a);sensor_off++;return ESP_OK;}
static int sc7a20h_read(void *a,sc7a20h_sample_t *s){assert(a);s->x_mg=motion_reads++?1000:0;s->y_mg=s->z_mg=0;s->status=stale_motion?0:8;return ESP_OK;}
static void metalio_frontlight(unsigned b,unsigned w){assert(!b&&w==50);frontlight_calls++;}
static void metalio_frontlight_restore(void){frontlight_calls++;}
static bool read_pico_pmu_take_key_wakeup(void){return key_at>=0&&clock_us>=key_at;}
static bool metalio_usb_blocks_sleep(void){return usb||powered||(plug_at>=0&&clock_us>=plug_at);}
static void metalio_usb_serial_reconnect(void){++reconnects;}
static void read_pico_clear_ioe_int(void){}
static void vTaskDelay(int ms){assert(ms==30);scheduler_yields++;clock_us+=(int64_t)ms*1000;}
static void app_settings_set_last_wake(unsigned source){last_wake=source;}
'''
unit+=function('app_light_sleep_wait_timed',root/'main/sleep.c')
unit+=r'''
static void reset(void){clock_us=0;key_at=plug_at=-1;frontlight_calls=motion_reads=sensor_on=sensor_off=reconnects=scheduler_yields=0;pickup=usb=powered=stale_motion=false;lp_mode=light_calls=deep_calls=cached_marks=0;cached_error=0;pin=s_lock_frame_cached=false;light_key=light_fail=s_lock_deep_ready=false;s_lock_sleep_touch=NULL;}
static void settled(void){assert(frontlight_calls==(last_wake==APP_WAKE_NONE?1:2));}
int main(void){
 reset();assert(app_light_sleep_wait_timed(NULL,1000)==APP_WAKE_TIMEOUT&&last_wake==APP_WAKE_NONE&&scheduler_yields==34);settled();
 reset();key_at=500000;assert(app_light_sleep_wait_timed(NULL,1000)==APP_WAKE_KEY&&clock_us<540000&&!reconnects);settled();
 reset();pickup=true;assert(app_light_sleep_wait_timed((void*)1,1000)==APP_WAKE_PICKUP&&sensor_on==1&&sensor_off==1&&motion_reads==2&&clock_us>=250000&&clock_us<300000);settled();
 reset();pickup=stale_motion=true;assert(app_light_sleep_wait_timed((void*)1,1000)==APP_WAKE_TIMEOUT&&sensor_on==1&&sensor_off==1&&motion_reads==4);settled();
 reset();assert(app_light_sleep_wait_timed((void*)1,1000)==APP_WAKE_TIMEOUT&&!sensor_on&&sensor_off==1);settled();
 reset();usb=true;key_at=500000;assert(app_light_sleep_wait_timed(NULL,100)==APP_WAKE_KEY&&clock_us>=500000);settled();
 reset();powered=true;key_at=500000;assert(app_light_sleep_wait_timed(NULL,100)==APP_WAKE_KEY&&!reconnects);settled();
 reset();plug_at=500000;key_at=1000000;assert(app_light_sleep_wait_timed(NULL,600)==APP_WAKE_KEY&&reconnects==1);settled();
 // Forty-five minutes and three hours: no deadline, only physical input returns, scheduler always progresses.
 for(unsigned mins=45;mins<=180;mins+=135){reset();key_at=(int64_t)mins*60*1000000;assert(app_light_sleep_wait_timed(NULL,0)==APP_WAKE_KEY&&clock_us>=key_at&&scheduler_yields==(int)(key_at/30000)&&!reconnects);settled();}
 reset();lp_mode=1;s_lock_sleep_touch=(void*)1;light_key=true;assert(app_light_sleep_wait_timed(NULL,0)==APP_WAKE_KEY&&light_calls==1&&!deep_calls);settled();
 reset();lp_mode=1;s_lock_sleep_touch=(void*)1;light_fail=true;key_at=500000;assert(app_light_sleep_wait_timed(NULL,0)==APP_WAKE_KEY&&light_calls==1);settled();
 reset();lp_mode=2;s_lock_sleep_touch=(void*)1;key_at=500000;assert(app_light_sleep_wait_timed(NULL,0)==APP_WAKE_KEY&&!deep_calls);settled();
 reset();lp_mode=2;s_lock_sleep_touch=(void*)1;s_lock_deep_ready=true;key_at=500000;assert(app_light_sleep_wait_timed(NULL,0)==APP_WAKE_KEY&&deep_calls==1);settled();
 for(unsigned c=0;c<2;c++)for(unsigned p=0;p<2;p++)for(int e=0;e<2;e++){
 reset();lp_mode=2;s_lock_sleep_touch=(void*)1;s_lock_deep_ready=true;s_lock_frame_cached=c;pin=p;cached_error=e?-1:0;key_at=500000;
 assert(app_light_sleep_wait_timed(NULL,0)==APP_WAKE_KEY&&deep_calls==1&&cached_marks==(c&&!p));settled();
 }
 reset();lp_mode=1;s_lock_sleep_touch=(void*)1;usb=true;key_at=500000;assert(app_light_sleep_wait_timed(NULL,0)==APP_WAKE_KEY&&!light_calls);settled();
 puts("PASS: production S31 retained lock 45min/3h scheduler model, no hardware sleep/TF unmount/CPU reset, bounded short-key response, 4Hz fresh motion, USB powered/hotplug, lamp restores only on real wake, timeout remains dark");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(unit)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-g','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
