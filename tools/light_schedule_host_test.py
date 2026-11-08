#!/usr/bin/env python3
"""Execute the real daily scheduler and frontlight PWM with RTC/NVS faults."""
from pathlib import Path
import subprocess, tempfile
from home_cover_cache_host_test import function
root=Path(__file__).resolve().parents[1]
hw=root/'components/metalio_hw/metalio_hw.c'
u=r'''
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include "metalio_light_schedule.h"
#include "nvs.h"
static uint8_t disk[15],staged[15];static bool present;static int fail_open,fail_set,fail_commit,fail_read,opens,writes,commits;
int nvs_open(const char*n,int mode,int*h){assert(!strcmp(n,"metalio_light"));++opens;if(fail_open)return ESP_FAIL;*h=1;if(mode==NVS_READWRITE)memcpy(staged,disk,15);return ESP_OK;}
int nvs_get_blob(int h,const char*k,void*b,size_t*n){assert(h==1&&!strcmp(k,"schedule"));if(fail_read)return ESP_FAIL;if(!present)return ESP_ERR_NVS_NOT_FOUND;assert(*n==15);memcpy(b,disk,15);*n=15;return ESP_OK;}
int nvs_set_blob(int h,const char*k,const void*b,size_t n){assert(h==1&&!strcmp(k,"schedule")&&n==15);++writes;if(fail_set)return ESP_FAIL;memcpy(staged,b,15);return ESP_OK;}
int nvs_commit(int h){assert(h==1);++commits;if(fail_commit)return ESP_FAIL;memcpy(disk,staged,15);present=true;return ESP_OK;}
void nvs_close(int h){assert(h==1);}
#include "metalio_light_schedule.c"
#define METALIO_FRONTLIGHT_RANGE 240
#define LEDC_LOW_SPEED_MODE 0
#define LEDC_CHANNEL_0 0
#define LEDC_CHANNEL_1 1
static bool fl_ready=true,light_enabled,light_dirty,light_suspended;
static uint8_t saved_brightness=30,saved_warmth=50;
static uint16_t brightness_position=72,warmth_position=120,last_on_position=72;
static time_t light_schedule_checked,wall;
static unsigned duties[2],pwm_updates,manual_commits;
static time_t fake_time(void*p){(void)p;return wall;}
#define time fake_time
static int ledc_stop(int m,int c,int idle){assert(!m&&!idle);duties[c]=0;return 0;}
static int ledc_set_duty(int m,int c,unsigned d){assert(!m&&d<=511);duties[c]=d;return 0;}
static int ledc_update_duty(int m,int c){assert(!m);(void)c;++pwm_updates;return 0;}
void metalio_frontlight_preview_position(unsigned,unsigned);
static void metalio_frontlight_commit(void){if(light_dirty){++manual_commits;light_dirty=false;}}
'''
for name in ['metalio_frontlight','metalio_brightness','metalio_warmth','metalio_frontlight_enabled','frontlight_apply_position','frontlight_schedule_update','metalio_frontlight_schedule_poll','metalio_frontlight_restore','metalio_frontlight_preview_position','metalio_frontlight_adjust_position','metalio_frontlight_toggle']:
    u+=function(name,hw)+'\n'
u+=r'''
static time_t stamp(int day,int minute){return (time_t)1704067200+day*86400+minute*60-7*3600;}
static void reset(void){
 present=loaded=load_attempted=false;load_error=0;memset(disk,0,15);memset(seen,0,sizeof(seen));
 schedules[0]=(metalio_light_schedule_t){false,420,1080,30};schedules[1]=(metalio_light_schedule_t){false,1080,1380,30};
 fail_open=fail_set=fail_commit=fail_read=opens=writes=commits=0;light_suspended=light_enabled=light_dirty=false;
 brightness_position=72;warmth_position=120;duties[0]=duties[1]=0;wall=stamp(10,0);light_schedule_checked=0;
}
static void schedule(bool warm,int on,int off,int level){metalio_light_schedule_t s={true,on,off,level};assert(metalio_light_schedule_save(warm,&s)==ESP_OK);}
static void tick(int day,int minute){wall=stamp(day,minute);metalio_frontlight_schedule_poll();assert(duties[0]+duties[1]<=511);}
int main(void){
 reset();assert(!metalio_light_schedule_get(false).enabled&&!metalio_light_schedule_get(true).enabled);
 schedule(false,420,1080,40);schedule(true,1080,1380,30);
 tick(10,419);assert(!duties[0]&&!duties[1]);tick(10,420);assert(duties[0]&&!duties[1]);
 unsigned count=pwm_updates;int flash=writes;tick(10,421);assert(pwm_updates==count&&writes==flash); // no repeated output/flash
 tick(10,1080);assert(!duties[0]&&duties[1]);tick(10,1380);assert(!duties[0]&&!duties[1]);
 reset();schedule(true,22*60,6*60,50);tick(10,21*60+59);assert(!duties[1]);tick(10,22*60);assert(duties[1]&&!duties[0]);
 count=pwm_updates;tick(11,0);assert(duties[1]&&pwm_updates==count);tick(11,6*60);assert(!duties[1]);
 // Manual override persists within the same phase, until the next scheduled edge.
 reset();schedule(false,420,1080,50);tick(10,500);assert(duties[0]);metalio_frontlight_toggle();assert(!duties[0]&&!duties[1]);
 tick(10,501);assert(!duties[0]);tick(10,1080);assert(!duties[0]);tick(11,420);assert(duties[0]);
 // Sleep remains dark across clock changes/maintenance; explicit wake reapplies current interval.
 metalio_frontlight(0,50);assert(!duties[0]&&!duties[1]);tick(11,500);assert(!duties[0]&&!duties[1]);metalio_frontlight_restore();assert(duties[0]);
 metalio_frontlight(0,50);tick(11,1200);assert(!duties[0]);metalio_frontlight_restore();assert(!duties[0]);
 // Both channels ON never exceed the established total PWM ceiling.
 reset();schedule(false,0,1439,100);schedule(true,0,1439,100);tick(10,500);assert(duties[0]&&duties[1]&&duties[0]+duties[1]<=511);
 for(int minute=0;minute<1440;minute++)tick(11,minute);
 reset();schedule(false,0,600,100);schedule(true,0,1200,100);tick(10,500);assert(duties[0]&&duties[1]);
 tick(10,600);assert(!duties[0]&&duties[1]==511); // Recover full warm level after the shared clamp.

 // Skipped boundaries, clock jumps and restored RTC apply the current phase, not a backlog.
 reset();schedule(false,420,1080,30);wall=0;metalio_frontlight_restore();assert(!duties[0]);tick(10,500);assert(duties[0]);tick(10,1100);assert(!duties[0]);tick(10,500);assert(duties[0]);
 // Both channels round-trip through the settings archive format.
 reset();schedule(false,360,720,65);schedule(true,1320,300,35);
 uint8_t archive[METALIO_LIGHT_SCHEDULE_BYTES];
 metalio_light_schedule_t restored[2];
 assert(metalio_light_schedule_export(archive)==ESP_OK);
 assert(metalio_light_schedule_decode(archive,restored));
 assert(restored[0].enabled&&restored[0].on_minute==360&&restored[0].off_minute==720&&restored[0].level==65);
 assert(restored[1].enabled&&restored[1].on_minute==1320&&restored[1].off_minute==300&&restored[1].level==35);
 archive[14]=1;assert(!metalio_light_schedule_decode(archive,restored));archive[14]=0;
 assert(metalio_light_schedule_decode(archive,restored));
 reset();memcpy(disk,archive,sizeof(disk));present=true;metalio_light_schedule_adopt(restored);
 tick(10,500);assert(duties[0]&&!duties[1]);tick(10,1330);assert(!duties[0]&&duties[1]);
 // Failed saves retain runtime config. Load validates stored version/values before enabling.
 metalio_light_schedule_t prior=metalio_light_schedule_get(false),bad=prior;bad.on_minute=bad.off_minute;
 assert(metalio_light_schedule_save(false,&bad)==ESP_ERR_INVALID_ARG);bad=prior;bad.level=0;assert(metalio_light_schedule_save(false,&bad)==ESP_ERR_INVALID_ARG);
 bad=prior;bad.off_minute=1500;assert(metalio_light_schedule_save(false,&bad)==ESP_ERR_INVALID_ARG);
 bad=prior;bad.level=80;
 fail_open=1;assert(metalio_light_schedule_save(false,&bad)!=ESP_OK);fail_open=0;
 fail_set=1;assert(metalio_light_schedule_save(false,&bad)!=ESP_OK);fail_set=0;
 fail_commit=1;assert(metalio_light_schedule_save(false,&bad)!=ESP_OK);fail_commit=0;
 assert(metalio_light_schedule_get(false).level==prior.level);
 loaded=false;schedules[0].enabled=false;assert(metalio_light_schedule_get(false).enabled&&metalio_light_schedule_get(false).level==prior.level);
 disk[0]=255;loaded=false;schedules[0].enabled=false;assert(!metalio_light_schedule_get(false).enabled);
 // A failed read cannot authorize saving defaults over the peer channel.
 reset();schedule(false,420,1080,80);loaded=false;load_attempted=false;
 schedules[0]=(metalio_light_schedule_t){false,420,1080,30};fail_open=1;
 unsigned white=0,warm=0;int reads_before=opens;for(int i=0;i<30;i++)assert(!metalio_light_schedule_apply(wall,&white,&warm));assert(opens==reads_before+1);
 metalio_light_schedule_t edit={true,1200,360,40};int before=writes;
 assert(metalio_light_schedule_save(true,&edit)!=ESP_OK&&writes==before&&disk[1]==1);
 fail_open=0;assert(metalio_light_schedule_save(true,&edit)==ESP_OK&&disk[1]==1&&schedules[0].level==80);
 loaded=false;load_attempted=false;fail_read=1;before=writes;
 assert(metalio_light_schedule_save(true,&edit)!=ESP_OK&&writes==before);fail_read=0;
 assert(metalio_light_schedule_get(false).enabled&&schedules[0].level==80);
 disk[0]=255;loaded=false;load_attempted=false;before=writes;
 assert(metalio_light_schedule_save(true,&edit)!=ESP_OK&&writes==before); // Unknown/corrupt formats stay untouched.
 puts("PASS actual daily channels: boundaries/midnight/manual override/Sleep maintenance/wake/clock jumps/no periodic PWM or flash/total power cap/NVS faults and reboot");
}
'''
with tempfile.TemporaryDirectory() as t:
    p=Path(t)
    (p/'esp_err.h').write_text('#pragma once\ntypedef int esp_err_t;\n#define ESP_OK 0\n#define ESP_FAIL -1\n#define ESP_ERR_INVALID_ARG 2\n#define ESP_ERR_INVALID_STATE 3\n#define ESP_ERR_NVS_NOT_FOUND 4\n')
    (p/'nvs.h').write_text('#pragma once\n#include <stddef.h>\ntypedef int nvs_handle_t;\n#define NVS_READONLY 0\n#define NVS_READWRITE 1\nint nvs_open(const char*,int,int*);int nvs_get_blob(int,const char*,void*,size_t*);int nvs_set_blob(int,const char*,const void*,size_t);int nvs_commit(int);void nvs_close(int);\n')
    (p/'test.c').write_text(u)
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined','-I'+str(p),'-I'+str(root/'components/metalio_hw/include'),'-I'+str(root/'components/metalio_hw'),str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
