/* SPDX-License-Identifier: Apache-2.0
 * 中文：BQ27220、PCF8563 和电源键接入 RC90，不访问 CW32 寄存器。
 * English: BQ27220, PCF8563 and the power key adapt RC90 without CW32 access.
 */
#include "read_pico_pmu.h"
#include "metalio_hw.h"
#include "esp_system.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <time.h>
#include <string.h>
static pmu_snapshot_t state;
static bool ready,held,long_sent,candidate;
static bool keys_paused;
static uint32_t key_epoch;
static int64_t candidate_since,down_since,battery_at;
static bool charger_initialized;
static int64_t charger_retry_at;
static read_pico_pmu_key_action_t action;
static TaskHandle_t key_task;
static portMUX_TYPE key_lock=portMUX_INITIALIZER_UNLOCKED;
static unsigned unbcd(unsigned v) { return (v&15)>9 || (v>>4)>9 ? 255 : (v>>4)*10+(v&15); }
static unsigned bcd(unsigned v) { return (v/10<<4)|(v%10); }
static esp_err_t rtc_read(void) {
    uint8_t regs[9]; esp_err_t err=metalio_read(0x51,0,regs,sizeof(regs)); state.time_ok=false; state.time_synced=0;
    const uint8_t *r=regs+2;
    if (err!=ESP_OK) return err;
    if ((regs[0]&0x20) || (r[0]&0x80)) return ESP_ERR_INVALID_STATE;
    struct tm t={.tm_sec=unbcd(r[0]&0x7f),.tm_min=unbcd(r[1]&0x7f),.tm_hour=unbcd(r[2]&0x3f),
        .tm_mday=unbcd(r[3]&0x3f),.tm_mon=unbcd(r[5]&0x1f)-1,.tm_year=100+unbcd(r[6]),.tm_isdst=0};
    if (t.tm_sec>59 || t.tm_min>59 || t.tm_hour>23 || t.tm_mday<1 || t.tm_mday>31 || t.tm_mon<0 || t.tm_mon>11) return ESP_ERR_INVALID_RESPONSE;
    if (t.tm_year>199) return ESP_ERR_INVALID_RESPONSE;
    const struct tm requested=t;
    time_t sec=timegm(&t); if (sec<946684800 || sec>4102444799LL) return ESP_ERR_INVALID_STATE;
    // timegm会归一化2月31日，RTC坏日期必须拒绝。/ Reject calendar normalization of invalid RTC dates.
    if (t.tm_mday!=requested.tm_mday || t.tm_mon!=requested.tm_mon || t.tm_year!=requested.tm_year)
        return ESP_ERR_INVALID_RESPONSE;
    state.unix_sec=(uint32_t)sec; state.time_synced=1; state.time_ok=true; return ESP_OK;
}
static esp_err_t rtc_write(uint32_t sec) {
    time_t seconds=sec; struct tm t; gmtime_r(&seconds,&t);
    if (t.tm_year<100 || t.tm_year>199) return ESP_ERR_INVALID_ARG;
    uint8_t bytes[]={bcd(t.tm_sec),bcd(t.tm_min),bcd(t.tm_hour),bcd(t.tm_mday),t.tm_wday,bcd(t.tm_mon+1),bcd(t.tm_year-100)};
    uint8_t ctrl;
    esp_err_t err=metalio_read(0x51,0,&ctrl,1); if(err!=ESP_OK) return err;
    // 写时暂时停表，完成或失败都恢复振荡器；保留其他控制位。
    // Stop briefly for the write, then restore the oscillator on success or failure, preserving other control bits.
    err=metalio_write8(0x51,0,ctrl|0x20); if(err!=ESP_OK) return err;
    esp_err_t written=metalio_write(0x51,2,bytes,sizeof(bytes));
    esp_err_t resumed=metalio_write8(0x51,0,ctrl&~0x20);
    err=written!=ESP_OK?written:resumed;
    if(err==ESP_OK) err=rtc_read();
    ESP_LOGI("metalio_rtc","sync utc=%lu oscillator=%s result=%s",(unsigned long)sec,
        resumed==ESP_OK?"running":"unknown",esp_err_to_name(err));
    return err;
}
static void keys(void) {
    portENTER_CRITICAL(&key_lock);
    bool paused=keys_paused; uint32_t epoch=key_epoch;
    portEXIT_CRITICAL(&key_lock);
    if (paused) return;
    uint16_t inputs;
    if (metalio_io_read(&inputs)!=ESP_OK) return;
    int64_t now=esp_timer_get_time()/1000; bool down=!(inputs&(1u<<15));
    portENTER_CRITICAL(&key_lock);
    if (keys_paused || epoch!=key_epoch) { portEXIT_CRITICAL(&key_lock); return; }
    if (down!=candidate) { candidate=down; candidate_since=now; }
    if (candidate!=held && now-candidate_since>=25) {
        held=candidate;
        if (held) { down_since=now; long_sent=false; }
        else if (!long_sent && now-down_since>=25) action=READ_PICO_PMU_KEY_SHORT;
    }
    if (held && !long_sent && now-down_since>=700) { action=READ_PICO_PMU_KEY_LONG; long_sent=true; }
    state.key_state=held?1:0;
    portEXIT_CRITICAL(&key_lock);
    metalio_volume_sample(inputs,now);
}
esp_err_t read_pico_pmu_keys_pause(void) {
    portENTER_CRITICAL(&key_lock);
    bool can_pause=ready && !held && action==READ_PICO_PMU_KEY_NONE;
    if (can_pause) { keys_paused=true; ++key_epoch; }
    portEXIT_CRITICAL(&key_lock);
    return can_pause?ESP_OK:ESP_ERR_INVALID_STATE;
}
void read_pico_pmu_keys_resume(void) {
    // 按原厂等待输入稳定；丢弃改方向造成的假边沿，不丢真实按键队列。
    // Follow the vendor input-settle delay; discard direction-change edges, retaining real queued keys.
    vTaskDelay(pdMS_TO_TICKS(20));
    uint16_t inputs=0xffff; bool valid=metalio_io_read(&inputs)==ESP_OK;
    int64_t now=esp_timer_get_time()/1000;
    portENTER_CRITICAL(&key_lock);
    held=candidate=valid && !(inputs&(1u<<15));
    down_since=candidate_since=now; long_sent=false;
    state.key_state=held?1:0;
    ++key_epoch; keys_paused=false;
    portEXIT_CRITICAL(&key_lock);
}
static void key_feed(void *arg) {
    (void)arg;
    for (;;) { keys(); vTaskDelay(pdMS_TO_TICKS(10)); }
}
static esp_err_t charger_update(uint8_t reg,uint8_t mask,uint8_t value) {
    uint8_t previous;
    esp_err_t err=metalio_read(0x6b,reg,&previous,1);
    if (err!=ESP_OK) return err;
    const uint8_t next=(previous&~mask)|(value&mask);
    if (next!=previous) err=metalio_write8(0x6b,reg,next);
    uint8_t verified;
    if (err==ESP_OK) err=metalio_read(0x6b,reg,&verified,1);
    if (err==ESP_OK && (verified&mask)!=(next&mask)) err=ESP_ERR_INVALID_RESPONSE;
    return err;
}
static esp_err_t charger_init(void) {
    uint8_t part;
    esp_err_t err=metalio_read(0x6b,0x38,&part,1);
    // 原厂cx25601n hw_init_defaults/enable_charge/enable_otg的公开控制位。
    // Public control bits from reference cx25601n hw_init_defaults/enable_charge/enable_otg.
    // 保留既有电压、电流、终止阈值与私有寄存器，只解除OTG/HIZ并开启无看门狗充电。
    // Preserve voltage/current/termination/private registers; exit OTG/HIZ and enable charging without its watchdog.
    if (err==ESP_OK) err=charger_update(0x18,0x40,0);
    if (err==ESP_OK) err=charger_update(0x16,0x33,0x20);
    charger_initialized=err==ESP_OK;
    ESP_LOGI("metalio_power","CX25601N init=%s part=%02x",esp_err_to_name(err),err==ESP_OK?part:0);
    return err;
}
static void battery(void) {
    int64_t now=esp_timer_get_time(); if (battery_at && now-battery_at<2000000) return;
    battery_at=now;
    if (!charger_initialized && now>=charger_retry_at) {
        (void)charger_init(); charger_retry_at=now+30000000;
    }
    uint8_t r[2]; state.flags &= ~(PMU_STATUS_BATTERY_VALID|PMU_STATUS_CHARGING_ACTIVE); state.quick_ok=false;
    if (metalio_read(0x55,0x08,r,2)==ESP_OK) {
        unsigned mv=r[0]|r[1]<<8;
        if (mv>=2500 && mv<=5000) {
            state.battery_mv=mv; state.flags|=PMU_STATUS_BATTERY_VALID; unsigned soc;
            if (metalio_read(0x55,0x2c,r,2)==ESP_OK && (soc=(unsigned)(r[0]|r[1]<<8))<=100) state.soc_permille=soc*10;
            else state.flags &= ~PMU_STATUS_BATTERY_VALID;
        }
    }
    uint8_t charger=0,control=0,otg=0; state.charge_state=PMU_CHARGE_UNKNOWN;
    const bool charger_ok=metalio_read(0x6b,0x1e,&charger,1)==ESP_OK &&
        metalio_read(0x6b,0x16,&control,1)==ESP_OK && metalio_read(0x6b,0x18,&otg,1)==ESP_OK;
    if (charger_ok) {
        // CX25601N三个非零阶段均含Top-off；同时核对EN_CHG和EN_OTG，遵循S31参考。
        // All nonzero CX25601N stages charge, including top-off; also check enable/OTG as S31 reference does.
        const bool charging=((charger>>3)&3)!=0 && (control&0x20)!=0 && (otg&0x40)==0;
        state.charge_state=charging?PMU_CHARGE_CHARGING:PMU_CHARGE_NOT_CHARGING;
        if (charging) state.flags|=PMU_STATUS_CHARGING_ACTIVE;
    }
    state.status_ok=(state.flags&PMU_STATUS_BATTERY_VALID)!=0 || charger_ok;
    static int previous=-1;
    const int status=charger_ok?charger|(control<<8)|(otg<<16):-1;
    if (status!=previous) {
        previous=status;
        ESP_LOGI("metalio_power","battery valid=%u mv=%u soc=%u charger=%s status=%02x control=%02x otg=%02x",
            (unsigned)((state.flags&PMU_STATUS_BATTERY_VALID)!=0),state.battery_mv,state.soc_permille,
            charger_ok?"online":"unavailable",charger,control,otg);
    }
}
esp_err_t read_pico_pmu_init(i2c_master_bus_handle_t bus,const read_pico_pmu_config_t *cfg) {
    (void)bus;(void)cfg; ready=metalio_ioe()!=NULL; state.present=ready;
    if (ready) { held=candidate=metalio_key_down(); down_since=candidate_since=esp_timer_get_time()/1000; battery(); (void)rtc_read(); }
    if (ready && !key_task && xTaskCreate(key_feed,"metalio_keys",2048,NULL,5,&key_task)!=pdPASS) ready=false;
    return ready?ESP_OK:ESP_ERR_INVALID_STATE;
}
esp_err_t read_pico_pmu_deinit(void) { if(key_task) {vTaskDelete(key_task);key_task=NULL;} ready=false; return ESP_OK; }
bool read_pico_pmu_ready(void) { return ready; }
const pmu_snapshot_t *read_pico_pmu_get(void) { return &state; }
esp_err_t read_pico_pmu_poll(void) { if (!ready) return ESP_ERR_INVALID_STATE; battery(); return ESP_OK; }
esp_err_t read_pico_pmu_refresh(void) { esp_err_t e=read_pico_pmu_poll(); (void)rtc_read(); return e; }
esp_err_t read_pico_pmu_cmd(uint16_t code,const uint8_t *p,uint8_t n) {
    if (code==PMU_CMD_TIME_GET) return rtc_read();
    if (code==PMU_CMD_TIME_SYNC && p && n==4) return rtc_write((uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24);
    if (code==PMU_CMD_HOST_READY) return ESP_OK;
    return ESP_ERR_NOT_SUPPORTED;
}
esp_err_t read_pico_pmu_vcom_get(int *mv) { if (!mv) return ESP_ERR_INVALID_ARG; *mv=1200; return ESP_OK; }
esp_err_t read_pico_pmu_vcom_set(int mv) { (void)mv; return ESP_ERR_NOT_SUPPORTED; }
esp_err_t read_pico_pmu_uid_get(uint8_t out[PMU_CHIP_UID_LEN]) {
    uint8_t uid[PMU_CHIP_UID_LEN]={'M','E','T','A','L','I'};
    esp_err_t err=esp_efuse_mac_get_default(uid+PMU_CHIP_UID_LEN-6);
    if (err==ESP_OK) { memcpy(state.uid,uid,sizeof(uid)); state.uid_ok=true; if(out)memcpy(out,uid,sizeof(uid)); }
    return err;
}
esp_err_t read_pico_pmu_event_ack(uint16_t id) { (void)id; return ESP_OK; }
esp_err_t read_pico_pmu_action(uint8_t a,uint16_t delay,uint16_t reason) { (void)a;(void)delay;(void)reason; return ESP_ERR_NOT_SUPPORTED; }
void read_pico_pmu_drain_events(void) { metalio_volume_clear(); portENTER_CRITICAL(&key_lock); action=READ_PICO_PMU_KEY_NONE; portEXIT_CRITICAL(&key_lock); }
read_pico_pmu_key_action_t read_pico_pmu_take_key_action(void) {
    portENTER_CRITICAL(&key_lock);
    read_pico_pmu_key_action_t a=action; action=READ_PICO_PMU_KEY_NONE;
    portEXIT_CRITICAL(&key_lock); return a;
}
bool read_pico_pmu_take_key_short(void) { return read_pico_pmu_take_key_action()==READ_PICO_PMU_KEY_SHORT; }
bool read_pico_pmu_take_key_wakeup(void) {
    portENTER_CRITICAL(&key_lock);
    // S31后台已记录去抖短按/长按；持有电平不是新动作，否则锁屏长按会再次唤醒。
    // The S31 sampler queues debounced clicks/holds; a held level is not a new wake action.
    bool wake=action!=READ_PICO_PMU_KEY_NONE; action=READ_PICO_PMU_KEY_NONE;
    portEXIT_CRITICAL(&key_lock); return wake;
}
esp_err_t read_pico_pmu_report_sleep(void) { return ESP_ERR_NOT_SUPPORTED; }
esp_err_t read_pico_pmu_report_ready(void) { return ready?ESP_OK:ESP_ERR_INVALID_STATE; }
esp_err_t read_pico_pmu_power_off(void) { metalio_poweroff(); return ESP_OK; }
const char *pmu_power_name(uint8_t v) { (void)v; return "Metalio"; }
const char *pmu_charge_name(uint8_t v) { return v==PMU_CHARGE_CHARGING?"charging":v==PMU_CHARGE_NOT_CHARGING?"idle":"unknown"; }
const char *pmu_event_name(uint8_t v) { (void)v; return "Metalio"; }
const char *pmu_status_name(uint16_t v) { (void)v; return "Metalio"; }
const char *pmu_led_name(uint8_t v) { (void)v; return "unsupported"; }
const char *pmu_action_name(uint8_t v) { (void)v; return "unsupported"; }
uint16_t pmu_crc16_ccitt_false(const uint8_t *data,uint16_t length) {
    uint16_t crc=0xffff;
    for (unsigned i=0;i<length;i++) { crc^=(uint16_t)data[i]<<8; for (unsigned b=0;b<8;b++) crc=(crc&0x8000)?(crc<<1)^0x1021:crc<<1; }
    return crc;
}
