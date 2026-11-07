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
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <errno.h>
#define RTC_NOINIT_ATTR
#define RP_CRASH_LOG_LINES 24
#define CRASH_NOTE_DETAIL_SIZE 160
#define ESP_LOGW(...) ((void)0)
#define ESP_LOGI(...) ((void)0)
enum {ESP_RST_POWERON=1,ESP_RST_SW=3,ESP_RST_PANIC=4,ESP_RST_PWR_GLITCH=14};
static int reset_reason=ESP_RST_POWERON;static int64_t clock_us;
static time_t wall_time=1704067200;
static time_t test_time(time_t *out){if(out)*out=wall_time;return wall_time;}
#define time test_time
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
static const char *crash_note_detail(void){return "assert failed: diagnostic fixture";}
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
 FILE *f=checked_open("crash_log/crash_2024-01-01_07-00-00_12345678_00000002_panic.txt","r");assert(f);
 char text[4096];size_t n=fread(text,1,sizeof(text)-1,f);text[n]=0;checked_close(f);
 assert(strstr(text,"reset_reason=4")&&strstr(text,"phase=11")&&strstr(text,"before failure"));
 assert(strstr(text,"recorded_at=2024-01-01_07-00-00"));
 assert(strstr(text,"panic_detail=assert failed: diagnostic fixture"));
 for(int e=1;e<=7;e++){
  reboot(ESP_RST_SW);unsigned before=pending();fault=e;
  clock_us=31000000;crash_report_poll(true);assert(pending()==before);
  char path[160];snprintf(path,sizeof(path),"crash_log/crash_2024-01-01_07-00-00_12345678_%08lx_software_reboot.txt",(unsigned long)s_retained.next_id);
  struct stat st;assert(stat(path,&st)!=0);
  fault=0;drain();assert(!pending());
 }
 // Reset after successful rename but before RTC acknowledgement is idempotent.
 reboot(ESP_RST_SW);report_t *r=&s_retained.pending[(s_retained.next_id-1)%REPORT_SLOTS];
 assert(report_write(r));assert(pending()==1);drain();assert(!pending());
 // RTC restore after boot still names the event, and a retry keeps its original filename.
 wall_time=0;reboot(ESP_RST_SW);r=&s_retained.pending[(s_retained.next_id-1)%REPORT_SLOTS];
 wall_time=1704067260;fault=5;assert(!report_write(r));fault=0;
 wall_time=1704067320;assert(report_write(r));
 char restored[160];snprintf(restored,sizeof(restored),"crash_log/crash_2024-01-01_07-01-00_12345678_%08lx_software_reboot.txt",(unsigned long)r->id);
 assert(access(restored,F_OK)==0);drain();
 wall_time=0;reboot(ESP_RST_SW);r=&s_retained.pending[(s_retained.next_id-1)%REPORT_SLOTS];
 assert(report_write(r));
 char unknown[160];snprintf(unknown,sizeof(unknown),"crash_log/crash_unknown-time_12345678_%08lx_software_reboot.txt",(unsigned long)r->id);
 assert(access(unknown,F_OK)==0);wall_time=1704067200;drain();
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

note_source = (root / 'main/app/crash_note.c').read_text()
note_source = re.sub(r'^#include "[^"]+"\n', '', note_source, flags=re.M)
note_prefix = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define __riscv 1
#define RTC_NOINIT_ATTR
#define CRASH_NOTE_DETAIL_SIZE 160
#define ESP_LOGE(tag, ...) ((void)(tag))
typedef struct { unsigned long ra; } RvExcFrame;
typedef struct {
    int core, exception;
    const char *reason, *description;
    const void *addr, *frame;
} panic_info_t;
enum { PANIC_EXCEPTION_ABORT=3, PANIC_EXCEPTION_FAULT=4 };
static bool g_panic_abort;
static char *g_panic_abort_details;
static unsigned forwarded;
void __real_esp_panic_handler(panic_info_t *info){(void)info;++forwarded;}
'''
note_test = r'''
int main(void) {
    s_note.magic = UINT32_C(0x434e4f54); // Historical record held a flash pointer.
    assert(!crash_note_valid() && !crash_note_text() && !crash_note_detail());
    RvExcFrame frame = {.ra = 0x2f012346};
    panic_info_t info = {.core=0,.exception=PANIC_EXCEPTION_FAULT,
        .description="Exception was unhandled.",.addr=(void *)0x2f017884,.frame=&frame};
    char abort_message[240];
    memset(abort_message, 'x', sizeof(abort_message)-1);
    abort_message[sizeof(abort_message)-1] = 0;
    memcpy(abort_message, "assert failed:\n", 15);
    g_panic_abort=true;
    g_panic_abort_details=abort_message;
    __wrap_esp_panic_handler(&info);
    memset(abort_message, 0, sizeof(abort_message));
    assert(forwarded==1 && crash_note_valid());
    assert(strstr(crash_note_text(),"pc=0x2f017884 exc=3 core=0 ra=0x2f012346"));
    assert(strncmp(crash_note_detail(),"assert failed: ",15)==0);
    assert(strlen(crash_note_detail())==CRASH_NOTE_DETAIL_SIZE-1);
    g_panic_abort=false;
    info.frame=NULL;
    info.reason="Load access fault";
    __wrap_esp_panic_handler(&info);
    assert(forwarded==2 && strstr(crash_note_text(),"exc=4 core=0 ra=0x00000000"));
    assert(strcmp(crash_note_detail(),"Load access fault")==0);
    puts("PASS: panic wrapper retains bounded abort detail and RISC-V return address without keeping stale pointers");
}
'''
with tempfile.TemporaryDirectory() as d:
    p = Path(d)
    (p / 'test.c').write_text(note_prefix + note_source + note_test)
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-fsanitize=undefined',
                    str(p / 'test.c'), '-o', str(p / 'test')], check=True)
    subprocess.run([str(p / 'test')], check=True)
