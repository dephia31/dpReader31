"""Fault-inject the actual S31 light-sleep resource transaction, not a reimplementation."""
from pathlib import Path
import subprocess,tempfile
from home_cover_cache_host_test import function
root=Path(__file__).resolve().parents[1]
unit=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
typedef int esp_err_t;typedef void *sc7a20h_handle_t;typedef void *cst836u_handle_t;
typedef struct {bool mounted;} read_pico_sd_info_t;
#define ESP_OK 0
#define ESP_ERR_INVALID_STATE -1
#define ESP_ERR_NOT_FINISHED -2
#define ESP_SLEEP_WAKEUP_TIMER 1
#define ESP_SLEEP_WAKEUP_EXT1 2
#define CST836U_MODE_DEEPSLEEP 3
#define ESP_LOGI(...) ((void)0)
#define pdMS_TO_TICKS(x) (x)
static unsigned stage,fail_at;
static int64_t now;
static bool usb,msc,online,unmounted,mounted=true,parked,prepared,held,cut,keys,touch_paused;
static unsigned slept,restored_usj,restore_font,restore_book;
static bool timer_fail,sleep_fail,key_cause,stuck_mount,usb_after_timer,queued_key;
static bool read_pico_pmu_take_key_wakeup(void){return queued_key;}
static int step(void){return ++stage==fail_at?-1:0;}
static bool metalio_usb_blocks_sleep(void){return usb;}
static bool usb_storage_active(void){return msc;}
static bool pico_online_busy(void){return online;}
static int read_pico_sd_get_info(read_pico_sd_info_t *i){i->mounted=mounted;return stuck_mount&&unmounted?-2:0;}
static int book_suspend_io(void){return step();}
static int ttf_font_suspend_io(void){return step();}
static int cst836u_set_mode(void *t,int m){assert(t&&m==3);int e=step();touch_paused=!e;return e;}
static int read_pico_sd_sync(void){assert(touch_paused);int e=step();unmounted=!e;if(!e)mounted=false;return e;}
static void metalio_haptic_stop(void){}
static void metalio_frontlight(unsigned b,unsigned w){assert(b==0&&w==50);}
static void epd_poweroff(void){assert(unmounted);}
static void vTaskDelay(int ms){now+=(int64_t)ms*1000;}
static int64_t esp_timer_get_time(void){return now;}
static void epd_lcd_deinit(void){parked=true;}
static int metalio_prepare_light_sleep_pins(void){assert(parked&&!cut);prepared=true;return step();}
static int sc7a20h_shared_irq_idle(void *a){assert(a);return step();}
static bool lock_prepare_key_wake(void){keys=step()==0;return keys;}
static int esp_sleep_enable_timer_wakeup(uint64_t us){assert(us==60000000&&keys);if(usb_after_timer)usb=true;return timer_fail?-1:step();}
static int metalio_sleep_cut_rails(uint16_t *s){assert(prepared&&keys);*s=0x1420;cut=true;return step();}
static int metalio_hold_for_light_sleep(void){assert(cut&&prepared);held=true;return step();}
static int esp_light_sleep_start(void){assert(held&&cut&&unmounted&&touch_paused&&keys);slept++;now+=60000000;return sleep_fail?-1:0;}
static unsigned esp_sleep_get_wakeup_causes(void){return 1u<<(key_cause?ESP_SLEEP_WAKEUP_EXT1:ESP_SLEEP_WAKEUP_TIMER);}
static void metalio_release_light_sleep_pins(void){assert(prepared);prepared=held=false;}
static int esp_sleep_disable_wakeup_source(int src){assert(src==ESP_SLEEP_WAKEUP_TIMER);return 0;}
static int metalio_sleep_restore_rails(uint16_t s){assert(!held&&s==0x1420);cut=false;return 0;}
static void lock_restore_key_wake(void){assert(!cut&&keys);keys=false;}
static int read_pico_sd_remount(void){assert(!cut&&!keys);if(!stuck_mount){mounted=true;restored_usj++;}return 0;}
static int ttf_font_resume_io(void){assert(!unmounted||mounted||stuck_mount);restore_font++;return 0;}
static int book_resume_io(void){assert(!unmounted||mounted||stuck_mount);restore_book++;return 0;}
static void read_pico_epd_restore_lcd(void){assert(!prepared&&(!unmounted||restored_usj||stuck_mount));parked=false;}
static int cst836u_wake(void *t){assert(t&&!cut&&!keys);touch_paused=false;return 0;}
static void metalio_haptic_resume(void){}
'''
unit+=function('lock_light_sleep_once',root/'main/sleep.c')
unit+=r'''
static void reset(void){stage=fail_at=slept=restored_usj=restore_font=restore_book=0;now=0;usb=msc=online=unmounted=parked=prepared=held=cut=keys=touch_paused=timer_fail=sleep_fail=key_cause=stuck_mount=usb_after_timer=queued_key=false;mounted=true;}
static void settled(void){assert(!parked&&!prepared&&!held&&!cut&&!keys&&!touch_paused);}
int main(void){bool key;
 reset();assert(lock_light_sleep_once((void*)1,(void*)2,&key)==0&&!key&&slept==1&&restored_usj==1&&restore_font==1&&restore_book==1);settled();
 reset();key_cause=true;assert(lock_light_sleep_once((void*)1,(void*)2,&key)!=0&&!key);settled();
 reset();queued_key=true;assert(lock_light_sleep_once((void*)1,(void*)2,&key)!=0&&key&&!slept);settled();
 for(unsigned i=1;i<=10;i++){reset();fail_at=i;assert(lock_light_sleep_once((void*)1,(void*)2,&key)!=0&&!key&&!slept);settled();}
 reset();usb=true;assert(lock_light_sleep_once((void*)1,(void*)2,&key)!=0&&!slept&&!stage);settled();
 reset();usb_after_timer=true;assert(lock_light_sleep_once((void*)1,(void*)2,&key)!=0&&!slept);settled();
 reset();timer_fail=true;assert(lock_light_sleep_once((void*)1,(void*)2,&key)!=0&&!slept);settled();
 reset();sleep_fail=key_cause=true;assert(lock_light_sleep_once((void*)1,(void*)2,&key)!=0&&!key);settled();
 reset();stuck_mount=true;assert(lock_light_sleep_once((void*)1,(void*)2,&key)!=0&&!key&&now>=75000000);settled();
 puts("PASS: actual S31 light transaction; pause IO before unmount, rails/pins/key ordering, remount+USJ before handles/LCD, ten injected admission failures, queued short key, USB/hotplug/timer/sleep/mount-timeout; timers never unlock or light");
}
'''
with tempfile.TemporaryDirectory() as d:
    p=Path(d);(p/'test.c').write_text(unit)
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-g','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
