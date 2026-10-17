/* 中文：独立S31手动对时，不创建/销毁全局SNTP定时服务。
 * English: Independent S31 manual sync; no global SNTP timer lifecycle. */
#include "transfer_ntp_time.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include <errno.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#include <arpa/inet.h>

esp_err_t transfer_sync_time_ntp_cancelable(uint32_t *utc_seconds, bool (*cancelled)(void)) {
    if(!utc_seconds) return ESP_ERR_INVALID_ARG;
    *utc_seconds=0;
    // 原厂MetalioRtc使用time.windows.com；国内公网源作为单次请求备用。
    // The reference MetalioRtc uses time.windows.com; retain public fallback sources.
    static const char *const servers[]={"time.windows.com","ntp.aliyun.com","time.cloud.tencent.com"};
    esp_err_t result=ESP_ERR_TIMEOUT;
    for(unsigned source=0;source<sizeof(servers)/sizeof(servers[0]);source++) {
        if (cancelled && cancelled()) return ESP_ERR_INVALID_STATE;
        struct addrinfo hints={.ai_family=AF_INET,.ai_socktype=SOCK_DGRAM},*address=NULL;
        int dns=getaddrinfo(servers[source],"123",&hints,&address);
        if(dns || !address) {
            if(address) freeaddrinfo(address);
            ESP_LOGW("time_sync","NTP DNS source=%u result=%d",source+1,dns);
            continue;
        }
        if (cancelled && cancelled()) {freeaddrinfo(address);return ESP_ERR_INVALID_STATE;}
        const struct sockaddr_in *peer=(const struct sockaddr_in*)address->ai_addr;
        char ip[INET_ADDRSTRLEN]={0};
        inet_ntop(AF_INET,&peer->sin_addr,ip,sizeof(ip));
        ESP_LOGI("time_sync","NTP source=%u host=%s IPv4=%s",source+1,servers[source],ip);
        int fd=socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
        if(fd<0) {freeaddrinfo(address);return errno==ENOMEM?ESP_ERR_NO_MEM:ESP_FAIL;}
        // UDP connect限定回包来源；所有退出路径均关闭socket并释放DNS结果。
        // Connected UDP restricts the peer; release sockets and DNS results on every exit.
        int connected=connect(fd,address->ai_addr,address->ai_addrlen);
        freeaddrinfo(address);
        if(connected) {ESP_LOGW("time_sync","NTP UDP connect errno=%d",errno);close(fd);continue;}
        struct timeval timeout={.tv_usec=100000};
        if(setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout))) {close(fd);return ESP_FAIL;}
        for(unsigned attempt=0;attempt<3;attempt++) {
            if (cancelled && cancelled()) {close(fd);return ESP_ERR_INVALID_STATE;}
            uint8_t request[48]={0x23},reply[128];
            esp_fill_random(request+40,8);request[47]|=1;
            int64_t started=esp_timer_get_time();
            ssize_t sent=send(fd,request,sizeof(request),0);
            if(sent!=(ssize_t)sizeof(request)) {
                ESP_LOGW("time_sync","NTP send source=%u attempt=%u bytes=%d errno=%d",source+1,attempt+1,(int)sent,errno);
                break;
            }
            ssize_t bytes;
            do {
                if (cancelled && cancelled()) {close(fd);return ESP_ERR_INVALID_STATE;}
                bytes=recv(fd,reply,sizeof(reply),0);
            } while (bytes<0 && (errno==EAGAIN || errno==EWOULDBLOCK || errno==EINTR) && esp_timer_get_time()-started<2000000);
            if (cancelled && cancelled()) {close(fd);return ESP_ERR_INVALID_STATE;}
            int64_t elapsed=esp_timer_get_time()-started;
            uint32_t utc=0;
            if(bytes>=0 && transfer_ntp_reply(reply,(size_t)bytes,request+40,&utc)) {
                uint64_t micros=(uint64_t)utc*1000000+
                    ((uint64_t)transfer_ntp_u32(reply+44)*1000000>>32);
                if(elapsed>0 && elapsed<=2000000) micros+=(uint64_t)elapsed/2;
                struct timeval tv={.tv_sec=(time_t)(micros/1000000),.tv_usec=(suseconds_t)(micros%1000000)};
                close(fd);
                if(settimeofday(&tv,NULL)) return ESP_FAIL;
                *utc_seconds=(uint32_t)tv.tv_sec;
                ESP_LOGI("time_sync","NTP complete utc=%lu rtt_ms=%ld",(unsigned long)*utc_seconds,(long)(elapsed/1000));
                return ESP_OK;
            }
            ESP_LOGW("time_sync","NTP receive source=%u attempt=%u bytes=%d errno=%d valid=0",
                source+1,attempt+1,(int)bytes,bytes<0?errno:0);
            if(bytes>=0) result=ESP_ERR_INVALID_RESPONSE;
            if(bytes<0 && errno!=EAGAIN && errno!=EWOULDBLOCK && errno!=EINTR) break;
        }
        close(fd);
    }
    return result;
}

esp_err_t transfer_sync_time_ntp(uint32_t *utc_seconds) {
    return transfer_sync_time_ntp_cancelable(utc_seconds,NULL);
}
