#!/usr/bin/env python3
"""Execute actual player/cleanup/cancel paths with hardware boundary substitutes."""
from pathlib import Path
import subprocess,tempfile,wave,struct
from home_cover_cache_host_test import function
R=Path(__file__).resolve().parents[1];source=R/'components/metalio_audio/metalio_audio.c'
unit=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "metalio_wav.h"
enum {ESP_OK,ESP_ERR_INVALID_ARG,ESP_ERR_NO_MEM,ESP_ERR_INVALID_STATE};
typedef int esp_err_t;
typedef enum {METALIO_AUDIO_IDLE,METALIO_AUDIO_STARTING,METALIO_AUDIO_PLAYING,METALIO_AUDIO_DONE,METALIO_AUDIO_ERROR} metalio_audio_state_t;
#define pdPASS 1
#define portMAX_DELAY 100000
static atomic_int state;
static atomic_bool cancel;
static int token;static int *done;
static bool pending,cleanup_failed,uart_ready;static void *tx;
static char path[320];static _Atomic(const char*)message;
static bool pa,mode_ok=true,out_ok=true,alloc_ok=true,task_ok=true;
static unsigned writes,releases,pa_on,deleted;
static int fail_write;
static void (*queued)(void*);
static void *xSemaphoreCreateBinary(void){return alloc_ok?&token:NULL;}
static int xSemaphoreTake(int *s,int ticks){assert(s==&token&&ticks==portMAX_DELAY);if(queued){void(*f)(void*)=queued;queued=NULL;f(NULL);}assert(token);token=0;return 1;}
static void xSemaphoreGive(int *s){assert(s==&token&&!token);token=1;}
static void vTaskDelete(void *p){assert(!p);++deleted;}
static int xTaskCreate(void(*f)(void*),const char*n,int stack,void*a,int priority,void *out){(void)n;assert(stack==6144&&!a&&priority==4&&!out);if(!task_ok)return 0;queued=f;return 1;}
static int metalio_io_set(int pin,bool on){assert(pin==4);pa=on;if(on)++pa_on;return ESP_OK;}
static bool mode_ready(void){return mode_ok&&!atomic_load(&cancel);}
static bool output_ready(void){return out_ok;}
static bool release_output(void){++releases;pa=false;return true;}
static bool write_slots(const int32_t *p,size_t n){assert(p&&n<=2048&&n%8==0);++writes;return writes!=(unsigned)fail_write&&!atomic_load(&cancel);}
'''
for name in ('status','metalio_audio_state','metalio_audio_message','player','metalio_audio_stop','metalio_audio_play_wav'):
 unit+=function(name,source)+'\n'
unit+=r'''
static void run(void){assert(queued);void(*f)(void*)=queued;queued=NULL;f(NULL);}
int main(int argc,char **argv){
 assert(argc==2);snprintf(path,sizeof(path),"%s",argv[1]);
 done=&token;
 for(int scenario=0;scenario<5;++scenario){
  token=0;atomic_store(&cancel,false);mode_ok=scenario!=1;out_ok=scenario!=2;writes=0;fail_write=scenario==3?2:0;
  unsigned before=releases;player(NULL);assert(releases==before+1&&!pa&&token);
  assert(metalio_audio_state()==(scenario==1||scenario==2||scenario==3?METALIO_AUDIO_ERROR:METALIO_AUDIO_DONE));
  if(scenario==1||scenario==2)assert(!writes);
 }
 token=0;pending=false;queued=NULL;
 assert(metalio_audio_play_wav("bad")==ESP_ERR_INVALID_ARG);
 assert(metalio_audio_play_wav("/sdcard/../bad")==ESP_ERR_INVALID_ARG);
 assert(metalio_audio_play_wav("/sdcard/test.wav")==ESP_OK&&pending&&queued);
 // Replace only the backing path, then execute an asynchronous successful completion.
 strcpy(path,argv[1]);mode_ok=out_ok=true;fail_write=0;writes=0;run();assert(token&&!pa);
 metalio_audio_stop();assert(!pending&&!token);
 assert(metalio_audio_play_wav("/sdcard/test.wav")==ESP_OK);strcpy(path,argv[1]);
 unsigned before=pa_on;metalio_audio_stop();assert(!pending&&!pa&&pa_on==before&&metalio_audio_state()==METALIO_AUDIO_IDLE);
 task_ok=false;assert(metalio_audio_play_wav("/sdcard/test.wav")==ESP_ERR_NO_MEM&&!pending);
 done=NULL;alloc_ok=false;assert(metalio_audio_play_wav("/sdcard/test.wav")==ESP_ERR_NO_MEM&&!pending);
 puts("PASS actual player lifecycle: finish, BH/I2S/write errors, PAoff/resourcecleanup, cancel+join, restart, invalidpaths, task/semaphoreOOM");
}
'''
# Also execute serial framing and partial-write loops with boundary substitutes.
serial=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdatomic.h>
#include <string.h>
#include <stdio.h>
#include "metalio_wav.h"
#define UART_NUM_1 1
#define pdMS_TO_TICKS(x) (x)
static atomic_bool cancel;
static const char *reply;static size_t offset,fragment=3;static int64_t now;
static int64_t esp_timer_get_time(void){now+=1000;return now;}
static int uart_read_bytes(int port,void *out,size_t n,int ticks){assert(port==1&&ticks==20);size_t left=strlen(reply)-offset;if(n>fragment)n=fragment;if(n>left)n=left;memcpy(out,reply+offset,n);offset+=n;return (int)n;}
'''+function('read_reply',source)+r'''
int main(void){
 const char*good[]={"SET MODE 1\r\n","OK SET MODE 1 OK\n","junk\nSET MODE 1\n"};
 for(unsigned i=0;i<3;++i){reply=good[i];offset=0;now=0;assert(read_reply(3000,true));}
 const char*bad[]={"SET MODE 10\n","ERROR\nSET MODE 1\n","FAIL\n","SET MODE 1","RESET MODE 1\n"};
 for(unsigned i=0;i<5;++i){reply=bad[i];offset=0;now=0;assert(!read_reply(3000,true));}
 reply="ERROR\n";offset=0;now=0;assert(!read_reply(100,false));
 reply="OK\n";offset=0;now=0;assert(read_reply(100,false));
 char overflow[256];memset(overflow,'X',sizeof(overflow));strcpy(overflow+220," SET MODE 1\n");reply=overflow;offset=0;now=0;assert(!read_reply(3000,true));
 atomic_store(&cancel,true);offset=0;now=0;reply="SET MODE 1\n";assert(!read_reply(3000,true));
 puts("PASS actual UART parser: fragmentedACK, error/timeout/incomplete/collision/overflow/cancel");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);f=p/'test.wav'
 with wave.open(str(f),'wb') as w:w.setnchannels(1);w.setsampwidth(2);w.setframerate(16000);w.writeframes(struct.pack('<1024h',*range(1024)))
 for name,code in [('lifecycle',unit),('serial',serial)]:
  (p/f'{name}.c').write_text(code)
  subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined','-I'+str(R/'components/metalio_audio/include'),str(p/f'{name}.c'),str(R/'components/metalio_audio/metalio_wav.c'),'-o',str(p/name)],check=True)
  subprocess.run([str(p/name),str(f)],check=True)
