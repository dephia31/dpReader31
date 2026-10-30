/* SPDX-License-Identifier: Apache-2.0
 * 中文：AI侧键去抖和有界事件队列，不复位、不触碰下载选择/休眠唤醒配置。
 * English: Debounce AI edges into a bounded queue, without resets, USB routing or sleep-wakeup configuration.
 */
#include "metalio_hw.h"
#include "freertos/FreeRTOS.h"
static portMUX_TYPE ai_lock=portMUX_INITIALIZER_UNLOCKED;
static struct {bool candidate,stable,armed,long_sent;int64_t changed,down_at;int events[8];unsigned head,count;} ai;
void metalio_ai_sample(bool down,int64_t now_ms) {
    portENTER_CRITICAL(&ai_lock);
    if(down!=ai.candidate){ai.candidate=down;ai.changed=now_ms;}
    if(now_ms<ai.changed){ai.changed=now_ms;ai.armed=false;}
    int event=METALIO_AI_NONE;
    if(now_ms-ai.changed>=25) {
        if(!down && !ai.stable)ai.armed=true;
        if(ai.stable!=down){
            ai.stable=down;
            if(down){ai.down_at=now_ms;ai.long_sent=false;}
            else {if(ai.armed&&!ai.long_sent)event=METALIO_AI_SHORT;ai.armed=true;}
        }
        if(down&&ai.stable&&ai.armed&&!ai.long_sent&&now_ms-ai.down_at>=500){
            event=METALIO_AI_LONG;ai.long_sent=true;
        }
    }
    if(event && ai.count<8){ai.events[(ai.head+ai.count)%8]=event;++ai.count;}
    portEXIT_CRITICAL(&ai_lock);
}
int metalio_take_ai(void) {
    portENTER_CRITICAL(&ai_lock);
    int event=ai.count?ai.events[ai.head]:METALIO_AI_NONE;
    if(ai.count){ai.head=(ai.head+1)%8;--ai.count;}
    portEXIT_CRITICAL(&ai_lock);return event;
}
void metalio_ai_clear(void) {
    portENTER_CRITICAL(&ai_lock);ai.head=ai.count=0;ai.armed=false;ai.long_sent=true;portEXIT_CRITICAL(&ai_lock);
}

bool metalio_ai_sampling_pending(void) {
    portENTER_CRITICAL(&ai_lock);
    bool pending=ai.candidate||ai.stable;
    portEXIT_CRITICAL(&ai_lock);
    return pending;
}
