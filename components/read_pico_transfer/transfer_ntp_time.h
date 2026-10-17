/* 单次IPv4 NTP请求的协议边界。/ Wire validation for one-shot IPv4 NTP. */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_err.h"

static inline uint32_t transfer_ntp_u32(const uint8_t *p) {
    return (uint32_t)p[0]<<24 | (uint32_t)p[1]<<16 | (uint32_t)p[2]<<8 | p[3];
}
static inline bool transfer_ntp_reply(const uint8_t *packet,size_t size,const uint8_t cookie[8],uint32_t *utc) {
    if(!packet || !cookie || !utc || size<48) return false;
    unsigned version=(packet[0]>>3)&7;
    if((packet[0]>>6)==3 || (packet[0]&7)!=4 || (version!=3 && version!=4) ||
        packet[1]==0 || packet[1]>15 || memcmp(packet+24,cookie,8)) return false;
    uint64_t seconds=transfer_ntp_u32(packet+40);
    if(!seconds && !transfer_ntp_u32(packet+44)) return false;
    // 2036年NTP秒数回卷；本设备仅接受2024..2099，不能把回卷解释为1900年。
    // NTP era rolls in 2036; the device accepts UTC years 2024 through 2099.
    if(seconds<2208988800ULL) seconds+=1ULL<<32;
    seconds-=2208988800ULL;
    if(seconds<1704067200ULL || seconds>4102444799ULL) return false;
    *utc=(uint32_t)seconds;
    return true;
}
esp_err_t transfer_sync_time_ntp(uint32_t *utc_seconds);

esp_err_t transfer_sync_time_ntp_cancelable(uint32_t *utc_seconds, bool (*cancelled)(void));
