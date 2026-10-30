/* SPDX-License-Identifier: Apache-2.0
 * S31 stage1: SD PCM playback only. User starts explicitly; no network/boot audio.
 * Vendor BH1098G mapping: UART1 TX39/RX38, I2S0 slave BCLK42/WS43/DOUT44,
 * stereo32 slots at16kHz, PA expanderP4. Never toggle the shared P14 supply.
 */
#include "metalio_audio.h"
#include "metalio_wav.h"
#include "metalio_hw.h"
#include "driver/i2s_std.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include <stdatomic.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
static atomic_int state=METALIO_AUDIO_IDLE;
static atomic_bool cancel;
static SemaphoreHandle_t done;
static bool pending;
static char path[320];
static _Atomic(const char *) message="Sẵn sàng";
static i2s_chan_handle_t tx;
static bool uart_ready,tx_enabled,cleanup_failed;
static void status(metalio_audio_state_t value,const char *text){atomic_store(&message,text);atomic_store_explicit(&state,value,memory_order_release);}
metalio_audio_state_t metalio_audio_state(void){return atomic_load_explicit(&state,memory_order_acquire);}
const char *metalio_audio_message(void){(void)metalio_audio_state();return atomic_load(&message);}
static bool wait_ms(unsigned ms){
    for(unsigned n=0;n<ms;n+=10){if(atomic_load(&cancel))return false;vTaskDelay(pdMS_TO_TICKS(10));}
    return !atomic_load(&cancel);
}
static bool read_reply(unsigned timeout_ms,bool require_ack){
    uint8_t bytes[64];char line[128];size_t used=0;bool overflow=false;
    int64_t deadline=esp_timer_get_time()+(int64_t)timeout_ms*1000;
    while(!atomic_load(&cancel)&&esp_timer_get_time()<deadline){
        int received=uart_read_bytes(UART_NUM_1,bytes,sizeof(bytes),pdMS_TO_TICKS(20));
        if(received<0)return false;
        if(!received&&!require_ack)return used==0&&!overflow;
        for(int i=0;i<received;++i){
            unsigned c=bytes[i];
            if(c=='\r'||c=='\n'){
                line[used]=0;
                if(overflow)return false;
                if(strstr(line,"ERROR")||strstr(line,"FAIL"))return false;
                if(require_ack&&metalio_audio_mode_ack(line))return true;
                used=0;overflow=false;
            }else if(c>=32&&c<127){if(used+1<sizeof(line))line[used++]=(char)c;else overflow=true;}
        }
    }
    return false;
}
static bool mode_ready(void){
    uart_config_t cfg={.baud_rate=115200,.data_bits=UART_DATA_8_BITS,.parity=UART_PARITY_DISABLE,
        .stop_bits=UART_STOP_BITS_1,.flow_ctrl=UART_HW_FLOWCTRL_DISABLE,.source_clk=UART_SCLK_DEFAULT};
    if(uart_driver_install(UART_NUM_1,1024,0,0,NULL,0)!=ESP_OK)return false;
    uart_ready=true;
    if(uart_param_config(UART_NUM_1,&cfg)!=ESP_OK||uart_set_pin(UART_NUM_1,39,38,UART_PIN_NO_CHANGE,UART_PIN_NO_CHANGE)!=ESP_OK)return false;
    if(uart_flush_input(UART_NUM_1)!=ESP_OK||uart_write_bytes(UART_NUM_1,"AT+RX=2\r\n",9)!=9)return false;
    if(!wait_ms(700)||!read_reply(100,false))return false;
    if(uart_write_bytes(UART_NUM_1,"AT+MODE=1\r\n",11)!=11)return false;
    return read_reply(3000,true);
}
static bool output_ready(void){
    i2s_chan_config_t chan=I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0,I2S_ROLE_SLAVE);
    chan.dma_desc_num=4;chan.dma_frame_num=256;
    if(i2s_new_channel(&chan,&tx,NULL)!=ESP_OK)return false;
    i2s_std_config_t cfg={.clk_cfg=I2S_STD_CLK_DEFAULT_CONFIG(16000),
        .slot_cfg=I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT,I2S_SLOT_MODE_STEREO),
        .gpio_cfg={.mclk=I2S_GPIO_UNUSED,.bclk=42,.ws=43,.dout=44,.din=I2S_GPIO_UNUSED}};
    if(i2s_channel_init_std_mode(tx,&cfg)!=ESP_OK||i2s_channel_enable(tx)!=ESP_OK)return false;
    tx_enabled=true;return true;
}
static bool release_output(void){
    bool ok=metalio_io_set(4,false)==ESP_OK;
    if(tx){
        if(tx_enabled){if(i2s_channel_disable(tx)==ESP_OK)tx_enabled=false;else ok=false;}
        if(!tx_enabled&&i2s_del_channel(tx)==ESP_OK){
            tx=NULL;(void)gpio_reset_pin(42);(void)gpio_reset_pin(43);(void)gpio_reset_pin(44);
        }else ok=false;
    }
    if(uart_ready){
        if(uart_driver_delete(UART_NUM_1)==ESP_OK){uart_ready=false;(void)gpio_reset_pin(38);(void)gpio_reset_pin(39);}
        else ok=false;
    }
    cleanup_failed=!ok;return ok;
}
static bool write_slots(const int32_t *slots,size_t bytes){
    const uint8_t *p=(const uint8_t*)slots;
    while(bytes&&!atomic_load(&cancel)){
        size_t sent=0;esp_err_t err=i2s_channel_write(tx,p,bytes,&sent,100);
        if(err!=ESP_OK||!sent||sent>bytes)return false;
        p+=sent;bytes-=sent;
    }
    return !bytes;
}
static void player(void *arg){
    (void)arg;
    FILE *file=NULL;int16_t *pcm=NULL;int32_t *slots=NULL;
    const char *error=NULL;metalio_wav_info_t info;bool released;
    file=fopen(path,"rb");
    if(!file||!metalio_wav_open(file,&info)){error="WAV cần PCM 16-bit, 16 kHz, mono/stereo";goto finish;}
    pcm=malloc(512*sizeof(*pcm));slots=malloc(512*sizeof(*slots));
    if(!pcm||!slots){error="Không đủ bộ nhớ âm thanh";goto finish;}
    if(metalio_io_set(4,false)!=ESP_OK){error="Không tắt được bộ khuếch đại";goto finish;}
    if(!mode_ready()){error="BH1098G chưa xác nhận chế độ I2S";goto finish;}
    if(atomic_load(&cancel))goto finish;
    if(!output_ready()){error="Không khởi tạo được I2S";goto finish;}
    /* Queue silence before enabling PA; detect missing external clocks first. */
    memset(slots,0,512*sizeof(*slots));
    if(!write_slots(slots,512*sizeof(*slots))){error="Không có nhịp I2S hoặc phát bị gián đoạn";goto finish;}
    if(metalio_io_set(4,true)!=ESP_OK){error="Không bật được bộ khuếch đại";goto finish;}
    status(METALIO_AUDIO_PLAYING,"Đang phát WAV · âm lượng 15%");
    uint32_t remaining=info.data_bytes;
    while(remaining&&!atomic_load(&cancel)){
        size_t bytes=remaining<256u*info.channels*2?remaining:256u*info.channels*2;
        if(fread(pcm,1,bytes,file)!=bytes){error="Không đọc được dữ liệu WAV";goto finish;}
        unsigned frames=(unsigned)(bytes/(info.channels*2));
        metalio_audio_expand(pcm,slots,frames,info.channels,15);
        if(!write_slots(slots,frames*2*sizeof(*slots))){error="I2S bị gián đoạn";goto finish;}
        remaining-=(uint32_t)bytes;
    }
    /* More than four256-frame DMA blocks of silence drains the last PCM data. */
    if(!atomic_load(&cancel)){
        memset(slots,0,512*sizeof(*slots));
        for(int i=0;i<6&&!atomic_load(&cancel);++i)if(!write_slots(slots,512*sizeof(*slots))){error="I2S bị gián đoạn";break;}
    }
finish:
    released=release_output();if(file)fclose(file);free(pcm);free(slots);
    if(!released)status(METALIO_AUDIO_ERROR,"Không đóng được phần cứng âm thanh");
    else if(atomic_load(&cancel))status(METALIO_AUDIO_IDLE,"Đã dừng");
    else if(error)status(METALIO_AUDIO_ERROR,error);
    else status(METALIO_AUDIO_DONE,"Đã phát xong");
    xSemaphoreGive(done);vTaskDelete(NULL);
}
void metalio_audio_stop(void){
    if(pending){atomic_store(&cancel,true);xSemaphoreTake(done,portMAX_DELAY);pending=false;}
    if(cleanup_failed)(void)release_output();
}
esp_err_t metalio_audio_play_wav(const char *name){
    if(!name||strncmp(name,"/sdcard/",8)||strstr(name,"/../")||strlen(name)>=sizeof(path))return ESP_ERR_INVALID_ARG;
    metalio_audio_stop();
    if(tx||uart_ready||cleanup_failed){status(METALIO_AUDIO_ERROR,"Phần cứng âm thanh chưa được giải phóng");return ESP_ERR_INVALID_STATE;}
    if(!done)done=xSemaphoreCreateBinary();
    if(!done){status(METALIO_AUDIO_ERROR,"Không đủ bộ nhớ âm thanh");return ESP_ERR_NO_MEM;}
    strcpy(path,name);atomic_store(&cancel,false);status(METALIO_AUDIO_STARTING,"Đang kết nối âm thanh...");
    if(xTaskCreate(player,"wav_player",6144,NULL,4,NULL)!=pdPASS){status(METALIO_AUDIO_ERROR,"Không tạo được tác vụ âm thanh");return ESP_ERR_NO_MEM;}
    pending=true;return ESP_OK;
}
