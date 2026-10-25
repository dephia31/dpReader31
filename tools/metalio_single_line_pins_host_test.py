#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
"""中文：单线唤醒释放不用的D1～D3，GPIO失败不继续改变专用PAD。
English: Release unusedD1–D3 before single-line wake; failed GPIO setup must not mutate dedicated pads.
"""
from pathlib import Path
import subprocess,tempfile
from home_cover_cache_host_test import function
R=Path(__file__).resolve().parents[1]
unit=r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
typedef int esp_err_t;
#define ESP_OK 0
#define GPIO_MODE_INPUT 1
#define GPIO_PULLUP_ENABLE 1
#define GPIO_PULLDOWN_DISABLE 0
#define GPIO_INTR_DISABLE 0
typedef struct {uint64_t pin_bit_mask;int mode,pull_up_en,pull_down_en,intr_type;}gpio_config_t;
static int error,calls;
static int gpio_config(const gpio_config_t *c){
 assert(c->pin_bit_mask==((1ULL<<21)|(1ULL<<22)|(1ULL<<23))&&c->mode==GPIO_MODE_INPUT);
 assert(c->pull_up_en==GPIO_PULLUP_ENABLE&&c->pull_down_en==GPIO_PULLDOWN_DISABLE&&c->intr_type==GPIO_INTR_DISABLE);
 ++calls;return error;
}
static struct {
 struct{int sdio_data1_fun_wpd,sdio_data1_fun_wpu;}sdio_data1;
 struct{int sdio_data2_fun_wpd,sdio_data2_fun_wpu;}sdio_data2;
 struct{int sdio_data3_fun_wpd,sdio_data3_fun_wpu;}sdio_data3;
}CNNT_PAD_CTRL;
'''+function('metalio_sd_single_line_idle',R/'components/metalio_hw/metalio_hw.c')+r'''
int main(void){
 CNNT_PAD_CTRL.sdio_data1.sdio_data1_fun_wpd=CNNT_PAD_CTRL.sdio_data2.sdio_data2_fun_wpd=CNNT_PAD_CTRL.sdio_data3.sdio_data3_fun_wpd=1;
 error=-1;assert(metalio_sd_single_line_idle()==-1&&calls==1);
 assert(CNNT_PAD_CTRL.sdio_data1.sdio_data1_fun_wpd&&CNNT_PAD_CTRL.sdio_data2.sdio_data2_fun_wpd&&CNNT_PAD_CTRL.sdio_data3.sdio_data3_fun_wpd);
 error=0;assert(metalio_sd_single_line_idle()==0&&calls==2);
 assert(!CNNT_PAD_CTRL.sdio_data1.sdio_data1_fun_wpd&&!CNNT_PAD_CTRL.sdio_data2.sdio_data2_fun_wpd&&!CNNT_PAD_CTRL.sdio_data3.sdio_data3_fun_wpd);
 assert(CNNT_PAD_CTRL.sdio_data1.sdio_data1_fun_wpu&&CNNT_PAD_CTRL.sdio_data2.sdio_data2_fun_wpu&&CNNT_PAD_CTRL.sdio_data3.sdio_data3_fun_wpu);
 assert(metalio_sd_single_line_idle()==0);
 puts("PASS: actual S31 unusedD1-D3 released from sleep-low to input/pull-up before1-bit use, GPIO failure preserves PADs, repeated calls safe");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(unit)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
