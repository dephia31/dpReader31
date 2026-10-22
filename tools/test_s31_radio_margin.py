#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""中文：临时对时不在上传状态中，仍须保留供屏余量。/ English: Temporary sync must retain display margin even with upload state stopped."""
from pathlib import Path
import subprocess,tempfile
from home_cover_cache_host_test import function
r=Path(__file__).resolve().parents[1]
unit=r'''
#include <stdbool.h>
#include <assert.h>
#include <stdio.h>
static int s_lock,depth;
static bool s_config_busy,ble;
static struct{int state;}s_status;
#define READ_PICO_TRANSFER_STOPPED 0
#define portENTER_CRITICAL(p) do{assert((p)==&s_lock&&!depth);++depth;}while(0)
#define portEXIT_CRITICAL(p) do{assert((p)==&s_lock&&depth==1);--depth;}while(0)
static bool ble_pt_running(void){assert(!depth);return ble;}
'''+function('read_pico_transfer_radio_busy',r/'components/read_pico_transfer/read_pico_transfer.c')+r'''
int main(void){
 assert(!read_pico_transfer_radio_busy());
 s_config_busy=true;assert(read_pico_transfer_radio_busy());s_config_busy=false;
 s_status.state=2;assert(read_pico_transfer_radio_busy());s_status.state=0;
 ble=true;assert(read_pico_transfer_radio_busy());ble=false;
 assert(!read_pico_transfer_radio_busy()&&!depth);
 puts("PASS: actual radio snapshot protects temporary sync/configuration with stopped transfer, active transfer and BLE; returns idle after release and queries BLE outside the lock");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(unit)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
