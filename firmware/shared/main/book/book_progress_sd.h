/* SPDX-License-Identifier: Apache-2.0
 * Nhật ký dự phòng dạng snapshot, giữ NVS cũ và công bố sau fsync.
 * Các API tiến độ có cùng hợp đồng tuần tự hóa như trước.
 */
#include <errno.h>
#include <sys/stat.h>
#include <unistd.h>
#ifndef BOOK_PROGRESS_SD_ROOT
#define BOOK_PROGRESS_SD_ROOT "/sdcard/.dpreader-progress"
#endif
#define SD_PROGRESS_FILE BOOK_PROGRESS_SD_ROOT "/progress.bin"
#define SD_PROGRESS_MAX 4096u
typedef struct {
    char path[PATH_CAP];
    book_progress_t progress;
    uint32_t deleted;
} sd_progress_entry_t;
typedef struct {
    uint32_t magic, count, sequence, checksum;
    char last[PATH_CAP];
} sd_progress_header_t;
typedef struct {
    sd_progress_header_t header;
    sd_progress_entry_t *entries;
} sd_progress_t;
static uint32_t sd_progress_crc(const void *data, size_t size, uint32_t crc) {
    const uint8_t *bytes = data;
    for (size_t i = 0; i < size; ++i) {
        crc ^= bytes[i];
        for (unsigned b = 0; b < 8; ++b)
            crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1)));
    }
    return crc;
}
static bool sd_progress_read_file(const char *path, sd_progress_t *db) {
    FILE *file = fopen(path, "rb");
    if (!file)
        return false;
    bool ok = fread(&db->header, 1, sizeof(db->header), file) == sizeof(db->header) &&
              db->header.magic == 0x31535044 && db->header.count <= SD_PROGRESS_MAX &&
              memchr(db->header.last, 0, PATH_CAP);
    if (ok) {
        db->entries = calloc(db->header.count + 1, sizeof(*db->entries));
        ok = db->entries != NULL;
    }
    if (ok)
        ok = fread(db->entries, sizeof(*db->entries), db->header.count, file) == db->header.count &&
             fgetc(file) == EOF;
    fclose(file);
    if (ok) {
        uint32_t expected = db->header.checksum;
        db->header.checksum = 0;
        uint32_t crc = sd_progress_crc(&db->header, sizeof(db->header), ~0u);
        ok = sd_progress_crc(db->entries, db->header.count * sizeof(*db->entries), crc) == expected;
        db->header.checksum = expected;
        for (uint32_t i = 0; ok && i < db->header.count; ++i)
            ok = memchr(db->entries[i].path, 0, PATH_CAP) != NULL;
    }
    if (!ok) {
        free(db->entries);
        memset(db, 0, sizeof(*db));
    }
    return ok;
}
static atomic_bool sd_progress_active;
static bool sd_progress_read(sd_progress_t *db) {
    memset(db, 0, sizeof(*db));
    bool ok = sd_progress_read_file(SD_PROGRESS_FILE, db) ||
              sd_progress_read_file(SD_PROGRESS_FILE ".old", db);
    if (ok)
        sd_progress_active = true;
    return ok;
}
static int sd_progress_find(const sd_progress_t *db, const char *path) {
    for (uint32_t i = 0; i < db->header.count; ++i)
        if (!strcmp(db->entries[i].path, path))
            return (int)i;
    return -1;
}
static esp_err_t sd_progress_write(sd_progress_t *db) {
    if (mkdir(BOOK_PROGRESS_SD_ROOT, 0775) && errno != EEXIST)
        return ESP_FAIL;
    db->header.magic = 0x31535044;
    db->header.checksum = 0;
    uint32_t crc = sd_progress_crc(&db->header, sizeof(db->header), ~0u);
    db->header.checksum =
        sd_progress_crc(db->entries, db->header.count * sizeof(*db->entries), crc);
    FILE *file = fopen(SD_PROGRESS_FILE ".tmp", "wb");
    if (!file)
        return ESP_FAIL;
    bool ok =
        fwrite(&db->header, 1, sizeof(db->header), file) == sizeof(db->header) &&
        fwrite(db->entries, sizeof(*db->entries), db->header.count, file) == db->header.count &&
        fflush(file) == 0 && fsync(fileno(file)) == 0;
    if (fclose(file))
        ok = false;
    if (!ok) {
        remove(SD_PROGRESS_FILE ".tmp");
        return ESP_FAIL;
    }
    struct stat st;
    if (stat(SD_PROGRESS_FILE, &st) == 0) {
        sd_progress_t verified = {0};
        bool valid = sd_progress_read_file(SD_PROGRESS_FILE, &verified);
        free(verified.entries);
        if (valid) {
            if (remove(SD_PROGRESS_FILE ".old") && errno != ENOENT)
                return ESP_FAIL;
            if (rename(SD_PROGRESS_FILE, SD_PROGRESS_FILE ".old"))
                return ESP_FAIL;
        } else if (remove(SD_PROGRESS_FILE))
            return ESP_FAIL;
    }
    if (rename(SD_PROGRESS_FILE ".tmp", SD_PROGRESS_FILE))
        return ESP_FAIL;
    book_progress_notify_changed();
    return ESP_OK;
}
static bool sd_progress_init(sd_progress_t *db) {
    if (sd_progress_read(db))
        return true;
    if (sd_progress_active)
        return false;
    struct stat st;
    if (!stat(SD_PROGRESS_FILE, &st) || !stat(SD_PROGRESS_FILE ".old", &st))
        return false;
    db->entries = calloc(1, sizeof(*db->entries));
    if (!db->entries)
        return false;
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READONLY, &h) == ESP_OK) {
        (void)nvs_get_u32(h, "seq", &db->header.sequence);
        nvs_close(h);
    }
    (void)nvs_progress_last_path(db->header.last, sizeof(db->header.last));
    return true;
}
bool book_progress_load(const char *path, uint32_t size, book_progress_t *out) {
    if (!path || !out)
        return false;
    sd_progress_t db;
    bool found = sd_progress_read(&db);
    int at = found ? sd_progress_find(&db, path) : -1;
    if (at < 0) {
        free(db.entries);
        return nvs_progress_load(path, size, out);
    }
    sd_progress_entry_t *entry = &db.entries[at];
    bool ok = !entry->deleted && entry->progress.file_size == size && entry->progress.pct <= 100 &&
              entry->progress.px >= BOOK_READER_PX_MIN && entry->progress.px <= BOOK_READER_PX_MAX;
    if (ok)
        *out = entry->progress;
    free(db.entries);
    return ok;
}
static esp_err_t sd_progress_put(const char *path, const book_progress_t *progress, bool forget) {
    sd_progress_t db;
    if (!sd_progress_init(&db))
        return ESP_FAIL;
    int at = sd_progress_find(&db, path);
    if (at < 0) {
        if (db.header.count == SD_PROGRESS_MAX) {
            free(db.entries);
            return ESP_ERR_NO_MEM;
        }
        at = (int)db.header.count++;
    }
    sd_progress_entry_t *entry = &db.entries[at];
    memset(entry, 0, sizeof(*entry));
    strcpy(entry->path, path);
    if (progress) {
        if (db.header.sequence == UINT32_MAX) {
            free(db.entries);
            return ESP_ERR_INVALID_STATE;
        }
        entry->progress = *progress;
        entry->progress.last_open_s = ++db.header.sequence;
    } else
        entry->deleted = 1;
    if (forget && !strcmp(db.header.last, path))
        db.header.last[0] = 0;
    esp_err_t err = sd_progress_write(&db);
    free(db.entries);
    return err;
}
esp_err_t book_progress_save(const char *path, const book_progress_t *progress) {
    if (!path_valid(path) || !progress || progress->px < BOOK_READER_PX_MIN ||
        progress->px > BOOK_READER_PX_MAX || progress->pct > 100)
        return ESP_ERR_INVALID_ARG;
    sd_progress_t db;
    bool active = sd_progress_read(&db);
    active = active || sd_progress_active;
    free(db.entries);
    esp_err_t err = active ? ESP_ERR_NVS_NOT_ENOUGH_SPACE : nvs_progress_save(path, progress);
    if (err != ESP_ERR_NVS_NOT_ENOUGH_SPACE)
        return err;
    esp_err_t fallback = sd_progress_put(path, progress, false);
    return fallback == ESP_OK ? ESP_OK : err;
}
bool book_progress_last_path(char *out, size_t cap) {
    if (!out || !cap)
        return false;
    sd_progress_t db;
    if (!sd_progress_read(&db))
        return nvs_progress_last_path(out, cap);
    bool ok = db.header.last[0] && strlen(db.header.last) < cap;
    out[0] = 0;
    if (ok)
        strcpy(out, db.header.last);
    free(db.entries);
    return ok;
}
esp_err_t book_progress_set_last_path(const char *path) {
    if (!path || strnlen(path, PATH_CAP) >= PATH_CAP)
        return ESP_ERR_INVALID_ARG;
    sd_progress_t db;
    bool active = sd_progress_read(&db);
    if (!active && sd_progress_active)
        return ESP_FAIL;
    if (!active) {
        esp_err_t err = nvs_progress_set_last_path(path);
        if (err != ESP_ERR_NVS_NOT_ENOUGH_SPACE)
            return err;
        if (!sd_progress_init(&db))
            return ESP_FAIL;
    }
    strcpy(db.header.last, path);
    esp_err_t err = sd_progress_write(&db);
    free(db.entries);
    return err;
}
esp_err_t book_progress_clear(const char *path) {
    if (!path_valid(path))
        return ESP_ERR_INVALID_ARG;
    sd_progress_t db;
    bool active = sd_progress_read(&db);
    active = active || sd_progress_active;
    free(db.entries);
    return active ? sd_progress_put(path, NULL, false) : nvs_progress_clear(path);
}
esp_err_t book_progress_forget(const char *path) {
    if (!path_valid(path))
        return ESP_ERR_INVALID_ARG;
    sd_progress_t db;
    bool active = sd_progress_read(&db);
    active = active || sd_progress_active;
    free(db.entries);
    if (!active)
        return nvs_progress_forget(path);
    for (book_progress_watch_t *w = s_watches; w; w = w->next)
        if (!strcmp(w->path, path))
            atomic_store(&w->invalidated, true);
    return sd_progress_put(path, NULL, true);
}
typedef struct {
    sd_progress_t *db;
    book_progress_visit_fn visit;
    void *ctx;
    bool more;
} sd_progress_visit_t;
static bool sd_progress_visit_nvs(const char *path, const book_progress_t *progress, void *ctx) {
    sd_progress_visit_t *walk = ctx;
    if (sd_progress_find(walk->db, path) < 0)
        walk->more = walk->visit(path, progress, walk->ctx);
    return walk->more;
}
esp_err_t book_progress_list(book_progress_visit_fn visit, void *ctx) {
    if (!visit)
        return ESP_ERR_INVALID_ARG;
    sd_progress_t db;
    if (!sd_progress_read(&db))
        return nvs_progress_list(visit, ctx);
    sd_progress_visit_t walk = {&db, visit, ctx, true};
    esp_err_t err = nvs_progress_list(sd_progress_visit_nvs, &walk);
    for (uint32_t i = 0; err == ESP_OK && walk.more && i < db.header.count; ++i)
        if (!db.entries[i].deleted)
            walk.more = visit(db.entries[i].path, &db.entries[i].progress, ctx);
    free(db.entries);
    return err;
}
