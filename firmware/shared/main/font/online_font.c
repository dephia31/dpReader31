/* SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
 * 中文：单一后台任务复用已有STA或临时连接，流式下载固定字体到TF。
 * English: One worker reuses STA or connects temporarily, streaming pinned fonts to TF.
 * 冻结：不停止别人的传书会话，不跳过TLS验证，不自动启用下载字体。
 * Frozen: never stop another transfer session, bypass TLS, or apply a downloaded face.
 */
#include "online_font.h"
#include "online_font_store.h"
#include <errno.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "read_pico_sd.h"
#include "read_pico_transfer.h"
#include "ble_page_turner.h"
#include "online_font_catalog.inc"

static portMUX_TYPE s_lock=portMUX_INITIALIZER_UNLOCKED;
static online_font_status_t s_status;
static atomic_bool s_running,s_cancel;
static const char *TAG="online_font";

const online_font_item_t *online_font_item(unsigned index){return index<ONLINE_FONT_COUNT?&s_fonts[index]:NULL;}
bool online_font_installed(unsigned index){
    const online_font_item_t *item=online_font_item(index);
    return item&&read_pico_sd_io_ready()&&online_font_store_installed(item,ONLINE_FONT_DIRECTORY);
}
void online_font_status(online_font_status_t *out){
    if(!out)return;
    portENTER_CRITICAL(&s_lock);*out=s_status;portEXIT_CRITICAL(&s_lock);
}
static void state(online_font_phase_t phase,const char *message){
    portENTER_CRITICAL(&s_lock);s_status.phase=phase;
    snprintf(s_status.message,sizeof(s_status.message),"%s",message);++s_status.revision;
    portEXIT_CRITICAL(&s_lock);
}
static bool cancelled(void *ctx){(void)ctx;return atomic_load(&s_cancel)||!read_pico_sd_io_ready();}
void online_font_cancel_join(void){
    atomic_store(&s_cancel,true);
    while(atomic_load(&s_running))vTaskDelay(pdMS_TO_TICKS(10));
}
static bool url_allowed(const char *url){
    static const char *hosts[]={"github.com/","raw.githubusercontent.com/","release-assets.githubusercontent.com/","objects.githubusercontent.com/"};
    if(strncmp(url,"https://",8))return false;
    for(unsigned i=0;i<sizeof(hosts)/sizeof(hosts[0]);++i)
        if(!strncmp(url+8,hosts[i],strlen(hosts[i])))return true;
    return false;
}
typedef struct {char *location;bool overflow;} redirect_t;
static esp_err_t header_event(esp_http_client_event_t *event){
    redirect_t *r=event->user_data;
    if(!r)return ESP_OK;
    if(event->event_id==HTTP_EVENT_ON_HEADER&&event->header_key&&event->header_value&&
       !strcasecmp(event->header_key,"Location")){
        size_t n=strlen(event->header_value);r->overflow=n>=4096;
        if(!r->overflow)memcpy(r->location,event->header_value,n+1);
    }
    return ESP_OK;
}
static esp_http_client_handle_t open_source(const char *source){
    char *url=heap_caps_malloc(4096,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    char *next=heap_caps_malloc(4096,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    esp_http_client_handle_t client=NULL;
    if(!url||!next)goto done;
    snprintf(url,4096,"%s",source);
    for(unsigned redirects=0;redirects<=4&&!cancelled(NULL);++redirects){
        if(!url_allowed(url))break;
        next[0]=0;redirect_t redirect={.location=next};
        esp_http_client_config_t config={.url=url,.timeout_ms=4000,.buffer_size=2048,
            .buffer_size_tx=1024,.crt_bundle_attach=esp_crt_bundle_attach,
            .disable_auto_redirect=true,.event_handler=header_event,.user_data=&redirect};
        client=esp_http_client_init(&config);if(!client)break;
        esp_http_client_set_header(client,"Accept-Encoding","identity");
        esp_http_client_set_header(client,"User-Agent","dpReader/1.0");
        if(esp_http_client_open(client,0)!=ESP_OK||cancelled(NULL)||esp_http_client_fetch_headers(client)<0)goto failed;
        int status=esp_http_client_get_status_code(client);
        if(status==200){esp_http_client_set_user_data(client,NULL);break;}
        if((status==301||status==302||status==303||status==307||status==308)&&
           !redirect.overflow&&next[0]&&url_allowed(next)){
            snprintf(url,4096,"%s",next);esp_http_client_cleanup(client);client=NULL;continue;
        }
failed:
        esp_http_client_cleanup(client);client=NULL;break;
    }
done:
    free(url);free(next);return client;
}
static esp_err_t ensure_network(bool *owned){
#if DPREADER_BOARD_METALIO47
    read_pico_transfer_prepare_network();
#endif
    if(!read_pico_transfer_network_shareable())return ESP_ERR_INVALID_STATE;
    read_pico_transfer_status_t status;read_pico_transfer_get_status(&status);
    if(status.mode==READ_PICO_TRANSFER_MODE_STA&&status.network_ready&&status.state!=READ_PICO_TRANSFER_UPLOADING)return ESP_OK;
    if(status.state!=READ_PICO_TRANSFER_STOPPED)return ESP_ERR_INVALID_STATE;
    char name[33];bool saved=false;
    if(read_pico_transfer_get_saved_wifi(name,&saved)!=ESP_OK||!saved)return ESP_ERR_NOT_FOUND;
    read_pico_transfer_cfg_t config={.mode=READ_PICO_TRANSFER_MODE_STA,.network_only=true};
    esp_err_t result=read_pico_transfer_start(&config);
    if(result!=ESP_OK)return result;
    *owned=true;
    int64_t limit=esp_timer_get_time()+30000000;
    while(!cancelled(NULL)&&esp_timer_get_time()<limit){
        read_pico_transfer_service_poll();read_pico_transfer_get_status(&status);
        if(status.network_ready)return ESP_OK;
        if(status.state==READ_PICO_TRANSFER_ERROR)break;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    return ESP_ERR_TIMEOUT;
}
static bool make_directory(const char *path){struct stat st;return !stat(path,&st)?S_ISDIR(st.st_mode):(!mkdir(path,0755)||errno==EEXIST);}
static void worker(void *arg){
    unsigned index=(unsigned)(uintptr_t)arg;const online_font_item_t *item=online_font_item(index);
    const char *source=ONLINE_FONT_DIRECTORY "/.kiiko-font.source";
    bool network_held=false;
    bool owned=false,ble_lease=false;uint8_t *buffer=NULL;FILE *file=NULL;
    esp_http_client_handle_t client=NULL;esp_err_t result=ESP_FAIL;
    const char *message="Tải thất bại, vui lòng thử lại";
    if(!item||cancelled(NULL))goto done;
    read_pico_sd_info_t sd={0};
    if(read_pico_sd_get_info(&sd)!=ESP_OK||!sd.mounted){message="Vui lòng lắp thẻ nhớ trước";goto done;}
    if(read_pico_sd_refresh_space()!=ESP_OK||read_pico_sd_get_info(&sd)!=ESP_OK||!sd.space_valid){message="Không kiểm tra được dung lượng thẻ nhớ";goto done;}
    uint64_t needed=item->source_size+(item->deflate_size?item->size:0)+strlen(item->license)+131072u;
    if(sd.free_bytes<needed){message="Thẻ nhớ không đủ dung lượng trống";goto done;}
    if(!make_directory("/sdcard/assets")||!make_directory(ONLINE_FONT_DIRECTORY))goto done;
    if(online_font_store_installed(item,ONLINE_FONT_DIRECTORY)){result=ESP_OK;goto done;}
    // 字体下载独占一个临时BLE租约，结束按原用户配置恢复，不持久改开关。
    // Borrow a BLE lease for font transfer; restore the original user preference on completion.
    if(!(network_held=read_pico_transfer_network_begin())){result=ESP_ERR_INVALID_STATE;goto done;}
    result=ble_pt_network_acquire();if(result!=ESP_OK){message="Bluetooth đang bận, vui lòng thử lại sau";goto done;}ble_lease=true;
    result=ensure_network(&owned);
    if(result!=ESP_OK){message=result==ESP_ERR_NOT_FOUND?"Vui lòng kết nối mạng trong Cài đặt Wi-Fi":"Kết nối Wi-Fi thất bại, vui lòng thử lại";goto done;}
    if(time(NULL)<1704067200){message="Vui lòng đồng bộ giờ qua Wi-Fi trước";result=ESP_ERR_INVALID_STATE;goto done;}
    if(cancelled(NULL)){result=ESP_ERR_INVALID_STATE;goto done;}
    buffer=heap_caps_malloc(8192,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!buffer){message="Không đủ bộ nhớ, vui lòng thử lại sau";result=ESP_ERR_NO_MEM;goto done;}
    client=open_source(item->url);if(!client){result=ESP_FAIL;message="Không kết nối được nguồn tải phông";goto done;}
    int64_t length=esp_http_client_get_content_length(client);
    if(length>0&&(uint64_t)length!=item->source_size){message="Kích thước tệp phông không khớp";result=ESP_ERR_INVALID_SIZE;goto done;}
    file=fopen(source,"wb");if(!file){result=ESP_FAIL;goto done;}
    state(ONLINE_FONT_DOWNLOADING,"Đang tải phông…");
    uint32_t received=0;int64_t idle=esp_timer_get_time();
    result=ESP_FAIL;
    while(!cancelled(NULL)){
        int n=esp_http_client_read(client,(char *)buffer,8192);
        if(n==-ESP_ERR_HTTP_EAGAIN&&esp_timer_get_time()-idle<20000000){vTaskDelay(pdMS_TO_TICKS(10));continue;}
        if(n<0)break;
        if(!n){if(esp_http_client_is_complete_data_received(client)){result=received==item->source_size?ESP_OK:ESP_ERR_INVALID_SIZE;break;}
            if(esp_timer_get_time()-idle>20000000)break;
            vTaskDelay(pdMS_TO_TICKS(10));continue;}
        if((unsigned)n>item->source_size-received){result=ESP_ERR_INVALID_SIZE;break;}
        if(!read_pico_sd_io_ready()||fwrite(buffer,1,(size_t)n,file)!=(size_t)n)break;
        received+=(uint32_t)n;idle=esp_timer_get_time();
        portENTER_CRITICAL(&s_lock);s_status.received=received;++s_status.revision;portEXIT_CRITICAL(&s_lock);
    }
    if(fflush(file)||fsync(fileno(file)))result=ESP_FAIL;
    if(fclose(file))result=ESP_FAIL;
    file=NULL;
    esp_http_client_cleanup(client);client=NULL;
    if(result==ESP_OK&&!cancelled(NULL)){
        state(ONLINE_FONT_VERIFYING,"Đang kiểm tra tệp…");message="Kiểm tra hoặc cài phông thất bại";
        result=online_font_store_commit(item,ONLINE_FONT_DIRECTORY,source,cancelled,NULL);
    }
done:
    if(client)esp_http_client_cleanup(client);
    if(file)fclose(file);
    if(read_pico_sd_io_ready())unlink(source);
    free(buffer);
    if(owned)read_pico_transfer_stop();
    if(ble_lease)ble_pt_network_release();
    if(network_held)read_pico_transfer_network_end();
    bool aborted=cancelled(NULL);
    if(aborted)state(ONLINE_FONT_CANCELLED,"Đã hủy tải phông");
    else if(result==ESP_OK)state(ONLINE_FONT_DONE,"Tải xong. Quay lại danh sách để chọn phông.");
    else{state(ONLINE_FONT_FAILED,message);ESP_LOGW(TAG,"item=%u failed: %s",index,esp_err_to_name(result));}
    portENTER_CRITICAL(&s_lock);s_status.busy=false;++s_status.revision;portEXIT_CRITICAL(&s_lock);
    atomic_store(&s_running,false);vTaskDelete(NULL);
}
esp_err_t online_font_start(unsigned index){
    const online_font_item_t *item=online_font_item(index);if(!item)return ESP_ERR_INVALID_ARG;
    if(atomic_exchange(&s_running,true))return ESP_ERR_INVALID_STATE;
    atomic_store(&s_cancel,false);
    portENTER_CRITICAL(&s_lock);uint32_t revision=s_status.revision+1;
    s_status=(online_font_status_t){.phase=ONLINE_FONT_CONNECTING,.item=index,.total=item->source_size,.revision=revision,.busy=true};
    snprintf(s_status.message,sizeof(s_status.message),"Đang kết nối Wi-Fi…");portEXIT_CRITICAL(&s_lock);
    if(xTaskCreate(worker,"font_online",8192,(void *)(uintptr_t)index,3,NULL)!=pdPASS){
        state(ONLINE_FONT_FAILED,"Không đủ bộ nhớ, vui lòng thử lại sau");
        portENTER_CRITICAL(&s_lock);s_status.busy=false;portEXIT_CRITICAL(&s_lock);
        atomic_store(&s_running,false);return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
