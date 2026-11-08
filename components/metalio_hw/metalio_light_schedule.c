/* SPDX-License-Identifier: Apache-2.0
 * Daily channel schedules have no worker, timer, radio or recurring flash writes.
 * Manual light changes last until the next boundary; wake reapplies current phase.
 */
#include "metalio_light_schedule.h"
#include "nvs.h"

static metalio_light_schedule_t schedules[2] = {
    {false, 7*60, 18*60, 30}, {false, 18*60, 23*60, 30}
};
static bool loaded, load_attempted, seen[2];
static esp_err_t load_error;
static int64_t last_edge[2];
#define SCHEDULE_BYTES METALIO_LIGHT_SCHEDULE_BYTES

static bool valid(const metalio_light_schedule_t *s) {
    return s && s->on_minute<1440 && s->off_minute<1440 &&
        s->on_minute!=s->off_minute && s->level>=1 && s->level<=100;
}
bool metalio_light_schedule_decode(const uint8_t bytes[SCHEDULE_BYTES],
                                   metalio_light_schedule_t out[2]) {
    if (bytes[0] != 1) return false;
    for (unsigned i = 0; i < 2; ++i) {
        const uint8_t *p = bytes + 1 + i*7;
        out[i] = (metalio_light_schedule_t){p[0]!=0,p[1]|(p[2]<<8),p[3]|(p[4]<<8),p[5]};
        if (p[0] > 1 || p[6] || !valid(&out[i])) return false;
    }
    return true;
}
void metalio_light_schedule_adopt(const metalio_light_schedule_t restored[2]) {
    for (unsigned i = 0; i < 2; ++i) {
        schedules[i] = restored[i];
        seen[i] = false;
    }
    loaded = true;
    load_error = ESP_OK;
}
static void schedule_encode(uint8_t bytes[SCHEDULE_BYTES], const metalio_light_schedule_t values[2]) {
    bytes[0] = 1;
    for (unsigned i = 0; i < 2; ++i) {
        const metalio_light_schedule_t *s = &values[i];
        uint8_t *p = bytes+1+i*7;
        p[0]=s->enabled;p[1]=s->on_minute&255;p[2]=s->on_minute>>8;
        p[3]=s->off_minute&255;p[4]=s->off_minute>>8;p[5]=s->level;p[6]=0;
    }
}
static esp_err_t schedule_load(bool retry) {
    if(loaded)return ESP_OK;
    if(load_attempted && !retry)return load_error;
    load_attempted=true;
    nvs_handle_t h;
    load_error=nvs_open("metalio_light",NVS_READONLY,&h);
    if(load_error==ESP_ERR_NVS_NOT_FOUND){loaded=true;return load_error=ESP_OK;}
    if(load_error!=ESP_OK)return load_error;
    uint8_t bytes[SCHEDULE_BYTES];size_t size=sizeof(bytes);
    load_error=nvs_get_blob(h,"schedule",bytes,&size);nvs_close(h);
    if(load_error==ESP_ERR_NVS_NOT_FOUND){loaded=true;return load_error=ESP_OK;}
    if(load_error!=ESP_OK)return load_error;
    metalio_light_schedule_t parsed[2];
    if (size != sizeof(bytes) || !metalio_light_schedule_decode(bytes, parsed))
        return load_error = ESP_ERR_INVALID_STATE;
    metalio_light_schedule_adopt(parsed);
    return ESP_OK;
}
esp_err_t metalio_light_schedule_export(uint8_t out[SCHEDULE_BYTES]) {
    esp_err_t err = schedule_load(true);
    if (err == ESP_OK) schedule_encode(out, schedules);
    return err;
}
metalio_light_schedule_t metalio_light_schedule_get(bool warm) {
    (void)schedule_load(true);return schedules[warm?1:0];
}
esp_err_t metalio_light_schedule_save(bool warm,const metalio_light_schedule_t *schedule) {
    if(!valid(schedule))return ESP_ERR_INVALID_ARG;
    esp_err_t read_error=schedule_load(true);
    if(read_error!=ESP_OK)return read_error;
    unsigned channel=warm?1:0;
    metalio_light_schedule_t next[2] = {schedules[0], schedules[1]};
    next[channel] = *schedule;
    uint8_t bytes[SCHEDULE_BYTES];
    schedule_encode(bytes, next);
    nvs_handle_t h;esp_err_t err=nvs_open("metalio_light",NVS_READWRITE,&h);
    if(err!=ESP_OK)return err;
    err=nvs_set_blob(h,"schedule",bytes,sizeof(bytes));
    if(err==ESP_OK)err=nvs_commit(h);
    nvs_close(h);
    if(err==ESP_OK){schedules[channel]=*schedule;seen[channel]=false;}
    return err;
}
void metalio_light_schedule_invalidate(void) {seen[0]=seen[1]=false;}
bool metalio_light_schedule_apply(time_t utc,unsigned *white,unsigned *warm) {
    if(schedule_load(false)!=ESP_OK)return false;
    if(utc<(time_t)1704067200LL || utc>(time_t)4102444799LL) {
        metalio_light_schedule_invalidate();return false;
    }
    int64_t minute=((int64_t)utc+7*3600)/60;
    unsigned today=(unsigned)(minute%1440);bool changed=false;
    unsigned desired[2]={*white,*warm};
    for(unsigned i=0;i<2;i++) {
        const metalio_light_schedule_t *s=&schedules[i];
        if(!s->enabled){seen[i]=false;continue;}
        bool on=s->on_minute<s->off_minute ? today>=s->on_minute&&today<s->off_minute :
            today>=s->on_minute||today<s->off_minute;
        unsigned boundary=on?s->on_minute:s->off_minute;
        int64_t edge=minute-(today+1440-boundary)%1440;
        desired[i]=on?(s->level*240+50)/100:0;
        if(!seen[i] || edge!=last_edge[i]) {
            last_edge[i]=edge;seen[i]=true;changed=true;
        }
    }
    // Recompute both scheduled channels at an edge: a prior shared power clamp
    // must not reduce the remaining channel after the other turns off.
    if(changed){*white=desired[0];*warm=desired[1];}
    return changed;
}
