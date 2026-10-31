"""Run production RTC crash-report queue and real filesystem commit with injected faults."""
from pathlib import Path
import re,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
source=(root/'main/app/crash_report.c').read_text()
source=re.sub(r'^#include "[^"]+"\n','',source,flags=re.M).replace('#define REPORT_DIR "/sdcard/crash_log"','#define REPORT_DIR "crash_log"')
prefix=r'''
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <errno.h>
#define RTC_NOINIT_ATTR
#define RP_CRASH_LOG_LINES 24
#define ESP_LOGW(...) ((void)0)
#define ESP_LOGI(...) ((void)0)
enum {ESP_RST_POWERON=1,ESP_RST_SW=3,ESP_RST_PANIC=4,ESP_RST_PWR_GLITCH=14};
static int reset_reason=ESP_RST_POWERON;static int64_t clock_us;
static bool ready,panic_valid=true;static int fault;
static struct {char version[32];} desc={"1.2.4RC2-vn"};
static const void *esp_app_get_description_placeholder;
#define esp_app_get_description() (&desc)
static int esp_reset_reason(void){return reset_reason;}
static uint32_t esp_random(void){return 0x12345678;}
static int64_t esp_timer_get_time(void){return clock_us;}
static bool read_pico_sd_io_ready(void){return ready;}
static bool crash_note_valid(void){return panic_valid;}
static const char *crash_note_text(void){return "panic pc=0x40001234 exc=5 core=1";}
static unsigned rp_crash_log_count(void){return 1;}
static const char *rp_crash_log_line(uint8_t i){assert(i==0);return "1.000 before failure";}
static void metalio_sleep_previous_summary(char *out,size_t n){snprintf(out,n,"sleep_previous valid=1 cycle=7 phase=11 raw_cpu0=19");}
static FILE *checked_open(const char *p,const char *m){return fault==1?NULL:fopen(p,m);}
static int checked_flush(FILE *f){int e=fflush(f);return fault==2?-1:e;}
static int checked_sync(int fd){int e=fsync(fd);return fault==3?-1:e;}
static int checked_close(FILE *f){int e=fclose(f);return fault==4?-1:e;}
static int checked_mkdir(const char *p,mode_t m){if(fault==7){errno=EIO;return -1;}return mkdir(p,m);}
static int checked_print(FILE *f,const char *fmt,...){
 if(fault==6)return -1;
 va_list a;va_start(a,fmt);int n=vfprintf(f,fmt,a);va_end(a);return n;
}
static int checked_rename(const char *a,const char *b){return fault==5?-1:rename(a,b);}
#define mkdir checked_mkdir
#define fprintf checked_print
#define fopen checked_open
#define fflush checked_flush
#define fsync checked_sync
#define fclose checked_close
#define rename checked_rename
'''
# Avoid an unused placeholder under strict host warnings.
prefix=prefix.replace('static const void *esp_app_get_description_placeholder;\n','')
tests=r'''
static unsigned pending(void){unsigned n=0;for(unsigned i=0;i<REPORT_SLOTS;i++)n+=s_retained.pending[i].magic==REPORT_MAGIC;return n;}
static void reboot(int reason){s_started=false;s_retry_at=0;reset_reason=reason;clock_us=0;crash_report_boot();}
static void drain(void){ready=true;for(unsigned i=0;i<5;i++){clock_us+=31000000;crash_report_poll(true);}}
int main(void){
 crash_report_boot();assert(pending()==1);crash_report_boot();assert(pending()==1);
 clock_us=5000000;crash_report_poll(true);assert(pending()==1);
 reboot(ESP_RST_PANIC);assert(pending()==2&&s_retained.pending[1].loop==1);
 assert(strstr(s_retained.pending[1].panic,"40001234"));
 assert(strstr(s_retained.pending[1].sleep,"phase=11"));
 assert(strstr(s_retained.pending[1].crumbs[0],"before failure"));
 ready=true;crash_report_poll(false);assert(pending()==2);
 drain();assert(!pending());
 FILE *f=checked_open("crash_log/crash_12345678_00000002.txt","r");assert(f);
 char text[4096];size_t n=fread(text,1,sizeof(text)-1,f);text[n]=0;checked_close(f);
 assert(strstr(text,"reset_reason=4")&&strstr(text,"phase=11")&&strstr(text,"before failure"));
 for(int e=1;e<=7;e++){
  reboot(ESP_RST_SW);unsigned before=pending();fault=e;
  clock_us=31000000;crash_report_poll(true);assert(pending()==before);
  char path[100];snprintf(path,sizeof(path),"crash_log/crash_12345678_%08lx.txt",(unsigned long)s_retained.next_id);
  struct stat st;assert(stat(path,&st)!=0);
  fault=0;drain();assert(!pending());
 }
 // Reset after successful rename but before RTC acknowledgement is idempotent.
 reboot(ESP_RST_SW);report_t *r=&s_retained.pending[(s_retained.next_id-1)%REPORT_SLOTS];
 assert(report_write(r));assert(pending()==1);drain();assert(!pending());
 // A long card-less bootloop retains four recent immutable events and reports loss.
 ready=false;for(unsigned i=0;i<6;i++)reboot(ESP_RST_PWR_GLITCH);
 assert(pending()==4&&s_retained.dropped==2);drain();assert(!pending());
 // Corrupt pending payload is never exported as valid evidence.
 reboot(ESP_RST_SW);r=&s_retained.pending[(s_retained.next_id-1)%REPORT_SLOTS];r->version[0]^=1;
 drain();assert(!pending());
 // Never dereference an old image's panic-description pointer on firmware upgrade.
 snprintf(desc.version,sizeof(desc.version),"1.2.4RC3-vn");reboot(ESP_RST_PANIC);
 r=&s_retained.pending[(s_retained.next_id-1)%REPORT_SLOTS];assert(!r->panic[0]);
 clock_us=180000000;crash_report_poll(false);reboot(ESP_RST_SW);
 assert(!s_retained.loop);
 puts("PASS: production crash queue, reset metadata/panic/sleep/breadcrumbs, card absence, mkdir/open/write/flush/fsync/close/rename failures, reboot idempotency, 4-event bootloop bound/loss, checksum and upgrade-pointer guard");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(prefix+source+tests)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],cwd=p,check=True)
