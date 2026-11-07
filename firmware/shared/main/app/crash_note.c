/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * Giữ dấu vết panic trong RTC trước khi ESP-IDF in lỗi và khởi động lại.
 * Chỉ sao chép dữ liệu có giới hạn. Định dạng chuỗi ở lần khởi động sau.
 */
#include "crash_note.h"

#include <stdio.h>

#include "esp_attr.h"
#include "esp_log.h"
#include "esp_private/panic_internal.h"
#if defined(__riscv)
#include "riscv/rvruntime-frames.h"
#endif

static const char *TAG = "crash";

// Phiên bản mới giữ nội dung chi tiết thay cho địa chỉ chuỗi của firmware cũ.
typedef struct {
    uint32_t magic, addr, caller, exception, core;
} crash_note_header_t;

#define CRASH_NOTE_MAGIC UINT32_C(0x434E4F32)
RTC_NOINIT_ATTR static crash_note_header_t s_note;
RTC_NOINIT_ATTR static char s_detail[CRASH_NOTE_DETAIL_SIZE];

void __real_esp_panic_handler(panic_info_t *info);

void __wrap_esp_panic_handler(panic_info_t *info) {
    if (info != NULL) {
        s_note.magic = 0;
        s_note.addr = (uint32_t)(uintptr_t)info->addr;
        s_note.caller = 0;
#if defined(__riscv)
        if (info->frame != NULL) {
            s_note.caller = (uint32_t)((const RvExcFrame *)info->frame)->ra;
        }
#endif
        s_note.exception = g_panic_abort ? PANIC_EXCEPTION_ABORT : info->exception;
        s_note.core = (uint32_t)info->core;
        const char *detail = g_panic_abort && g_panic_abort_details
            ? g_panic_abort_details : (info->reason ? info->reason : info->description);
        // volatile giữ việc sao chép trong vòng lặp, không gọi thư viện trong panic.
        volatile char *out = s_detail;
        size_t i = 0;
        if (detail != NULL) {
            while (i < sizeof(s_detail) - 1 && detail[i] != 0) {
                const unsigned char c = (unsigned char)detail[i];
                out[i++] = c < 32 || c == 127 ? ' ' : (char)c;
            }
        }
        out[i] = 0;
        s_note.magic = CRASH_NOTE_MAGIC;
    }
    __real_esp_panic_handler(info);
}

bool crash_note_valid(void) { return s_note.magic == CRASH_NOTE_MAGIC; }

const char *crash_note_text(void) {
    static char line[112];
    if (!crash_note_valid()) return NULL;
    snprintf(line, sizeof(line), "panic pc=0x%08lx exc=%lu core=%lu ra=0x%08lx",
             (unsigned long)s_note.addr, (unsigned long)s_note.exception,
             (unsigned long)s_note.core, (unsigned long)s_note.caller);
    return line;
}

const char *crash_note_detail(void) {
    return crash_note_valid() && s_detail[0] ? s_detail : NULL;
}

void crash_note_log(void) {
    const char *line = crash_note_text();
    if (line != NULL) ESP_LOGE(TAG, "%s", line);
    const char *detail = crash_note_detail();
    if (detail != NULL) ESP_LOGE(TAG, "panic detail: %.*s", CRASH_NOTE_DETAIL_SIZE - 1, detail);
}
