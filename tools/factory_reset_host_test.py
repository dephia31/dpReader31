#!/usr/bin/env python3
# SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
"""Real per-board reset allowlist with NVS open/erase/commit fault injection."""
from pathlib import Path
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]
unit=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include "kiiko_board_config.h"
typedef int esp_err_t;typedef unsigned nvs_handle_t;
#define ESP_OK 0
#define ESP_ERR_NVS_NOT_FOUND 1
#define NVS_READONLY 0
#define NVS_READWRITE 1
static const char *names[32];static unsigned count,calls,opened,closed,erased,committed,failed_at;static bool missing;
static int fault(void){return ++calls==failed_at?-9:0;}
static int nvs_open(const char *name,int mode,nvs_handle_t *h){
 assert(strcmp(name,"nvs")&&strcmp(name,"metalio_hw")&&strcmp(name,"vcom")&&strcmp(name,"phy")&&strcmp(name,"otadata"));
 int e=fault();if(e)return e;if(missing)return ESP_ERR_NVS_NOT_FOUND;
 unsigned at;for(at=0;at<count&&strcmp(name,names[at]);++at){}if(at==count){assert(mode==NVS_READONLY);names[count++]=name;}
 *h=at;++opened;return 0;
}
static void nvs_close(nvs_handle_t h){assert(h<count);++closed;}
static int nvs_erase_all(nvs_handle_t h){assert(h<count);++erased;return fault();}
static int nvs_commit(nvs_handle_t h){assert(h<count);++committed;return fault();}
'''
source=(ROOT/'main/factory_reset.c').read_text()
unit+='\n'.join(line for line in source.splitlines() if not line.startswith('#include'))
unit+=r'''
static void reset(void){count=calls=opened=closed=erased=committed=failed_at=0;missing=false;}
int main(void){
 reset();assert(app_factory_reset()==0&&opened==closed);unsigned operations=calls,n=count;
 assert(!strcmp(names[0],KIIKO_BOARD_SETTINGS_NAMESPACE)&&!strcmp(names[1],KIIKO_BOARD_STARTUP_NAMESPACE)&&!strcmp(names[n-1],KIIKO_BOARD_LOCK_NAMESPACE));
 assert(erased==n&&committed==n);
 for(unsigned failure=1;failure<=operations;++failure){reset();failed_at=failure;assert(app_factory_reset()==-9&&opened==closed);assert(calls==failure);failed_at=0;assert(app_factory_reset()==0&&opened==closed);}
 reset();missing=true;assert(app_factory_reset()==0&&!opened&&!erased&&!committed);
 puts("PASS: board reset allowlist, credentials last, absent namespaces, all NVS error paths close handles and permit retry; no partition/card/calibration erasure");
}
'''
with tempfile.TemporaryDirectory() as folder:
    p=Path(folder);(p/'test.c').write_text(unit)
    for board in (0,1):
        subprocess.run(['cc','-std=gnu11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-DKIIKO_BOARD_METALIO47='+str(board),'-I'+str(ROOT/'main'),str(p/'test.c'),'-o',str(p/'test')],check=True)
        subprocess.run([str(p/'test')],check=True)
