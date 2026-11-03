/* SPDX-License-Identifier: Apache-2.0 */
#include "transfer_network_activity.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <limits.h>
static portMUX_TYPE s_activity_lock=portMUX_INITIALIZER_UNLOCKED;
static TaskHandle_t s_owner;
static unsigned s_depth;
static uint32_t s_generation;
static bool s_stop_requested;
extern void read_pico_transfer_stop(void);
bool read_pico_transfer_network_begin(void) {
    TaskHandle_t task=xTaskGetCurrentTaskHandle();
    portENTER_CRITICAL(&s_activity_lock);
    bool accepted=task&&(!s_owner||s_owner==task)&&s_depth<UINT_MAX;
    if(accepted){if(!s_depth)++s_generation;s_owner=task;++s_depth;}
    portEXIT_CRITICAL(&s_activity_lock);
    return accepted;
}
void read_pico_transfer_network_end(void) {
    TaskHandle_t task=xTaskGetCurrentTaskHandle();
    portENTER_CRITICAL(&s_activity_lock);
    bool stop=false;
    if(s_owner==task&&s_depth) {
        if(s_depth>1)--s_depth;
        else if(s_stop_requested){s_stop_requested=false;stop=true;}
        else {s_depth=0;s_owner=NULL;++s_generation;}
    }
    portEXIT_CRITICAL(&s_activity_lock);
    if(stop){read_pico_transfer_stop();read_pico_transfer_network_end();}
}
void read_pico_transfer_network_request_stop(void) {
    portENTER_CRITICAL(&s_activity_lock);bool immediate=!s_owner;if(!immediate)s_stop_requested=true;portEXIT_CRITICAL(&s_activity_lock);
    if(immediate)read_pico_transfer_stop();
}
bool read_pico_transfer_network_busy(void) {
    portENTER_CRITICAL(&s_activity_lock);bool busy=s_depth!=0;portEXIT_CRITICAL(&s_activity_lock);
    return busy;
}

bool read_pico_transfer_network_background_busy(void) {
    TaskHandle_t task=xTaskGetCurrentTaskHandle();
    portENTER_CRITICAL(&s_activity_lock);bool busy=s_owner&&s_owner!=task;portEXIT_CRITICAL(&s_activity_lock);
    return busy;
}

uint32_t read_pico_transfer_network_generation(void) {
    portENTER_CRITICAL(&s_activity_lock);uint32_t generation=s_generation;portEXIT_CRITICAL(&s_activity_lock);
    return generation;
}
