"""S31 production key sampler/wake consumption and shared-IRQ release barrier."""
from pathlib import Path
import subprocess, tempfile
from home_cover_cache_host_test import function
ROOT=Path(__file__).resolve().parents[1]
unit=(ROOT/'components/read_pico_pmu/include/power_double_click.h').read_text().replace('#pragma once','')+r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#define ESP_OK 0
#define ESP_ERR_INVALID_STATE -2
#define pdMS_TO_TICKS(x) (x)
#define portENTER_CRITICAL(x) ((void)(x))
#define portEXIT_CRITICAL(x) ((void)(x))
enum {READ_PICO_PMU_KEY_NONE,READ_PICO_PMU_KEY_SHORT,READ_PICO_PMU_KEY_LONG};
typedef int read_pico_pmu_key_action_t;
typedef int esp_err_t;
static struct {unsigned key_state;} state;
static bool held,long_sent,candidate,ready=true,keys_paused;
static uint32_t key_epoch;
static int64_t candidate_since,down_since,clock_us;
static int key_lock,action,io_error,poll_error;
static power_double_click_t s_wake_click;
static uint16_t inputs=0xffff;
static unsigned drains;
static int64_t esp_timer_get_time(void){return clock_us;}
static int metalio_io_read(uint16_t *value){*value=inputs;return io_error;}
static void metalio_volume_sample(uint16_t value,int64_t ms){(void)value;(void)ms;}
static void metalio_volume_clear(void){++drains;}
static int read_pico_pmu_poll(void){return poll_error;}
static const typeof(state) *read_pico_pmu_get(void){return &state;}
static void read_pico_clear_ioe_int(void){}
static void vTaskDelay(int ms){clock_us+=(int64_t)ms*1000;}
'''
for name in ('keys','read_pico_pmu_keys_pause','read_pico_pmu_keys_resume','read_pico_pmu_drain_events','read_pico_pmu_take_key_wakeup'):
    unit+=function(name,ROOT/'components/read_pico_pmu/metalio_power_adapter.c')+'\n'
unit+=function('app_lock_wait_key_idle',ROOT/'main/sleep.c')+'\n'
unit+=r'''
static void sample(bool down,int ms){inputs=down?0x7fff:0xffff;clock_us=(int64_t)ms*1000;keys();}
static void reset(void){s_wake_click=(power_double_click_t){0};held=long_sent=candidate=keys_paused=false;key_epoch=0;candidate_since=down_since=clock_us=0;state.key_state=0;action=io_error=poll_error=drains=0;inputs=0xffff;}
int main(void){
 reset();sample(true,0);sample(false,10);sample(false,40);assert(!held&&!read_pico_pmu_take_key_wakeup());
 // Short clicks wake on the debounced release, not a stale held level.
 reset();sample(true,0);sample(true,30);assert(held&&!read_pico_pmu_take_key_wakeup());
 sample(false,80);sample(false,110);assert(!held&&!read_pico_pmu_take_key_wakeup());
 sample(true,180);sample(true,210);sample(false,260);sample(false,290);assert(read_pico_pmu_take_key_wakeup()&&!read_pico_pmu_take_key_wakeup());
 // Consume the lock-initiating long action once; its continuing hold/release cannot wake again.
 reset();sample(true,0);sample(true,30);sample(true,730);assert(!read_pico_pmu_take_key_wakeup());
 sample(true,800);assert(!read_pico_pmu_take_key_wakeup());sample(false,810);sample(false,840);assert(!read_pico_pmu_take_key_wakeup());
 // A later short click can be queued while collage composition occupies the UI thread.
 read_pico_pmu_drain_events();sample(true,900);sample(true,930);sample(false,1000);sample(false,1030);
 sample(true,1100);sample(true,1130);sample(false,1200);sample(false,1230);
 clock_us=4000000;assert(read_pico_pmu_take_key_wakeup()&&!read_pico_pmu_take_key_wakeup());
 // Release barrier must finish without a GPIO2-high condition (shared touch/sensor IRQ).
 reset();app_lock_wait_key_idle(800);assert(clock_us==40000&&drains==3);
 reset();held=true;state.key_state=1;app_lock_wait_key_idle(800);assert(clock_us==800000);
 reset();poll_error=-1;app_lock_wait_key_idle(800);assert(clock_us==800000);
 reset();io_error=-1;sample(true,0);sample(true,1000);assert(!held&&!read_pico_pmu_take_key_wakeup());
 // Masked output latches cannot synthesize a key; resumption starts from the real input level.
 reset();assert(read_pico_pmu_keys_pause()==ESP_OK);sample(true,0);sample(true,1000);assert(!held&&!action);
 inputs=0xffff;read_pico_pmu_keys_resume();assert(!keys_paused&&!held&&!read_pico_pmu_take_key_wakeup());
 sample(true,1100);sample(true,1130);sample(false,1200);sample(false,1230);assert(action==READ_PICO_PMU_KEY_SHORT);
 assert(read_pico_pmu_keys_pause()==ESP_ERR_INVALID_STATE&&action==READ_PICO_PMU_KEY_SHORT&&!keys_paused);
 assert(!read_pico_pmu_take_key_wakeup());sample(true,1300);sample(true,1330);assert(read_pico_pmu_keys_pause()==ESP_ERR_INVALID_STATE);
 puts("PASS: production S31 short/long wake consumption, no held-level duplicate, queued click during collage, shared-IRQ independent release and failed-poll timeout");
}
'''
with tempfile.TemporaryDirectory() as d:
    p=Path(d);(p/'test.c').write_text(unit)
    subprocess.run(['cc','-std=gnu11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
