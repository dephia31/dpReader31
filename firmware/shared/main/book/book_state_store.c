/* SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
 * Checked per-book records. A failed write keeps the preceding valid slot.
 */
#include "book_state_store.h"
#include <dirent.h>
#include <errno.h>
#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef ESP_PLATFORM
#include "read_pico_sd.h"
#include "book_store.h"
#include "esp_heap_caps.h"
#define state_alloc(size) heap_caps_malloc((size), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
#else
#define state_alloc(size) malloc(size)
#endif

typedef struct {
    uint32_t magic, generation, size, checksum;
    char key[BOOK_STATE_KEY_MAX];
} state_header_t;
#define STATE_MAGIC UINT32_C(0x32525044)

static uint32_t hash_bytes(uint32_t hash, const void *data, size_t size) {
    const uint8_t *bytes = data;
    for (size_t i = 0; i < size; ++i) hash = (hash ^ bytes[i]) * UINT32_C(16777619);
    return hash;
}
const char *book_state_root(const char *book) {
    return book && !strncmp(book, "/flash/books/", 13) ? BOOK_STATE_FLASH_ROOT : BOOK_STATE_SD_ROOT;
}
bool book_state_ready(const char *root) {
#ifdef ESP_PLATFORM
    if (!strcmp(root, BOOK_STATE_SD_ROOT)) {
        read_pico_sd_info_t info = {0};
        return read_pico_sd_get_info(&info) == ESP_OK && info.mounted;
    }
    return !strcmp(root, BOOK_STATE_FLASH_ROOT) && book_store_mount_flash() == ESP_OK;
#else
    struct stat st;
    return stat(root, &st) == 0 && S_ISDIR(st.st_mode);
#endif
}
static bool valid_kind(const char *kind) {
    return !strcmp(kind, "progress") || !strcmp(kind, "profile") ||
           !strcmp(kind, "library") || !strcmp(kind, "session");
}
static bool record_base(const char *root, const char *key, const char *kind, char *base, size_t cap) {
    if (!key || strnlen(key, BOOK_STATE_KEY_MAX) == BOOK_STATE_KEY_MAX || !valid_kind(kind)) return false;
    uint64_t hash = UINT64_C(14695981039346656037);
    for (const unsigned char *p = (const unsigned char *)key; *p; ++p)
        hash = (hash ^ *p) * UINT64_C(1099511628211);
    int n = snprintf(base, cap, "%s/books/%x/%016" PRIx64 ".%s", root, (unsigned)(hash & 15), hash, kind);
    return n > 0 && (size_t)n < cap;
}
static esp_err_t read_slot(const char *file, const char *key, state_header_t *header, void *data) {
    FILE *stream = fopen(file, "rb");
    if (!stream) return errno == ENOENT ? ESP_ERR_NOT_FOUND : ESP_FAIL;
    bool ok = fread(header, 1, sizeof(*header), stream) == sizeof(*header) &&
        header->magic == STATE_MAGIC && header->generation && header->size <= BOOK_STATE_DATA_MAX &&
        memchr(header->key, 0, sizeof(header->key));
    if (ok) ok = fread(data, 1, header->size, stream) == header->size && fgetc(stream) == EOF && !ferror(stream);
    if (fclose(stream)) ok = false;
    if (!ok) return ESP_ERR_INVALID_RESPONSE;
    uint32_t expected = header->checksum;
    header->checksum = 0;
    uint32_t hash = hash_bytes(UINT32_C(2166136261), header, sizeof(*header));
    header->checksum = expected;
    if (hash_bytes(hash, data, header->size) != expected) return ESP_ERR_INVALID_RESPONSE;
    if (key && strcmp(header->key, key)) return ESP_ERR_INVALID_STATE;
    return ESP_OK;
}
static esp_err_t read_pair(const char *base, const char *key, state_header_t *header,
                           void *data, unsigned *selected) {
    uint8_t *other = state_alloc(BOOK_STATE_DATA_MAX);
    if (!other) return ESP_ERR_NO_MEM;
    state_header_t candidate;
    char file[384];
    snprintf(file, sizeof(file), "%s.0", base);
    esp_err_t first = read_slot(file, key, header, data);
    snprintf(file, sizeof(file), "%s.1", base);
    esp_err_t second = read_slot(file, key, &candidate, other);
    *selected = 0;
    if (second == ESP_OK && (first != ESP_OK || candidate.generation > header->generation)) {
        *header = candidate;
        memcpy(data, other, candidate.size);
        *selected = 1;
    }
    free(other);
    if (first == ESP_ERR_INVALID_STATE || second == ESP_ERR_INVALID_STATE) return ESP_ERR_INVALID_STATE;
    if (first == ESP_OK || second == ESP_OK) return ESP_OK;
    return first == ESP_ERR_NOT_FOUND && second == ESP_ERR_NOT_FOUND ? ESP_ERR_NOT_FOUND : ESP_ERR_INVALID_RESPONSE;
}
esp_err_t book_state_read(const char *root, const char *key, const char *kind, void *data, size_t *size) {
    if (!data || !size || !book_state_ready(root)) return ESP_ERR_INVALID_STATE;
    char base[376];
    if (!record_base(root, key, kind, base, sizeof(base))) return ESP_ERR_INVALID_ARG;
    uint8_t *buffer = state_alloc(BOOK_STATE_DATA_MAX);
    if (!buffer) return ESP_ERR_NO_MEM;
    state_header_t header;
    unsigned selected;
    esp_err_t err = read_pair(base, key, &header, buffer, &selected);
    if (err == ESP_OK) {
        if (*size < header.size) err = ESP_ERR_INVALID_SIZE;
        else { memcpy(data, buffer, header.size); *size = header.size; }
    }
    free(buffer);
    return err;
}
static bool directory(const char *path) {
    if (mkdir(path, 0775) == 0) return true;
    struct stat st;
    return errno == EEXIST && stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}
esp_err_t book_state_write(const char *root, const char *key, const char *kind, const void *data, size_t size) {
    if (!data || size > BOOK_STATE_DATA_MAX) return ESP_ERR_INVALID_ARG;
    if (!book_state_ready(root)) return ESP_ERR_INVALID_STATE;
    char base[376], folder[376], file[384];
    if (!record_base(root, key, kind, base, sizeof(base))) return ESP_ERR_INVALID_ARG;
    if (!directory(root)) return ESP_FAIL;
    snprintf(folder, sizeof(folder), "%s/books", root);
    if (!directory(folder)) return ESP_FAIL;
    snprintf(folder, sizeof(folder), "%s", base);
    *strrchr(folder, '/') = 0;
    if (!directory(folder)) return ESP_FAIL;
    uint8_t *previous = state_alloc(BOOK_STATE_DATA_MAX);
    if (!previous) return ESP_ERR_NO_MEM;
    state_header_t header = {0};
    unsigned selected = 1;
    esp_err_t err = read_pair(base, key, &header, previous, &selected);
    free(previous);
    if (err != ESP_OK && err != ESP_ERR_NOT_FOUND) return err;
    if (err == ESP_OK && header.generation == UINT32_MAX) return ESP_ERR_INVALID_STATE;
    uint32_t generation = err == ESP_OK ? header.generation + 1 : 1;
    header = (state_header_t){.magic = STATE_MAGIC, .generation = generation, .size = size};
    memcpy(header.key, key, strlen(key) + 1);
    header.checksum = hash_bytes(hash_bytes(UINT32_C(2166136261), &header, sizeof(header)), data, size);
    snprintf(file, sizeof(file), "%s.%u", base, selected ^ 1u);
    FILE *stream = fopen(file, "wb");
    if (!stream) return ESP_FAIL;
    bool ok = fwrite(&header, 1, sizeof(header), stream) == sizeof(header) &&
        fwrite(data, 1, size, stream) == size && fflush(stream) == 0 && fsync(fileno(stream)) == 0;
    if (fclose(stream)) ok = false;
    if (!ok) (void)unlink(file);
    return ok ? ESP_OK : ESP_FAIL;
}
esp_err_t book_state_list(const char *root, const char *kind, book_state_visit_fn visit, void *ctx) {
    if (!visit || !valid_kind(kind)) return ESP_ERR_INVALID_ARG;
    if (!book_state_ready(root)) return ESP_ERR_INVALID_STATE;
    uint8_t *data = state_alloc(BOOK_STATE_DATA_MAX);
    if (!data) return ESP_ERR_NO_MEM;
    esp_err_t err = ESP_OK;
    bool more = true;
    for (unsigned shard = 0; more && err == ESP_OK && shard < 16; ++shard) {
        char folder[376];
        snprintf(folder, sizeof(folder), "%s/books/%x", root, shard);
        DIR *dir = opendir(folder);
        if (!dir) { if (errno != ENOENT) err = ESP_FAIL; continue; }
        struct dirent *entry;
        while (more && (entry = readdir(dir)) != NULL) {
            size_t length = strlen(entry->d_name), kind_length = strlen(kind);
            if (length != 16 + 1 + kind_length + 2 || entry->d_name[16] != '.' ||
                strncmp(entry->d_name + 17, kind, kind_length) ||
                entry->d_name[length - 2] != '.' ||
                (entry->d_name[length - 1] != '0' && entry->d_name[length - 1] != '1')) continue;
            char base[376], sibling[384];
            int n = snprintf(base, sizeof(base), "%s/%.*s", folder, (int)length - 2, entry->d_name);
            if (n < 0 || (size_t)n >= sizeof(base)) { err = ESP_ERR_INVALID_SIZE; break; }
            if (entry->d_name[length - 1] == '1') {
                snprintf(sibling, sizeof(sibling), "%s.0", base);
                struct stat st;
                if (stat(sibling, &st) == 0) continue;
                if (errno != ENOENT) { err = ESP_FAIL; break; }
            }
            state_header_t header;
            unsigned selected;
            err = read_pair(base, NULL, &header, data, &selected);
            if (err != ESP_OK) break;
            char expected[376];
            if (!record_base(root, header.key, kind, expected, sizeof(expected)) || strcmp(base, expected)) {
                err = ESP_ERR_INVALID_RESPONSE; break;
            }
            more = visit(header.key, data, header.size, ctx);
        }
        closedir(dir);
    }
    free(data);
    return err;
}
