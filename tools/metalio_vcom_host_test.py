#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Execute real S31 VCOM NVS, key-entry and runtime TPS paths with fault injection."""
from pathlib import Path
import subprocess,tempfile
from home_cover_cache_host_test import function
r=Path(__file__).resolve().parents[1]
u=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef int esp_err_t;typedef int nvs_handle_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_ARG -2
#define ESP_ERR_TIMEOUT -3

#define NVS_READONLY 0
#define NVS_READWRITE 1
#define ESP_LOGE(...) ((void)0)
#define ESP_LOGI(tag,fmt,...) ((void)printf(fmt, __VA_ARGS__))
#define pdMS_TO_TICKS(x) (x)
#define VCOM_MIN_MV 900
#define VCOM_MAX_MV 4000
#define VCOM_DEFAULT_MV 3000
#define VCOM_NVS_NAMESPACE "metalio_hw"
#define VCOM_NVS_KEY "vcom_mv"
static uint16_t saved=1200,staged;
static bool ro_fail,rw_fail,get_fail,set_fail,commit_fail,corrupt;
static unsigned opens,closes,action,commits;
static int applied;
static esp_err_t nvs_open(const char*ns,int mode,nvs_handle_t*h){assert(!strcmp(ns,VCOM_NVS_NAMESPACE));if((mode==0&&ro_fail)||(mode==1&&rw_fail))return ESP_FAIL;*h=mode+1;++opens;return ESP_OK;}
static esp_err_t nvs_get_u16(nvs_handle_t h,const char*k,uint16_t*v){assert(h==1&&!strcmp(k,VCOM_NVS_KEY));if(get_fail)return ESP_FAIL;*v=saved+(corrupt?10:0);return ESP_OK;}
static esp_err_t nvs_set_u16(nvs_handle_t h,const char*k,uint16_t v){assert(h==2&&!strcmp(k,VCOM_NVS_KEY));if(set_fail)return ESP_FAIL;staged=v;return ESP_OK;}
static esp_err_t nvs_commit(nvs_handle_t h){assert(h==2);if(commit_fail)return ESP_FAIL;saved=staged;++commits;return ESP_OK;}
static void nvs_close(nvs_handle_t h){assert(h==1||h==2);++closes;}
static void epd_set_vcom(uint16_t mv){assert(!action++);applied=mv;}
static void epd_poweroff(void){assert(action++==1);}
static void display_request_full_refresh(void){assert(action++==2);}
static unsigned key_reads;static int key_error=-1;static uint16_t key_inputs[2];
static esp_err_t metalio_io_read(uint16_t*v){unsigned at=key_reads++;assert(at<2);if((int)at==key_error)return ESP_FAIL;*v=key_inputs[at];return ESP_OK;}
static void vTaskDelay(int ms){assert(ms==30);}
static int vcom=3000;
'''
for name in ['vcom_setup_load_mv','persist','vcom_typed_mv','vcom_digits_valid','vcom_push_digit','vcom_pop_digit']:
 u+=function(name,r/'main/factory/vcom_setup.c')+'\n'
u+=function('volume_key_held',r/'main/app_main.c')+'\n'
u+=function('set_vcom',r/'components/read_pico/read_pico_board.c')+'\n'
u+=r'''
static void reset(void){saved=1200;staged=0;ro_fail=rw_fail=get_fail=set_fail=commit_fail=corrupt=false;opens=closes=action=commits=0;applied=0;}
int main(void){
 int valid[]={900,1200,3000,4000};for(unsigned i=0;i<4;i++){reset();saved=valid[i];assert(vcom_setup_load_mv()==valid[i]&&opens==closes);}
 int invalid[]={0,899,901,2999,4001,65535};for(unsigned i=0;i<6;i++){reset();saved=invalid[i];assert(vcom_setup_load_mv()==3000&&opens==closes);assert(!persist(invalid[i])&&!action&&!commits);}
 reset();ro_fail=true;assert(vcom_setup_load_mv()==3000&&!opens);reset();get_fail=true;assert(vcom_setup_load_mv()==3000&&opens==closes);
 for(unsigned i=0;i<4;i++){reset();assert(persist(valid[i])&&saved==valid[i]&&applied==valid[i]&&action==3&&commits==1&&opens==closes);assert(vcom_setup_load_mv()==valid[i]);}
 for(unsigned fault=0;fault<6;fault++){reset();rw_fail=fault==0;set_fail=fault==1;commit_fail=fault==2;ro_fail=fault==3;get_fail=fault==4;corrupt=fault==5;assert(!persist(3000));assert(!action&&opens==closes);if(fault<3)assert(saved==1200);}
 const char*good[]={"90","090","120","300","400"};int want[]={900,900,1200,3000,4000};for(unsigned i=0;i<5;i++){int n=-1;assert(vcom_digits_valid(good[i],&n)&&n==want[i]);}
 const char*bad[]={NULL,"","089","401","000","3000","-90","9.0","9a0"};for(unsigned i=0;i<9;i++)assert(!vcom_digits_valid(bad[i],NULL));
 char d[4]="";assert(!vcom_pop_digit(d));assert(vcom_push_digit(d,3)&&vcom_push_digit(d,0)&&vcom_push_digit(d,0)&&!vcom_push_digit(d,1));assert(!strcmp(d,"300"));assert(vcom_pop_digit(d)&&!strcmp(d,"30"));
 for(unsigned a=0;a<4;a++)for(unsigned b=0;b<4;b++){key_reads=0;key_error=-1;key_inputs[0]=0xfcff|(a<<8);key_inputs[1]=0xfcff|(b<<8);assert(volume_key_held()==(a!=3&&b!=3));}
 for(int error=0;error<2;error++){key_reads=0;key_error=error;assert(!volume_key_held());}
 set_vcom(600);assert(vcom==600);set_vcom(3000);assert(vcom==3000);set_vcom(4000);assert(vcom==4000);set_vcom(4001);assert(vcom==4000);set_vcom(599);assert(vcom==4000);
 puts("PASS: actual VCOM defaults/range/digits/NVS failures and exact readback, rail-cycle order, volume boot admission and board clamp");}
'''
# Compile independently to exercise the actual runtime register path, not a model.
t=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_ERR_INVALID_ARG -2
#define ESP_ERR_TIMEOUT -3
static const char* esp_err_to_name(int err){(void)err;return "error";}
#define ESP_LOGI(tag,fmt,...) ((void)printf(fmt, __VA_ARGS__))
#define ESP_LOGE(...) ((void)0)
#define pdMS_TO_TICKS(x) (x)
static bool tps_on,pg_good=true,io_good=true;static unsigned output_bits,pg_seen;static int fail_reg=-1;static unsigned off,writes;static uint8_t regs[256];
static int metalio_io_set(unsigned pin,bool high){assert(pin<3);if(high)output_bits|=1u<<pin;else output_bits&=~(1u<<pin);if(pin==0&&!high)pg_seen=0;return 0;}
static int metalio_io_mask(unsigned port,unsigned mask,unsigned value){assert(port==0&&mask==3&&value==3&&(output_bits&4));output_bits|=3;return 0;}
static int tps_last_vcom_mv=-1;
static void vTaskDelay(int ms){assert(ms==50||ms==10||ms==5);}
static int metalio_read(unsigned addr,unsigned reg,uint8_t*v,unsigned n){assert(addr==0x68&&n==1&&(reg==0x10||reg==0x0f||reg==3||reg==4));*v=reg==0x10?0x65:reg==0x0f?(pg_good?0xfa:0):regs[reg];return 0;}
static int metalio_io_read(uint16_t*v){assert((output_bits&7)==7);*v=io_good?0x80:0;if(io_good)pg_seen=1;return 0;}
static int metalio_write8(unsigned addr,unsigned reg,unsigned value){assert(addr==0x68);assert(reg==0x09||reg==0x0a||reg==0x01||reg==0x04||reg==0x03);if(reg==1)assert(pg_seen&&(output_bits&7)==7);if(reg==3||reg==4)assert(regs[1]==0x3f&&pg_seen);if(reg==0x04)assert(value<=1);if((int)reg==fail_reg)return -1;regs[reg]=value;++writes;return 0;}
static void metalio_tps_off(void){++off;tps_on=false;}
'''+function('metalio_tps_vcom_mv',r/'components/metalio_hw/metalio_hw.c')+'\n'+function('metalio_tps_on',r/'components/metalio_hw/metalio_hw.c')+r'''
int main(void){int values[]={900,1200,3000,4000};for(unsigned i=0;i<4;i++){tps_on=false;writes=0;assert(!metalio_tps_on(values[i])&&tps_on&&writes==5);assert(regs[4]==((values[i]/10)>>8)&&regs[3]==((values[i]/10)&255));assert(!metalio_tps_on(1200)&&writes==5);}
 tps_on=false;off=writes=0;assert(metalio_tps_on(-1)==ESP_ERR_INVALID_ARG&&!writes);assert(metalio_tps_on(5111)==ESP_ERR_INVALID_ARG&&!writes);
 int fail[]={9,10,1,4,3};for(unsigned i=0;i<5;i++){tps_on=false;off=0;fail_reg=fail[i];assert(metalio_tps_on(3000)!=ESP_OK&&!tps_on&&off==1);}fail_reg=-1;
 pg_good=false;tps_on=false;off=0;assert(metalio_tps_on(3000)==ESP_ERR_TIMEOUT&&off==1&&!tps_on);pg_good=true;io_good=false;off=0;assert(metalio_tps_on(3000)==ESP_ERR_TIMEOUT&&off==1&&!tps_on);
 puts("PASS: actual TPS65185 runtime VCOM register encoding 900/1200/3000/4000, no OTP command, PG/write faults shut rails off");}
'''
with tempfile.TemporaryDirectory() as d:
 d=Path(d)
 for name,src in [('nvs',u),('tps',t)]:
  (d/(name+'.c')).write_text(src)
  subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(d/(name+'.c')),'-o',str(d/name)],check=True)
  subprocess.run([str(d/name)],check=True)
