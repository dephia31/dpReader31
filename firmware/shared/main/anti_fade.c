/* SPDX-License-Identifier: Apache-2.0
 * 基于Moku 2e1e4e2：缓存设置，静置整间隔后重刷；不增加唤醒任务。
 * Based on Moku 2e1e4e2: cache settings and refresh after a complete idle interval; no wake task.
 */
#include "anti_fade.h"
#include "nvs.h"
#include <stdbool.h>
#include <stdint.h>
static unsigned s_minutes=5;
static bool s_loaded;
static bool s_clock_ready;
static int64_t s_last_refresh_ms;
bool anti_fade_minutes_valid(unsigned minutes){return minutes==0||minutes==1||minutes==3||minutes==5||minutes==10||minutes==30;}
unsigned anti_fade_minutes(void){
    if(s_loaded)return s_minutes;
    s_loaded=true;
    nvs_handle_t h;uint8_t value;
    if(nvs_open("metalio_hw",NVS_READONLY,&h)!=ESP_OK)return s_minutes;
    if(nvs_get_u8(h,"antifade_min",&value)==ESP_OK&&anti_fade_minutes_valid(value))s_minutes=value;
    nvs_close(h);return s_minutes;
}
esp_err_t anti_fade_set_minutes(unsigned minutes){
    if(!anti_fade_minutes_valid(minutes))return ESP_ERR_INVALID_ARG;
    (void)anti_fade_minutes();
    if(minutes==s_minutes)return ESP_OK;
    nvs_handle_t h;
    esp_err_t err=nvs_open("metalio_hw",NVS_READWRITE,&h);
    if(err!=ESP_OK)return err;
    err=nvs_set_u8(h,"antifade_min",(uint8_t)minutes);
    if(err==ESP_OK)err=nvs_commit(h);
    nvs_close(h);
    if(err==ESP_OK){s_minutes=minutes;s_clock_ready=false;}
    return err;
}
bool anti_fade_due(int64_t now_ms,int64_t last_input_ms,bool blocked){
    unsigned minutes=anti_fade_minutes();
    if(!s_clock_ready||now_ms<s_last_refresh_ms||blocked){s_last_refresh_ms=now_ms;s_clock_ready=true;return false;}
    int64_t interval=(int64_t)minutes*60000;
    if(!minutes||now_ms<last_input_ms||now_ms-last_input_ms<interval||now_ms-s_last_refresh_ms<interval)return false;
    s_last_refresh_ms=now_ms;return true;
}

int64_t anti_fade_next_ms(int64_t now_ms,int64_t last_input_ms) {
    unsigned minutes=anti_fade_minutes();
    if(!minutes)return INT64_MAX;
    if(!s_clock_ready || now_ms<s_last_refresh_ms || now_ms<last_input_ms)return now_ms;
    int64_t base=s_last_refresh_ms>last_input_ms?s_last_refresh_ms:last_input_ms;
    return base+(int64_t)minutes*60000;
}
