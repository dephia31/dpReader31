/* SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0 */
#include "book_state_store.h"

typedef struct {
    uint32_t version, deleted;
    book_progress_t progress;
} file_progress_t;
typedef struct {
    uint32_t version, sequence, ignore_legacy;
    char last[PATH_CAP];
} file_library_t;

static esp_err_t file_library_read(const char *root, file_library_t *library) {
    memset(library, 0, sizeof(*library));
    size_t size = sizeof(*library);
    esp_err_t err = book_state_read(root, "", "library", library, &size);
    if (err == ESP_OK && (size != sizeof(*library) || library->version != 1 ||
        library->ignore_legacy > 1 || !memchr(library->last, 0, sizeof(library->last))))
        err = ESP_ERR_INVALID_RESPONSE;
    return err;
}
static bool file_progress_legacy_allowed(const char *root) {
    file_library_t library;
    esp_err_t err = file_library_read(root, &library);
    return err == ESP_ERR_NOT_FOUND || (err == ESP_OK && !library.ignore_legacy);
}
static esp_err_t file_progress_read(const char *path, file_progress_t *value) {
    size_t size = sizeof(*value);
    esp_err_t err = book_state_read(book_state_root(path), path, "progress", value, &size);
    if (err == ESP_OK && (size != sizeof(*value) || value->version != 1 || value->deleted > 1 ||
        (!value->deleted && (value->progress.px < BOOK_READER_PX_MIN ||
         value->progress.px > BOOK_READER_PX_MAX || value->progress.pct > 100))))
        err = ESP_ERR_INVALID_RESPONSE;
    return err;
}
bool book_progress_load(const char *path, uint32_t size, book_progress_t *out) {
    if (!path_valid(path) || !out) return false;
    file_progress_t value;
    esp_err_t err = file_progress_read(path, &value);
    if (err == ESP_ERR_NOT_FOUND && file_progress_legacy_allowed(book_state_root(path)))
        return legacy_sd_progress_load(path, size, out);
    if (err != ESP_OK || value.deleted || value.progress.file_size != size) return false;
    *out = value.progress;
    return true;
}
static esp_err_t file_library_init(const char *root, file_library_t *library) {
    esp_err_t err = file_library_read(root, library);
    if (err != ESP_ERR_NOT_FOUND) return err;
    *library = (file_library_t){.version = 1};
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READONLY, &h) == ESP_OK) {
        (void)nvs_get_u32(h, "seq", &library->sequence);
        nvs_close(h);
    }
    sd_progress_t legacy;
    if (sd_progress_read(&legacy) && legacy.header.sequence > library->sequence)
        library->sequence = legacy.header.sequence;
    free(legacy.entries);
    (void)legacy_sd_progress_last_path(library->last, sizeof(library->last));
    if(library->last[0]&&strcmp(book_state_root(library->last),root))library->last[0]=0;
    return ESP_OK;
}
static esp_err_t file_progress_put(const char *path, const book_progress_t *progress, bool preserve_order) {
    const char *root = book_state_root(path);
    file_progress_t value = {.version = 1, .deleted = progress == NULL};
    if (progress) {
        value.progress = *progress;
        file_library_t library;
        esp_err_t err = file_library_init(root, &library);
        if (err != ESP_OK) return err;
        if (preserve_order) {
            if (progress->last_open_s > library.sequence) library.sequence = progress->last_open_s;
        } else {
            file_library_t other;
            const char *other_root = !strcmp(root, BOOK_STATE_SD_ROOT) ? BOOK_STATE_FLASH_ROOT : BOOK_STATE_SD_ROOT;
            if (file_library_read(other_root, &other) == ESP_OK && other.sequence > library.sequence)
                library.sequence = other.sequence;
            if (library.sequence == UINT32_MAX) return ESP_ERR_INVALID_STATE;
            value.progress.last_open_s = ++library.sequence;
        }
        err = book_state_write(root, "", "library", &library, sizeof(library));
        if (err != ESP_OK) return err;
    }
    esp_err_t err = book_state_write(root, path, "progress", &value, sizeof(value));
    if (err == ESP_OK) book_progress_notify_changed();
    return err;
}
esp_err_t book_progress_save(const char *path, const book_progress_t *progress) {
    if (!path_valid(path) || !progress || progress->px < BOOK_READER_PX_MIN ||
        progress->px > BOOK_READER_PX_MAX || progress->pct > 100) return ESP_ERR_INVALID_ARG;
    return warn_error(file_progress_put(path, progress, false));
}
esp_err_t book_progress_import(const char *path, const book_progress_t *progress) {
    if (!path_valid(path) || !progress || progress->px < BOOK_READER_PX_MIN ||
        progress->px > BOOK_READER_PX_MAX || progress->pct > 100) return ESP_ERR_INVALID_ARG;
    return file_progress_put(path, progress, true);
}
bool book_progress_last_path(char *out, size_t cap) {
    if (!out || !cap) return false;
    out[0] = 0;
    file_library_t library = {0}, candidate;
    const char *roots[] = {BOOK_STATE_SD_ROOT, BOOK_STATE_FLASH_ROOT};
    bool found = false, modern = false;
    for (unsigned i=0;i<2;++i) {
        if (!book_state_ready(roots[i])) continue;
        esp_err_t err=file_library_read(roots[i],&candidate);
        if(err==ESP_OK) {
            modern=true;
            if(candidate.last[0]&&(!found||candidate.sequence>library.sequence)){library=candidate;found=true;}
        } else if(err!=ESP_ERR_NOT_FOUND)modern=true;
    }
    if(!modern)return legacy_sd_progress_last_path(out,cap);
    if(!found||strlen(library.last)>=cap)return false;
    strcpy(out, library.last);
    return true;
}
esp_err_t book_progress_set_last_path(const char *path) {
    if (!path || strnlen(path, PATH_CAP) >= PATH_CAP) return ESP_ERR_INVALID_ARG;
    if(!path[0]) {
        const char *roots[]={BOOK_STATE_SD_ROOT,BOOK_STATE_FLASH_ROOT};
        for(unsigned i=0;i<2;++i)if(book_state_ready(roots[i])) {
            file_library_t value;esp_err_t err=file_library_init(roots[i],&value);
            if(err!=ESP_OK)return err;
            value.last[0]=0;
            err=book_state_write(roots[i],"","library",&value,sizeof(value));
            if(err!=ESP_OK)return err;
        }
        book_progress_notify_changed();return ESP_OK;
    }
    const char *root = path[0] ? book_state_root(path) :
        (book_state_ready(BOOK_STATE_SD_ROOT) ? BOOK_STATE_SD_ROOT : BOOK_STATE_FLASH_ROOT);
    file_library_t library;
    esp_err_t err = file_library_init(root, &library);
    if (err != ESP_OK) return err;
    file_library_t other;
    const char *other_root=!strcmp(root,BOOK_STATE_SD_ROOT)?BOOK_STATE_FLASH_ROOT:BOOK_STATE_SD_ROOT;
    if(file_library_read(other_root,&other)==ESP_OK&&other.sequence>library.sequence)library.sequence=other.sequence;
    if(library.sequence==UINT32_MAX)return ESP_ERR_INVALID_STATE;
    ++library.sequence;
    strcpy(library.last, path);
    err = book_state_write(root, "", "library", &library, sizeof(library));
    if (err == ESP_OK) book_progress_notify_changed();
    return err;
}
esp_err_t book_progress_clear(const char *path) {
    return path_valid(path) ? file_progress_put(path, NULL, false) : ESP_ERR_INVALID_ARG;
}
esp_err_t book_progress_forget(const char *path) {
    if (!path_valid(path)) return ESP_ERR_INVALID_ARG;
    for (book_progress_watch_t *w = s_watches; w; w = w->next)
        if (!strcmp(w->path, path)) atomic_store(&w->invalidated, true);
    file_library_t library;
    const char *root = book_state_root(path);
    esp_err_t err = file_library_init(root, &library);
    if (err != ESP_OK) return err;
    if (!strcmp(library.last, path)) {
        library.last[0] = 0;
        err = book_state_write(root, "", "library", &library, sizeof(library));
        if (err != ESP_OK) return err;
    }
    err = file_progress_put(path, NULL, false);
    if (err == ESP_OK) err = book_state_write(root, path, "profile", "", 0);
    return err;
}
typedef struct {
    book_progress_visit_fn visit;
    void *ctx;
    bool more;
} file_progress_walk_t;
static bool file_progress_visit(const char *path, const void *data, size_t size, void *ctx) {
    file_progress_walk_t *walk = ctx;
    if (size != sizeof(file_progress_t)) return true;
    const file_progress_t *value = data;
    if (value->version == 1 && !value->deleted && value->progress.pct <= 100 &&
        value->progress.px >= BOOK_READER_PX_MIN && value->progress.px <= BOOK_READER_PX_MAX)
        walk->more = walk->visit(path, &value->progress, walk->ctx);
    return walk->more;
}
static bool file_progress_visit_legacy(const char *path, const book_progress_t *progress, void *ctx) {
    file_progress_walk_t *walk = ctx;
    file_progress_t value;
    if (file_progress_legacy_allowed(book_state_root(path)) &&
        file_progress_read(path, &value) == ESP_ERR_NOT_FOUND)
        walk->more = walk->visit(path, progress, walk->ctx);
    return walk->more;
}
esp_err_t book_progress_list(book_progress_visit_fn visit, void *ctx) {
    if (!visit) return ESP_ERR_INVALID_ARG;
    file_progress_walk_t walk = {visit, ctx, true};
    esp_err_t err = legacy_sd_progress_list(file_progress_visit_legacy, &walk);
    const char *roots[] = {BOOK_STATE_SD_ROOT, BOOK_STATE_FLASH_ROOT};
    for (unsigned i = 0; err == ESP_OK && walk.more && i < 2; ++i)
        if (book_state_ready(roots[i]))
            err = book_state_list(roots[i], "progress", file_progress_visit, &walk);
    return err;
}
typedef struct { const char *root; esp_err_t error; } file_progress_reset_t;
static bool file_progress_reset_one(const char *path, const void *data, size_t size, void *ctx) {
    (void)data; (void)size;
    file_progress_reset_t *reset = ctx;
    reset->error = book_progress_clear(path);
    if (reset->error == ESP_OK) reset->error = book_state_write(reset->root, path, "profile", "", 0);
    return reset->error == ESP_OK;
}
esp_err_t book_progress_reset_files(void) {
    const char *roots[] = {BOOK_STATE_SD_ROOT, BOOK_STATE_FLASH_ROOT};
    for (unsigned i = 0; i < 2; ++i) {
        if (!book_state_ready(roots[i])) continue;
        file_progress_reset_t reset = {roots[i], ESP_OK};
        esp_err_t err = book_state_list(roots[i], "progress", file_progress_reset_one, &reset);
        if (err != ESP_OK) return err;
        if (reset.error != ESP_OK) return reset.error;
        err = book_state_list(roots[i], "profile", file_progress_reset_one, &reset);
        if (err != ESP_OK) return err;
        if (reset.error != ESP_OK) return reset.error;
        file_library_t library = {.version = 1, .ignore_legacy = 1};
        err = book_state_write(roots[i], "", "library", &library, sizeof(library));
        if (err != ESP_OK) return err;
    }
    book_progress_notify_changed();
    return ESP_OK;
}
