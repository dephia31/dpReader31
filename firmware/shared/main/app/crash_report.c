/* SPDX-License-Identifier: Apache-2.0
 * Retain bounded boot evidence in RTC; export only from normal awake context.
 * No filesystem access, allocation or formatting is added to the panic handler.
 */
#include "crash_report.h"
#include "crash_note.h"
#include "metalio_sleep.h"
#include "read_pico_crashlog.h"
#include "read_pico_sd.h"
#include "esp_attr.h"
#include "esp_app_desc.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "esp_log.h"
#include <stdint.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#define REPORT_MAGIC UINT32_C(0x43525032)
#define REPORT_SLOTS 4
#define REPORT_DIR "/sdcard/crash_log"
#define REPORT_FIRST_VALID_TIME UINT32_C(1704067200)
#define REPORT_LAST_VALID_TIME UINT32_C(4102444799)
typedef struct {
    uint32_t seconds, check;
} report_stamp_t;
typedef struct {
    uint32_t magic, id, reason, loop, previous_uptime_s, checksum;
    char version[32], previous_version[32], panic[112], sleep[256];
    char crumbs[RP_CRASH_LOG_LINES][40];
} report_t;
typedef struct {
    uint32_t magic, session, next_id, loop, uptime_s, dropped;
    char version[32];
    report_t pending[REPORT_SLOTS];
    // Keep the original RTC report layout so older queued events can still be exported.
    report_stamp_t stamps[REPORT_SLOTS];
    struct {
        uint32_t check;
        char text[CRASH_NOTE_DETAIL_SIZE];
    } panic_details[REPORT_SLOTS];
} retained_t;
static RTC_NOINIT_ATTR retained_t s_retained;
static int64_t s_retry_at;
static bool s_started;
static uint32_t report_clock(void) {
    time_t now = time(NULL);
    return now >= REPORT_FIRST_VALID_TIME && now <= REPORT_LAST_VALID_TIME ? (uint32_t)now : 0;
}
static void report_set_stamp(const report_t *r, uint32_t seconds) {
    size_t slot = (size_t)(r - s_retained.pending);
    s_retained.stamps[slot] = (report_stamp_t){
        seconds, seconds ^ s_retained.session ^ r->id ^ UINT32_C(0x361f7ab2)
    };
}
static uint32_t report_get_stamp(const report_t *r) {
    report_stamp_t stamp = s_retained.stamps[r - s_retained.pending];
    uint32_t expected = stamp.seconds ^ s_retained.session ^ r->id ^ UINT32_C(0x361f7ab2);
    if (stamp.check == expected && (stamp.seconds == UINT32_MAX ||
        (stamp.seconds >= REPORT_FIRST_VALID_TIME && stamp.seconds <= REPORT_LAST_VALID_TIME)))
        return stamp.seconds;
    uint32_t now = report_clock();
    // Freeze the selected name before writing so a retry cannot create a duplicate.
    report_set_stamp(r, now ? now : UINT32_MAX);
    return now;
}
static uint32_t report_checksum(const report_t *r) {
    const unsigned char *p=(const unsigned char *)r->version;
    uint32_t h=r->id ^ r->reason ^ r->loop ^ r->previous_uptime_s;
    for(size_t i=0;i<sizeof(*r)-(size_t)(p-(const unsigned char *)r);++i) h=(h^p[i])*UINT32_C(16777619);
    return h;
}
void crash_report_boot(void) {
    if(s_started)return;
    s_started=true;
    const uint32_t reason=(uint32_t)esp_reset_reason();
    const char *version=esp_app_get_description()->version;
    if(s_retained.magic!=REPORT_MAGIC || reason==ESP_RST_POWERON) {
        memset(&s_retained,0,sizeof(s_retained));
        s_retained.session=esp_random();
        s_retained.magic=REPORT_MAGIC;
    }
    const uint32_t previous_uptime=s_retained.uptime_s;
    s_retained.loop=previous_uptime<120 && s_retained.next_id ? s_retained.loop+1 : 0;
    if(s_retained.loop>100000)s_retained.loop=100000;
    const uint32_t id=++s_retained.next_id;
    report_t *r=&s_retained.pending[(id-1)%REPORT_SLOTS];
    if(r->magic==REPORT_MAGIC && r->checksum==report_checksum(r))++s_retained.dropped;
    r->magic=0;
    memset(r,0,sizeof(*r));
    r->id=id;r->reason=reason;r->loop=s_retained.loop;r->previous_uptime_s=previous_uptime;
    report_set_stamp(r, report_clock());
    snprintf(r->version,sizeof(r->version),"%s",version);
    snprintf(r->previous_version,sizeof(r->previous_version),"%s",s_retained.version);
    size_t slot = (size_t)(r - s_retained.pending);
    s_retained.panic_details[slot].check = 0;
    s_retained.panic_details[slot].text[0] = 0;
    if(reason==ESP_RST_PANIC && !strcmp(version,s_retained.version) && crash_note_valid()) {
        snprintf(r->panic,sizeof(r->panic),"%s",crash_note_text());
        const char *detail = crash_note_detail();
        if (detail) {
            snprintf(s_retained.panic_details[slot].text, CRASH_NOTE_DETAIL_SIZE,
                     "%.*s", CRASH_NOTE_DETAIL_SIZE - 1, detail);
            s_retained.panic_details[slot].check = s_retained.session ^ id ^ UINT32_C(0x504e4332);
        }
    }
    metalio_sleep_previous_summary(r->sleep,sizeof(r->sleep));
    const unsigned n=rp_crash_log_count();
    for(unsigned i=0;i<n && i<RP_CRASH_LOG_LINES;++i) {
        const char *line=rp_crash_log_line((uint8_t)i);
        if(line)snprintf(r->crumbs[i],sizeof(r->crumbs[i]),"%.*s",39,line);
    }
    r->checksum=report_checksum(r);
    r->magic=REPORT_MAGIC;
    snprintf(s_retained.version,sizeof(s_retained.version),"%s",version);
    s_retained.uptime_s=0;
}
static const char *report_reason(uint32_t reason) {
    static const char *const names[]={"unknown","power_on","external","software_reboot","panic",
        "interrupt_watchdog","task_watchdog","watchdog","deep_sleep","brownout","sdio","usb",
        "jtag","efuse","power_glitch","cpu_lockup"};
    return reason<sizeof(names)/sizeof(names[0])?names[reason]:"unknown";
}
static bool report_write(const report_t *r) {
    if(mkdir(REPORT_DIR,0775)!=0 && errno!=EEXIST)return false;
    char path[180],tmp[184];
    snprintf(path,sizeof(path),REPORT_DIR "/crash_%08lx_%08lx.txt",(unsigned long)s_retained.session,(unsigned long)r->id);
    struct stat st;
    // A committed file from the previous naming scheme may still be queued in RTC.
    if(stat(path,&st)==0 && st.st_size>0)return true;
    uint32_t seconds = report_get_stamp(r);
    char date[24] = "unknown-time";
    if (seconds && seconds != UINT32_MAX) {
        time_t local = (time_t)seconds + 7 * 3600;
        struct tm calendar;
        if (gmtime_r(&local, &calendar))
            (void)strftime(date, sizeof(date), "%Y-%m-%d_%H-%M-%S", &calendar);
    }
    snprintf(path,sizeof(path),REPORT_DIR "/crash_%s_%08lx_%08lx_%s.txt",
             date,(unsigned long)s_retained.session,(unsigned long)r->id,report_reason(r->reason));
    snprintf(tmp,sizeof(tmp),"%s.tmp",path);
    /* A reset after rename but before clearing RTC must not duplicate a committed report. */
    if(stat(path,&st)==0)return st.st_size>0;
    FILE *f=fopen(tmp,"w");
    if(!f)return false;
    bool ok=fprintf(f,"dpReader crash_log\nfirmware=%s\nprevious_firmware=%s\nreset_reason=%lu (%s)\n"
        "recorded_at=%s (UTC+07:00, boot or first export)\nevent=%08lx:%08lx\nprevious_uptime_s=%lu\nconsecutive_short_boots=%lu\n"
        "older_events_not_retained=%lu\n%s\n%s\n",
        r->version,r->previous_version,(unsigned long)r->reason,report_reason(r->reason),date,
        (unsigned long)s_retained.session,(unsigned long)r->id,(unsigned long)r->previous_uptime_s,
        (unsigned long)r->loop,(unsigned long)s_retained.dropped,r->panic,r->sleep)>=0;
    size_t slot = (size_t)(r - s_retained.pending);
    if (ok && r->panic[0] && s_retained.panic_details[slot].text[0] &&
        s_retained.panic_details[slot].check == (s_retained.session ^ r->id ^ UINT32_C(0x504e4332))) {
        ok = fprintf(f, "panic_detail=%.*s\n", CRASH_NOTE_DETAIL_SIZE - 1,
                     s_retained.panic_details[slot].text) >= 0;
    }
    if (ok) ok = fprintf(f, "previous_boot_breadcrumbs (newest first):\n") >= 0;
    for(unsigned i=0;ok && i<RP_CRASH_LOG_LINES;++i)
        if(r->crumbs[i][0])ok=fprintf(f,"%s\n",r->crumbs[i])>=0;
    if(fflush(f)!=0)ok=false;
    if(ok && fsync(fileno(f))!=0)ok=false;
    if(fclose(f)!=0)ok=false;
    if(!ok)return false;
    return rename(tmp,path)==0;
}
void crash_report_poll(bool allow_sd) {
    if(!s_started)return;
    const int64_t now=esp_timer_get_time();
    s_retained.uptime_s=(uint32_t)(now/1000000);
    if(!allow_sd || now<s_retry_at || !read_pico_sd_io_ready())return;
    s_retry_at=now+INT64_C(30000000);
    for(unsigned i=0;i<REPORT_SLOTS;++i) {
        report_t *r=&s_retained.pending[i];
        if(r->magic!=REPORT_MAGIC)continue;
        if(r->checksum!=report_checksum(r)) {r->magic=0;continue;}
        if(!report_write(r)) {ESP_LOGW("crash_report","SD report pending; retry later");return;}
        r->magic=0;
        ESP_LOGI("crash_report","Saved boot event %lu to " REPORT_DIR,(unsigned long)r->id);
        /* One bounded report per pass; SD ownership never crosses a UI event. */
        s_retry_at=now+INT64_C(1000000);
        return;
    }
}
