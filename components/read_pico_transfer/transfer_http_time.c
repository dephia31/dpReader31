/* 中文：NTP不可达时用已验证TLS证书的HEAD响应取UTC，不下载网页、不跟随降级跳转。
 * English: If NTP is unreachable, obtain UTC from certificate-verified HTTPS HEAD responses. */
#include "transfer_http_time.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_log.h"
#include <strings.h>
#include <stdlib.h>
#include <sys/time.h>

typedef struct {uint32_t utc;unsigned age;bool have_date,bad_age;} http_time_headers_t;
static esp_err_t time_header(esp_http_client_event_t *event) {
    if(event->event_id!=HTTP_EVENT_ON_HEADER || !event->header_key || !event->header_value) return ESP_OK;
    http_time_headers_t *headers=event->user_data;
    if(!strcasecmp(event->header_key,"Date")) headers->have_date=transfer_http_date(event->header_value,&headers->utc);
    else if(!strcasecmp(event->header_key,"Age")) {
        char *end;unsigned long age=strtoul(event->header_value,&end,10);
        headers->bad_age=end==event->header_value || *end || age>300;
        if(!headers->bad_age) headers->age=(unsigned)age;
    }
    return ESP_OK;
}
esp_err_t transfer_sync_time_https(uint32_t *utc_seconds) {
    if(!utc_seconds) return ESP_ERR_INVALID_ARG;
    static const char *const sources[]={"https://www.cloudflare.com/","https://www.google.com/"};
    esp_err_t err=ESP_ERR_TIMEOUT;
    for(unsigned i=0;i<sizeof(sources)/sizeof(sources[0]);i++) {
        http_time_headers_t headers={0};
        esp_http_client_config_t cfg={.url=sources[i],.method=HTTP_METHOD_HEAD,.timeout_ms=5000,
            .addr_type=HTTP_ADDR_TYPE_INET,
            .crt_bundle_attach=esp_crt_bundle_attach,.disable_auto_redirect=true,
            .buffer_size=1024,.buffer_size_tx=512,.event_handler=time_header,.user_data=&headers};
        esp_http_client_handle_t client=esp_http_client_init(&cfg);
        if(!client) return ESP_ERR_NO_MEM;
        // 已有连接只有IPv4 DHCP；不让首个AAAA地址把请求引到无路由的IPv6。
        // DHCP supplies IPv4; do not pick an unroutable IPv6 address from DNS.
        ESP_LOGI("time_sync","HTTPS time attempt=%u family=IPv4",i+1);
        err=esp_http_client_perform(client);
        int status=esp_http_client_get_status_code(client);
        esp_http_client_cleanup(client);
        // HEAD不支持时仍可从已验证TLS服务的Date取时，避免下载响应正文。
        // A HEAD error can still carry the verified server Date; never fetch its body.
        if(err==ESP_OK && status>=200 && status<600 && headers.have_date && !headers.bad_age) {
            uint32_t utc=headers.utc+headers.age;
            struct timeval tv={.tv_sec=utc,.tv_usec=0};
            if(settimeofday(&tv,NULL)) return ESP_FAIL;
            *utc_seconds=utc;
            ESP_LOGI("time_sync","HTTPS time complete utc=%lu",(unsigned long)utc);
            return ESP_OK;
        }
        if(err==ESP_OK) err=ESP_ERR_INVALID_RESPONSE;
        ESP_LOGW("time_sync","HTTPS time attempt=%u status=%d result=%s",i+1,status,esp_err_to_name(err));
    }
    return err;
}
