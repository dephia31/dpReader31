/* SPDX-License-Identifier: LGPL-3.0-or-later
 * 中文：仅本地构建的S31实机像素oracle和同核C/PIE微基准。
 * English: Local-build-only S31 pixel oracle and same-core C/PIE microbench.
 * 冻结：不扫描屏幕、不修改设置，不在产品构建加入任务或USB命令。
 * Frozen: No display scan/settings changes; no bench task or USB command in products.
 */
#include "pie_pixels.h"
#include "esp_cpu.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>

uint32_t epd_interlace_4bpp_line_VE(const uint8_t*,const uint8_t*,uint8_t*,uint8_t*,int);
void epd_apply_line_mask_VE(uint8_t*,const uint8_t*,int);
enum { B=1312 };
typedef struct {
    uint8_t target[B],previous[B],actual[2*B],expected[2*B],columns[B],columns_ref[B],mask[B];
} buffers_t;
typedef struct { SemaphoreHandle_t done; bool passed; unsigned cases; } bench_request_t;
static uint32_t random_word(uint32_t* state) {
    uint32_t x=*state;x^=x<<13;x^=x>>17;x^=x<<5;return *state=x;
}
static void fill(uint8_t* bytes,size_t size,uint32_t* state) {
    for(size_t i=0;i<size;++i)bytes[i]=(uint8_t)random_word(state);
}
static bool diff_case(buffers_t* b,int pixels,unsigned offset,unsigned* count) {
    memset(b->actual,0xa5,sizeof(b->actual));memcpy(b->expected,b->actual,sizeof(b->actual));
    memset(b->columns,0x42,sizeof(b->columns));memcpy(b->columns_ref,b->columns,sizeof(b->columns));
    bool changed=false;
    for(int x=0;x<pixels;++x) {
        uint8_t to=(b->target[offset+x/2]>>(4*(x&1)))&15;
        uint8_t from=(b->previous[offset+x/2]>>(4*(x&1)))&15;
        b->expected[offset+x]=(to<<4)|from;
        b->columns_ref[offset+x/2]|=(to^from)<<(4*(x&1));changed|=to!=from;
    }
    uint32_t dirty=epd_interlace_4bpp_line_VE(b->target+offset,b->previous+offset,
        b->actual+offset,b->columns+offset,pixels);
    ++*count;
    if((dirty!=0)!=changed||memcmp(b->actual,b->expected,sizeof(b->actual))||
       memcmp(b->columns,b->columns_ref,sizeof(b->columns))) {
        unsigned first=0;while(first<sizeof(b->actual)&&b->actual[first]==b->expected[first])++first;
        ESP_LOGE("pie_bench","DIFF mismatch pixels=%d offset=%u first=%u actual=%u expected=%u",
            pixels,offset,first,first<sizeof(b->actual)?b->actual[first]:0,first<sizeof(b->actual)?b->expected[first]:0);
        for(unsigned i=0;i<8;++i)ESP_LOGI("pie_bench","DIFF_BYTES i=%u to=%u from=%u got0=%u got1=%u want0=%u want1=%u",
            i,b->target[offset+i],b->previous[offset+i],b->actual[offset+2*i],b->actual[offset+2*i+1],
            b->expected[offset+2*i],b->expected[offset+2*i+1]);
        return false;
    }
    return true;
}
static bool mask_case(buffers_t* b,int length,unsigned offset,unsigned mask_offset,unsigned* count) {
    memcpy(b->actual,b->target,B);memcpy(b->expected,b->actual,sizeof(b->actual));
    for(int x=0;x<length;++x)b->expected[offset+x]&=b->mask[mask_offset+x];
    epd_apply_line_mask_VE(b->actual+offset,b->mask+mask_offset,length);++*count;
    if(memcmp(b->actual,b->expected,sizeof(b->actual))) {
        ESP_LOGE("pie_bench","MASK mismatch length=%d offset=%u mask_offset=%u",length,offset,mask_offset);
        return false;
    }
    return true;
}
static bool verify(buffers_t* b,unsigned* cases) {
    uint32_t seed=UINT32_C(0x731a95ed);fill(b->target,B,&seed);fill(b->previous,B,&seed);fill(b->mask,B,&seed);
    // 同时检查对齐PIE、未对齐C、奇数像素、非整块尾部和所有前后哨兵。
    // Check aligned PIE,unaligned C,odd pixels,partial tails and all surrounding sentinels.
    for(unsigned offset=0;offset<16;++offset) {
        for(int n=0;n<=129;++n)if(!diff_case(b,n,offset,cases))return false;
        if(!diff_case(b,1216,offset,cases)||!diff_case(b,1215,offset,cases))return false;
        for(int n=0;n<=321;++n)if(!mask_case(b,n,offset,offset,cases))return false;
        if(!mask_case(b,304,offset,(offset+7)&15,cases))return false;
    }
    memcpy(b->previous,b->target,B);
    for(int n=31;n<=1216;n+=31)if(!diff_case(b,n,0,cases))return false;
    for(unsigned a=0;a<16;++a)for(unsigned f=0;f<16;++f) {
        memset(b->target,a*17,B);memset(b->previous,f*17,B);
        if(!diff_case(b,64,0,cases))return false;
    }
    return true;
}
static int64_t measure_mask(buffers_t* b,bool vector) {
    const int repeats=4000;int64_t start=esp_timer_get_time();
    for(int n=0;n<repeats;++n) {
        if(vector)epd_apply_line_mask_VE(b->actual,b->mask,304);
        else epd_apply_line_mask_c(b->actual,b->mask,304);
    }
    return esp_timer_get_time()-start;
}
static int64_t measure_diff(buffers_t* b,bool vector) {
    const int repeats=400;int64_t start=esp_timer_get_time();
    for(int n=0;n<repeats;++n) {
        if(vector)epd_interlace_4bpp_line_VE(b->target,b->previous,b->actual,b->columns,1216);
        else epd_interlace_4bpp_line_c(b->target,b->previous,b->actual,b->columns,1216);
    }
    return esp_timer_get_time()-start;
}
static void benchmark_task(void* argument) {
    bench_request_t* request=argument;
    buffers_t* b=heap_caps_aligned_alloc(16,sizeof(*b),MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT);
    const int core=esp_cpu_get_core_id();
    request->passed=b&&core==1&&verify(b,&request->cases);
    ESP_LOGI("pie_bench","PIE_SELFTEST core=%d cases=%u passed=%d",core,request->cases,request->passed);
    if(request->passed) {
        for(int round=0;round<4;++round) {
            int64_t scalar,vector;
            // 交替顺序减轻缓存/温度偏差，均计入真实分派和尾部处理。
            // Alternate order to limit cache/thermal bias; include actual dispatch/tail overhead.
            if(round&1){vector=measure_mask(b,true);scalar=measure_mask(b,false);}
            else {scalar=measure_mask(b,false);vector=measure_mask(b,true);}
            ESP_LOGI("pie_bench","PIE_BENCH op=mask memory=internal bytes=304 repeats=4000 round=%d c_us=%lld pie_us=%lld",round,scalar,vector);
            if(round&1){vector=measure_diff(b,true);scalar=measure_diff(b,false);}
            else {scalar=measure_diff(b,false);vector=measure_diff(b,true);}
            ESP_LOGI("pie_bench","PIE_BENCH op=diff memory=internal pixels=1216 repeats=400 round=%d c_us=%lld pie_us=%lld",round,scalar,vector);
        }
    }
    heap_caps_free(b);xSemaphoreGive(request->done);vTaskDelete(NULL);
}
void epd_pie_pixels_bench(void) {
    bool before=epd_pie_pixels_enabled();epd_pie_pixels_enable(true);
    bench_request_t request={.done=xSemaphoreCreateBinary()};
    if(request.done&&xTaskCreatePinnedToCore(benchmark_task,"pie_check",4096,&request,
        configMAX_PRIORITIES-2,NULL,1)==pdPASS) {
        xSemaphoreTake(request.done,portMAX_DELAY);
        // 主任务保持Core0；同一分派接口在Core0也必须安全回退。
        // Main stays onCore0; the same dispatch must safely fall back there.
        uint8_t a[32] __attribute__((aligned(16)))={0};
        uint8_t mask[16] __attribute__((aligned(16)));memset(mask,0xff,sizeof(mask));
        epd_apply_line_mask_VE(a,mask,16);
        ESP_LOGI("pie_bench","PIE_DONE passed=%d cases=%u caller_core=%d",request.passed,request.cases,esp_cpu_get_core_id());
    } else ESP_LOGE("pie_bench","PIE_DONE allocation/task failed");
    if(request.done)vSemaphoreDelete(request.done);
    epd_pie_pixels_enable(before&&request.passed);
}
