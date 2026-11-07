/* SPDX-License-Identifier: Apache-2.0
 * Last reading event, separate from the legacy progress ordering sequence.
 * Turns are RAM-only; bounded SD checkpoints never consume settings NVS. */
#include "kosync_state.h"
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include "book_store.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#if DPREADER_BOARD_METALIO47 && defined(ESP_PLATFORM)
#include "esp_attr.h"
#define STATE_RAM EXT_RAM_BSS_ATTR
#else
#define STATE_RAM
#endif
#ifndef KOSYNC_STATE_PARENT
#define KOSYNC_STATE_PARENT "/sdcard/.readpico"
#endif
#ifndef KOSYNC_STATE_ROOT
#define KOSYNC_STATE_ROOT "/sdcard/.readpico/kosync"
#endif
#define CAPS (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
typedef struct {
    uint32_t version, size, offset;
    uint16_t chapter;
    int64_t timestamp;
    char path[BOOK_STORE_PATH_MAX];
    uint32_t checksum;
} record_t;
static portMUX_TYPE state_lock = portMUX_INITIALIZER_UNLOCKED;
static STATE_RAM record_t current;
static bool io_busy;
static bool trusted(int64_t t) {
    int64_t now = time(NULL);
    return now >= 1700000000 && t >= 1700000000 && t <= now + 300;
}
static bool io_enter(void) {
    portENTER_CRITICAL(&state_lock);
    bool available = !io_busy;
    if (available) io_busy = true;
    portEXIT_CRITICAL(&state_lock);
    return available;
}
static void io_exit(void) {
    portENTER_CRITICAL(&state_lock);
    io_busy = false;
    portEXIT_CRITICAL(&state_lock);
}
static void filename(const char* p, char* out, size_t cap) {
    uint64_t h = UINT64_C(14695981039346656037);
    for (; *p; p++) h = (h ^ (unsigned char)*p) * UINT64_C(1099511628211);
    snprintf(out, cap, KOSYNC_STATE_ROOT "/%016llx.bin", (unsigned long long)h);
}
static uint32_t checksum(const record_t* r) {
    uint32_t crc = ~0u;
    const unsigned char* p = (const unsigned char*)r;
    for (size_t i = 0; i < offsetof(record_t, checksum); i++) {
        crc ^= p[i];
        for (unsigned j = 0; j < 8; j++)
            crc = (crc >> 1) ^ (0xedb88320u & ((uint32_t)-(int32_t)(crc & 1)));
    }
    return ~crc;
}
static bool read_record(const char* file, record_t* r) {
    FILE* f = fopen(file, "rb");
    if (!f) return false;
    bool valid = fread(r, 1, sizeof(*r), f) == sizeof(*r) && fgetc(f) == EOF;
    fclose(f);
    return valid && r->version == 1 && memchr(r->path, 0, sizeof(r->path)) &&
           r->checksum == checksum(r);
}
static bool load(const char* path, record_t* r) {
    if (!io_enter()) return false;
    char file[192], old[200];
    filename(path, file, sizeof(file));
    snprintf(old, sizeof(old), "%s.old", file);
    bool ok = read_record(file, r) || read_record(old, r);
    io_exit();
    return ok && !strcmp(path, r->path) && trusted(r->timestamp);
}
void kosync_state_begin(const char* path, uint32_t size, uint16_t chapter, uint32_t offset) {
    if (!path || strlen(path) >= BOOK_STORE_PATH_MAX) return;
    record_t* r = heap_caps_calloc(1, sizeof(*r), CAPS);
    if (!r) return;
    bool found = load(path, r) && r->size == size && r->chapter == chapter && r->offset == offset;
    if (!found) {
        memset(r, 0, sizeof(*r));
        r->version = 1;
        r->size = size;
        r->chapter = chapter;
        r->offset = offset;
        strcpy(r->path, path);
    }
    portENTER_CRITICAL(&state_lock);
    current = *r;
    portEXIT_CRITICAL(&state_lock);
    free(r);
}
void kosync_state_touch(const char* path, uint32_t size, uint16_t chapter, uint32_t offset) {
    if (!path) return;
    portENTER_CRITICAL(&state_lock);
    bool changed = !strcmp(path, current.path) &&
                   (current.size != size || current.chapter != chapter || current.offset != offset);
    portEXIT_CRITICAL(&state_lock);
    if (!changed) return;
    int64_t now = time(NULL);
    if (!trusted(now)) now = 0;
    portENTER_CRITICAL(&state_lock);
    if (!strcmp(path, current.path)) {
        current.size = size;
        current.chapter = chapter;
        current.offset = offset;
        current.timestamp = now;
    }
    portEXIT_CRITICAL(&state_lock);
}
void kosync_state_accept(const char* path, uint16_t chapter, uint32_t offset, int64_t timestamp) {
    if (!path) return;
    if (!trusted(timestamp)) timestamp = 0;
    portENTER_CRITICAL(&state_lock);
    if (!strcmp(path, current.path)) {
        current.chapter = chapter;
        current.offset = offset;
        current.timestamp = timestamp;
    }
    portEXIT_CRITICAL(&state_lock);
}
int64_t kosync_state_timestamp(const char* path, uint16_t chapter, uint32_t offset) {
    if (!path) return 0;
    int64_t value = 0;
    bool active;
    portENTER_CRITICAL(&state_lock);
    active = !strcmp(path, current.path);
    if (active && chapter == current.chapter && offset == current.offset) value = current.timestamp;
    portEXIT_CRITICAL(&state_lock);
    if (!active) {
        record_t* r = heap_caps_calloc(1, sizeof(*r), CAPS);
        struct stat st;
        if (r && load(path, r) && !stat(path, &st) && st.st_size == r->size &&
            r->chapter == chapter && r->offset == offset)
            value = r->timestamp;
        free(r);
    }
    return trusted(value) ? value : 0;
}
void kosync_state_commit(const char* path) {
    if (!path) return;
    record_t *r = heap_caps_calloc(1, sizeof(*r), CAPS),
             *old = heap_caps_calloc(1, sizeof(*old), CAPS);
    if (!r || !old) {
        free(r);
        free(old);
        return;
    }
    portENTER_CRITICAL(&state_lock);
    *r = current;
    portEXIT_CRITICAL(&state_lock);
    if (strcmp(path, r->path) || !trusted(r->timestamp) || !io_enter()) {
        free(r);
        free(old);
        return;
    }
    char file[192], temp[200], backup[200];
    filename(path, file, sizeof(file));
    snprintf(temp, sizeof(temp), "%s.tmp", file);
    snprintf(backup, sizeof(backup), "%s.old", file);
    r->checksum = checksum(r);
    bool valid = read_record(file, old);
    bool exists = access(file, F_OK) == 0;
    if (valid && (strcmp(old->path, path) || !memcmp(r, old, sizeof(*r)))) goto done;
    if (mkdir(KOSYNC_STATE_PARENT, 0777) && errno != EEXIST) goto done;
    if (mkdir(KOSYNC_STATE_ROOT, 0777) && errno != EEXIST) goto done;
    FILE* f = fopen(temp, "wb");
    if (!f) goto done;
    bool ok = fwrite(r, 1, sizeof(*r), f) == sizeof(*r) && fflush(f) == 0 && fsync(fileno(f)) == 0;
    if (fclose(f)) ok = false;
    if (!ok) {
        remove(temp);
        goto done;
    }
    // POSIX can replace atomically. FAT may require a rollback sibling first.
    if (rename(temp, file)) {
        bool moved = false;
        if (exists) {
            if (remove(backup) && errno != ENOENT) {
                remove(temp);
                goto done;
            }
            if (rename(file, backup)) {
                remove(temp);
                goto done;
            }
            moved = true;
        }
        if (rename(temp, file)) {
            if (moved) rename(backup, file);
            remove(temp);
            ESP_LOGW("kosync", "reading timestamp checkpoint failed");
        }
    }
done:
    io_exit();
    free(r);
    free(old);
}
bool kosync_state_remote_newer(int64_t local, int64_t remote, bool* known) {
    bool valid = trusted(local) && trusted(remote) && local != remote;
    if (known) *known = valid;
    return valid && remote > local;
}
