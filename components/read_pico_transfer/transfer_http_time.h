/* 中文：HTTPS响应日期作为手动对时备用源，严格解析UTC日历。
 * English: HTTPS response dates back up manual time sync; strictly parse the UTC calendar. */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "esp_err.h"

static inline bool transfer_http_date(const char *date,uint32_t *out) {
    if(!date || !out || strlen(date)!=29) return false;
    char weekday[4],month[4],zone[4];
    unsigned day,year,hour,minute,second;int used=0;
    if(sscanf(date,"%3s, %u %3s %u %u:%u:%u %3s%n",weekday,&day,month,&year,
        &hour,&minute,&second,zone,&used)!=8 || used!=29 || strcmp(zone,"GMT")) return false;
    static const char *const months[]={"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
    static const char *const days[]={"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
    unsigned m=0,w=0;
    while(m<12 && strcmp(month,months[m])) ++m;
    while(w<7 && strcmp(weekday,days[w])) ++w;
    if(m==12 || w==7 || year<2024 || year>2099 || day<1 || day>31 || hour>23 || minute>59 || second>59) return false;
    struct tm t={.tm_year=(int)year-1900,.tm_mon=(int)m,.tm_mday=(int)day,
        .tm_hour=(int)hour,.tm_min=(int)minute,.tm_sec=(int)second,.tm_isdst=0};
    time_t utc=timegm(&t);
    // timegm会把非法日期归一化，不能让2月31日变成一次成功对时。
    // timegm normalizes invalid dates; reject them instead of accepting a shifted day.
    if(utc<1704067200 || utc>UINT32_MAX || t.tm_year!=(int)year-1900 || t.tm_mon!=(int)m ||
        t.tm_mday!=(int)day || t.tm_wday!=(int)w) return false;
    *out=(uint32_t)utc;return true;
}
esp_err_t transfer_sync_time_https(uint32_t *utc_seconds);
