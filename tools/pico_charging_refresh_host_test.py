#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
"""中文：真实PMU轮询的充电位/状态枚举不同步时，百分比仍增长。/ English: Real PMU polling must track charging even when its state enum lags."""
from pathlib import Path
import re,subprocess,tempfile
from home_cover_cache_host_test import function
root=Path(__file__).resolve().parents[1]
source=root/'components/read_pico_pmu/read_pico_pmu.c'
unit=r'''
#include "read_pico_pmu.h"
#include "pico_soc_filter.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#define ESP_OK 0
#define ESP_ERR_INVALID_STATE -1
#define pdMS_TO_TICKS(x) (x)
static pmu_snapshot_t s_snap;
static pico_soc_filter_t s_soc_filter;
static void *s_dev=(void*)1;
static uint32_t clock_ms;
static uint16_t soc=900;
static bool charging,quick_bad;
static int64_t esp_timer_get_time(void){return (int64_t)clock_ms*1000;}
static void vTaskDelay(unsigned ms){clock_ms+=ms;}
'''
for name in ['pmu_crc16_ccitt_false','rd16','rd32','crc_ok','parse_status','parse_quick']:
 unit+=function(name,source)+'\n'
unit+=r'''
static bool parse_identity(const uint8_t *p){(void)p;return true;}
static void parse_event(const uint8_t *p){(void)p;}
static void parse_diag(const uint8_t *p){(void)p;}
esp_err_t read_pico_pmu_cmd(uint16_t c,const uint8_t*p,uint8_t n){(void)c;(void)p;(void)n;return 0;}
static esp_err_t pmu_read_reg(uint8_t reg,uint8_t *out,size_t n){
 memset(out,0,n);
 if(reg==PMU_REG_STATUS){
  uint32_t flags=PMU_STATUS_BATTERY_VALID|(charging?PMU_STATUS_CHARGING_ACTIVE:0);
  out[4]=flags;out[5]=flags>>8;out[6]=flags>>16;out[7]=flags>>24;
  out[8]=4100&255;out[9]=4100>>8;out[12]=soc&255;out[13]=soc>>8;
  out[16]=PMU_CHARGE_NOT_CHARGING;
 } else if(reg==PMU_REG_QUICK_BATTERY){
  out[0]=4100&255;out[1]=4100>>8;out[2]=soc&255;out[3]=soc>>8;
  out[4]=PMU_CHARGE_NOT_CHARGING;out[5]=3|(charging?4:0);
 }
 if(reg==PMU_REG_STATUS||reg==PMU_REG_QUICK_BATTERY){unsigned crc=pmu_crc16_ccitt_false(out,n-2);out[n-2]=crc;out[n-1]=crc>>8;}
 if(reg==PMU_REG_QUICK_BATTERY&&quick_bad)out[n-1]^=1;
 return 0;
}
'''
unit+=function('refresh_core',source)+'\n'
unit+=r'''
int main(void){
 assert(refresh_core(false)==0);assert(pmu_battery_percent(&s_snap)==90&&!pmu_battery_charging(&s_snap));
 charging=true;soc=915;clock_ms=1000;
 assert(refresh_core(false)==0);assert(pmu_battery_percent(&s_snap)==92&&pmu_battery_charging(&s_snap));
 soc=930;clock_ms=2000;assert(refresh_core(false)==0);assert(pmu_battery_percent(&s_snap)==93);
 // CRC失败时回退到有效STATUS的同一个充电位。/ CRC failure uses the valid STATUS charging bit.
 quick_bad=true;soc=945;clock_ms=3000;assert(refresh_core(false)==0);assert(pmu_battery_percent(&s_snap)>=93&&pmu_battery_charging(&s_snap));
 clock_ms=4000;assert(refresh_core(false)==0);assert(pmu_battery_percent(&s_snap)==95&&pmu_battery_charging(&s_snap));
 charging=false;soc=900;clock_ms=5000;assert(refresh_core(false)==0);assert(pmu_battery_percent(&s_snap)==95&&!pmu_battery_charging(&s_snap));
 puts("PASS: real refresh_core charging percentage grows with active flags despite a lagging enum; valid STATUS fallback and unplug smoothing retained");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'driver').mkdir();(p/'driver/i2c_master.h').write_text('#pragma once\ntypedef void *i2c_master_bus_handle_t;\n')
 (p/'esp_err.h').write_text('#pragma once\ntypedef int esp_err_t;\n');(p/'test.c').write_text(unit)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I'+d,'-I'+str(root/'components/read_pico_pmu/include'),str(p/'test.c'),str(root/'components/read_pico_pmu/pico_soc_filter.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
