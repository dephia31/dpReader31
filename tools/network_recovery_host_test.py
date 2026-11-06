"""中文：验证S31唤醒与错误恢复，不抢占活动网络。
English: Execute S31 wake/error recovery without stealing active network ownership.
"""
from pathlib import Path
import subprocess, tempfile
from home_cover_cache_host_test import function
root=Path(__file__).resolve().parents[1]
source=root/'components/read_pico_transfer/read_pico_transfer.c'
unit=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#define TRANSFER_NETWORK_GUARD_OR_RETURN(...) do { if (!owner) return __VA_ARGS__; } while(0)
#define portENTER_CRITICAL(x) ((void)0)
#define portEXIT_CRITICAL(x) ((void)0)
enum {READ_PICO_TRANSFER_STOPPED,READ_PICO_TRANSFER_ERROR,READ_PICO_TRANSFER_READY};
static struct {int state;} s_status;
static struct {bool network_only;} s_cfg;
static bool s_sleep_blocked,s_sleep_resume_requested,s_started,s_wifi,s_http,s_netif;
static bool s_config_busy,s_time_active,s_upload_active,s_shutdown_only,s_stopping,owner=true,stop_fails;
static int64_t s_sleep_resume_retry,now;
static unsigned stops;
static int64_t esp_timer_get_time(void){return now;}
static void read_pico_transfer_stop(void){++stops;if(stop_fails)return;s_started=s_wifi=s_http=s_netif=s_shutdown_only=false;s_status.state=READ_PICO_TRANSFER_STOPPED;}
'''
for name in ['read_pico_transfer_sleep_blocked','read_pico_transfer_network_shareable','read_pico_transfer_resume_service','read_pico_transfer_prepare_network']:
 unit+=function(name,source)+'\n'
unit+=r'''
static void reset(void){s_sleep_blocked=s_sleep_resume_requested=s_started=s_wifi=s_http=s_netif=s_config_busy=s_time_active=s_upload_active=s_shutdown_only=s_stopping=s_cfg.network_only=stop_fails=false;owner=true;stops=0;now=s_sleep_resume_retry=0;s_status.state=READ_PICO_TRANSFER_STOPPED;}
int main(void){
 reset();s_sleep_blocked=true;assert(!read_pico_transfer_network_shareable());read_pico_transfer_prepare_network();assert(s_sleep_blocked&&!stops);
 s_sleep_resume_requested=true;read_pico_transfer_prepare_network();assert(!s_sleep_blocked&&!s_sleep_resume_requested&&stops==1&&read_pico_transfer_network_shareable());
 reset();s_sleep_blocked=s_sleep_resume_requested=s_wifi=stop_fails=true;read_pico_transfer_prepare_network();assert(s_sleep_blocked&&s_wifi&&stops==1);read_pico_transfer_prepare_network();assert(stops==1);now=5000000;stop_fails=false;read_pico_transfer_prepare_network();assert(!s_sleep_blocked&&!s_wifi&&stops==2);
 reset();s_status.state=READ_PICO_TRANSFER_ERROR;assert(!read_pico_transfer_network_shareable());read_pico_transfer_prepare_network();assert(stops==1&&s_status.state==READ_PICO_TRANSFER_STOPPED&&read_pico_transfer_network_shareable());
 bool *guards[]={&s_started,&s_wifi,&s_http,&s_netif,&s_config_busy,&s_time_active,&s_upload_active};
 for(unsigned i=0;i<sizeof(guards)/sizeof(*guards);++i){reset();s_status.state=READ_PICO_TRANSFER_ERROR;*guards[i]=true;read_pico_transfer_prepare_network();assert(!stops&&s_status.state==READ_PICO_TRANSFER_ERROR);}
 reset();s_status.state=READ_PICO_TRANSFER_READY;s_wifi=true;read_pico_transfer_prepare_network();assert(!stops&&s_wifi);
 reset();s_status.state=READ_PICO_TRANSFER_ERROR;s_wifi=s_netif=s_shutdown_only=true;
 assert(!read_pico_transfer_network_shareable());read_pico_transfer_prepare_network();
 assert(stops==1&&!s_wifi&&!s_netif&&read_pico_transfer_network_shareable());
 reset();s_status.state=READ_PICO_TRANSFER_ERROR;s_wifi=s_stopping=s_cfg.network_only=stop_fails=true;
 read_pico_transfer_prepare_network();assert(stops==1&&s_wifi&&!read_pico_transfer_network_shareable());
 stop_fails=false;read_pico_transfer_prepare_network();assert(stops==2&&!s_wifi&&read_pico_transfer_network_shareable());
 reset();s_status.state=READ_PICO_TRANSFER_ERROR;s_wifi=s_stopping=s_upload_active=true;
 read_pico_transfer_prepare_network();assert(!stops&&s_wifi);
 reset();s_status.state=READ_PICO_TRANSFER_ERROR;s_wifi=s_stopping=true;owner=false;
 read_pico_transfer_prepare_network();assert(!stops&&s_wifi);
 reset();owner=false;s_sleep_blocked=s_sleep_resume_requested=true;read_pico_transfer_prepare_network();assert(!stops&&s_sleep_blocked);
 puts("PASS wake request required, cleanup retries, stale ERROR recovery, interrupted teardown recovered, live resources/config/upload protected, foreign owner protected");
}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp)/'test.c';p.write_text(unit)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(p),'-o',str(p.with_suffix(''))],check=True)
 subprocess.run([str(p.with_suffix(''))],check=True)
for client in ['kosync','opds']:
 f=function(client+'_acquire_network',root/'main/book'/f'{client}_client.c')
 assert f.index('read_pico_transfer_prepare_network();')<f.index('read_pico_transfer_network_shareable()')
for name in ['read_pico_transfer_start','read_pico_transfer_scan_wifi','read_pico_transfer_preempt_for_network','sync_time_run']:
 f=function(name,source)
 assert f.index('read_pico_transfer_prepare_network();')<f.index('if(read_pico_transfer_sleep_blocked())')
assert 's_start_pending = s_scan_pending = s_network_connect_pending = false;' in function('transfer_on_exit',root/'main/apps/app_transfer.c')
for client, path in [('online', 'boards/metalio47/src/main/ota_online.c'),
                     ('ensure_network', 'main/font/online_font.c')]:
    entry = function(client, root/path)
    assert entry.index('read_pico_transfer_prepare_network();') < entry.index('read_pico_transfer_network_shareable()')
stop = function('read_pico_transfer_stop', source)
assert stop.index('s_status.network_ready = false;') < stop.index('httpd_stop(s_http)')
