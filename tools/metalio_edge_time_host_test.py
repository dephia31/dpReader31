"""Run production LCD queue, NTP retry and RTC functions with simulated hardware.
中文：生产函数回归，不复制业务算法。/ English: Extract actual functions, never copies of their algorithms.
"""
from pathlib import Path
import re, subprocess, tempfile
root = Path(__file__).resolve().parents[1]

def function(path, name):
    text=(root/path).read_text()
    start=re.search(r'^(?:static )?[^\n]+\b'+name+r'\([^;{}]*\)\s*\{',text,re.M)
    assert start, name
    at=start.end(); depth=1
    while depth:
        depth+=(text[at]=='{')-(text[at]=='}'); at+=1
    return text[start.start():at]+'\n'

common=r'''
#define _DEFAULT_SOURCE
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
#define IRAM_ATTR
typedef int esp_err_t;
enum {ESP_OK,ESP_ERR_TIMEOUT,ESP_ERR_INVALID_RESPONSE,ESP_ERR_INVALID_STATE,ESP_ERR_INVALID_ARG,ESP_ERR_NO_MEM,ESP_FAIL};
'''

lcd=common+r'''
#include "output_common/white_border.h"
#define BOUNCE_BUF_LINES 8
#define DMA_DESCRIPTOR_BUFFER_OWNER_DMA 1
typedef void* gdma_channel_handle_t;
typedef struct {int unused;} gdma_event_data_t;
static bool border_whitening;
static int done,source_rows,stops;
static uint8_t buffers[2][BOUNCE_BUF_LINES*9];
static struct {
 bool frame_finishing; int native_height,display_lines,line_bytes,dummy_bytes;
 int64_t frame_t0_us,last_frame_us,eof_last_us,eof_dt_min_us,eof_dt_max_us,eof_dt_sum_us,eof_work_min_us,eof_work_max_us,eof_work_sum_us;
 unsigned eof_dt_n,eof_work_n; size_t lines_fed,bb_eof_count;
 struct {int white_border_pixels;} config;
 struct {void *dev;} hal; void *dma_chan;
 bool (*line_source_cb)(void*,uint8_t*); void *line_cb_payload;
 void (*frame_done_cb)(void*); void *frame_cb_payload;
 uint8_t *bounce_buffer[2]; struct {struct {int owner;} dw0;} dma_nodes[2];
} lcd;
static int64_t esp_timer_get_time(void){static int64_t t;return ++t;}
static void lcd_ll_stop(void *dev){(void)dev;++stops;}
static void gdma_stop(void *dev){(void)dev;}
static bool source(void *ctx,uint8_t *data){(void)ctx;uint32_t row=(uint32_t)source_rows++;memset(data,0,8);memcpy(data,&row,4);return false;}
static void finished(void *ctx){(void)ctx;++done;}
'''
lcd_path='components/epdiy/src/output_lcd/lcd_driver.c'
for name in ('lcd_finish_frame_from_isr','fill_bounce_buffer','lcd_rgb_panel_eof_handler'):
    lcd+=function(lcd_path,name)
lcd+=r'''
int main(void){
 lcd.native_height=684;lcd.display_lines=688;lcd.line_bytes=8;lcd.dummy_bytes=1;
 lcd.line_source_cb=source;lcd.frame_done_cb=finished;
 for(int i=0;i<2;i++){lcd.bounce_buffer[i]=buffers[i];fill_bounce_buffer(buffers[i]);}
 assert(lcd.lines_fed==16&&!done&&!lcd.frame_finishing);
 int sent=0;
 while(!lcd.frame_finishing){
  assert(sent<704);int bb=(sent/8)&1;
  for(int i=0;i<8;i++){
   uint32_t row;memcpy(&row,buffers[bb]+i*9+1,4);
   if(sent+i<688)assert(row==(uint32_t)(sent+i));else assert(row==0);
  }
  sent+=8;lcd_rgb_panel_eof_handler(NULL,NULL,NULL);
  if(sent<=688)assert(!lcd.frame_finishing);
 }
 assert(sent==696&&source_rows==688&&done==1&&stops==1);
 lcd_rgb_panel_eof_handler(NULL,NULL,NULL);assert(done==1&&source_rows==688);
 puts("PASS: production LCD delivers every row including final 16, drains prefetched buffers/FIFO, blanks padding, finishes once even without VSYNC");
}
'''

network=common+r'''
#define pdMS_TO_TICKS(ms) (ms)
#define pdTRUE true
#define pdFALSE false
#define TIME_GOT_IP 1
#define TIME_DISCONNECTED 2
typedef unsigned EventBits_t;
static void *s_time_events=(void*)1;
static int connections,connect_failure,event_index;
static int events[3];
static bool s_time_automatic;
static bool time_sync_cancelled(void){return false;}static int64_t us;
static int64_t esp_timer_get_time(void){return us;}
static esp_err_t esp_wifi_connect(void){++connections;return connect_failure?ESP_FAIL:ESP_OK;}
static void xEventGroupClearBits(void *e,unsigned bits){assert(e==s_time_events&&bits==3);}
static unsigned xEventGroupWaitBits(void *e,unsigned bits,bool clear,bool all,unsigned ms){assert(e==s_time_events&&bits==3&&clear&&!all&&ms<=15000);unsigned event=event_index<3?events[event_index++]:0;us+=event==TIME_DISCONNECTED?100000:(int64_t)ms*1000;return event;}
static void vTaskDelay(unsigned ms){us+=(int64_t)ms*1000;}
'''
network+=r'''
typedef int wifi_ps_type_t;
enum {WIFI_PS_NONE,WIFI_PS_MIN_MODEM};
static wifi_ps_type_t power_save=WIFI_PS_MIN_MODEM;
static int get_ps_fail,set_ps_fail,ps_sets;
static esp_err_t esp_wifi_get_ps(wifi_ps_type_t *out){*out=power_save;return get_ps_fail?ESP_FAIL:ESP_OK;}
static esp_err_t esp_wifi_set_ps(wifi_ps_type_t mode){++ps_sets;if(set_ps_fail)return ESP_FAIL;power_save=mode;return ESP_OK;}
static int ntp_calls,https_calls;static esp_err_t ntp_result=ESP_ERR_TIMEOUT,https_result=ESP_ERR_TIMEOUT;
static esp_err_t transfer_sync_time_ntp_cancelable(uint32_t *utc,bool (*cancel)(void)){(void)cancel;assert(power_save==WIFI_PS_NONE);++ntp_calls;if(ntp_result==ESP_OK)*utc=1791633600;return ntp_result;}
static esp_err_t transfer_sync_time_https(uint32_t *utc){assert(power_save==WIFI_PS_NONE);++https_calls;if(https_result==ESP_OK)*utc=1791633600;return https_result;}
'''
for name in ('time_connect','sync_network_time'):
    network+=function('components/read_pico_transfer/read_pico_transfer.c',name)
network+=r'''
int main(void){
 uint32_t utc=0;
 events[0]=TIME_GOT_IP;assert(time_connect()==ESP_OK&&connections==1);
 connections=event_index=0;us=0;events[0]=events[1]=TIME_DISCONNECTED;events[2]=TIME_GOT_IP;
 assert(time_connect()==ESP_OK&&connections==3);
 connections=event_index=0;us=0;events[2]=TIME_DISCONNECTED;
 assert(time_connect()==ESP_ERR_TIMEOUT&&connections==3&&us<30000000);
 connections=event_index=0;us=0;events[0]=events[1]=events[2]=0;
 assert(time_connect()==ESP_ERR_TIMEOUT&&connections==1);
 connect_failure=1;connections=0;assert(time_connect()==ESP_FAIL&&connections==1);
 https_result=ESP_OK;
 assert(sync_network_time(&utc)==ESP_OK&&utc==1791633600&&https_calls==1);
 assert(power_save==WIFI_PS_MIN_MODEM&&ps_sets==2);
 ntp_result=ESP_OK;
 assert(sync_network_time(&utc)==ESP_OK&&https_calls==1); // No HTTPS when NTP succeeds.
 assert(power_save==WIFI_PS_MIN_MODEM&&ps_sets==4);
 ntp_result=ESP_ERR_TIMEOUT;https_result=ESP_ERR_TIMEOUT;
 assert(sync_network_time(&utc)==ESP_ERR_TIMEOUT&&power_save==WIFI_PS_MIN_MODEM&&ps_sets==6);
 ntp_result=ESP_ERR_NO_MEM;int before_https=https_calls;
 assert(sync_network_time(&utc)==ESP_ERR_NO_MEM&&https_calls==before_https&&power_save==WIFI_PS_MIN_MODEM&&ps_sets==8);
 ntp_calls=0;get_ps_fail=1;assert(sync_network_time(&utc)==ESP_FAIL&&!ntp_calls&&ps_sets==8);
 get_ps_fail=0;set_ps_fail=1;assert(sync_network_time(&utc)==ESP_FAIL&&!ntp_calls&&power_save==WIFI_PS_MIN_MODEM&&ps_sets==9);
 puts("PASS: actual time transport fallback, WiFi DHCP/disconnect retries and power save restoration on success/failure");
}
'''

rtc=common+r'''
static struct {bool time_ok;unsigned time_synced;uint32_t unix_sec;} state;
static uint8_t regs[9];static int fail_write;
static esp_err_t metalio_read(unsigned addr,unsigned reg,uint8_t *out,size_t n){assert(addr==0x51&&reg+n<=9);memcpy(out,regs+reg,n);return ESP_OK;}
static esp_err_t metalio_write(unsigned addr,unsigned reg,const uint8_t *in,size_t n){assert(addr==0x51&&reg+n<=9);if(fail_write&&reg==2)return ESP_FAIL;memcpy(regs+reg,in,n);return ESP_OK;}
static esp_err_t metalio_write8(unsigned addr,unsigned reg,uint8_t v){return metalio_write(addr,reg,&v,1);}
'''
for name in ('unbcd','bcd','rtc_read','rtc_write'):
    rtc+=function('components/read_pico_pmu/metalio_power_adapter.c',name)
rtc+=r'''
int main(void){
 const uint32_t utc=1791633600;regs[0]=0x22;
 assert(rtc_write(utc)==ESP_OK&&state.time_ok&&state.time_synced&&state.unix_sec==utc&&regs[0]==2);
 regs[0]|=0x20;assert(rtc_read()==ESP_ERR_INVALID_STATE&&!state.time_ok&&!state.time_synced);
 regs[0]=2;regs[2]|=0x80;assert(rtc_read()==ESP_ERR_INVALID_STATE&&!state.time_ok);
 assert(rtc_write(utc)==ESP_OK&&state.unix_sec==utc&&!(regs[2]&0x80));
 fail_write=1;assert(rtc_write(utc)==ESP_FAIL&&regs[0]==2);
 assert(rtc_write(0)==ESP_ERR_INVALID_ARG);
 fail_write=0;assert(rtc_write(utc)==ESP_OK);
 regs[3]=0x1a;assert(rtc_read()==ESP_ERR_INVALID_RESPONSE&&!state.time_ok);
 assert(rtc_write(utc)==ESP_OK);regs[5]=0x31;regs[7]=0x02;assert(rtc_read()==ESP_ERR_INVALID_RESPONSE);
 assert(rtc_write(utc)==ESP_OK);regs[8]=0xfa;assert(rtc_read()==ESP_ERR_INVALID_RESPONSE);
 puts("PASS: production PCF8563 UTC write/read, STOP recovery/control preservation, voltage-low rejection and oscillator resume on failed write");
}
'''
with tempfile.TemporaryDirectory() as tmp:
    for name,code in [('lcd-tail',lcd),('time-network',network),('rtc',rtc)]:
        src=Path(tmp)/(name+'.c');src.write_text(code);exe=Path(tmp)/name
        subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-unknown-attributes',
            '-fsanitize=address,undefined','-g','-I'+str(root/'components/epdiy/src'),str(src),'-o',str(exe)],check=True)
        subprocess.run([str(exe)],check=True)
