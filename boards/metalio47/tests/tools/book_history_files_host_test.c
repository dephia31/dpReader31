/* SPDX-License-Identifier: Apache-2.0 */
/* Round-trip the real streaming history format through an in-memory NVS. */
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "nvs.h"
#include "../main/book/book_history_backup.c"

typedef struct { int ns; char key[16]; nvs_type_t type; size_t len; uint8_t data[1024]; } record_t;
static record_t records[2][32];
static int counts[2], active;

struct nvs_iterator_opaque { int ns, index; };

static int namespace_index(const char *name) {
    for (unsigned i = 0; i < sizeof(namespaces) / sizeof(namespaces[0]); ++i)
        if (!strcmp(name, namespaces[i])) return (int)i;
    return -1;
}
static record_t *find_record(int ns, const char *key) {
    for (int i = 0; i < counts[active]; ++i)
        if (records[active][i].ns == ns && !strcmp(records[active][i].key, key)) return &records[active][i];
    return NULL;
}
static esp_err_t put(int ns, const char *key, nvs_type_t type, const void *data, size_t len) {
    if (ns < 0 || len > 1024) return ESP_ERR_INVALID_ARG;
    record_t *r = find_record(ns, key);
    if (!r) { assert(counts[active] < 32); r = &records[active][counts[active]++]; }
    memset(r, 0, sizeof(*r)); r->ns = ns; strcpy(r->key, key); r->type = type; r->len = len;
    memcpy(r->data, data, len); return ESP_OK;
}
static esp_err_t get(int ns, const char *key, nvs_type_t type, void *data, size_t *len) {
    record_t *r = find_record(ns, key);
    if (!r || r->type != type) return ESP_ERR_NVS_NOT_FOUND;
    if (data && *len < r->len) return ESP_ERR_INVALID_SIZE;
    if (data) memcpy(data, r->data, r->len);
    *len = r->len; return ESP_OK;
}
esp_err_t nvs_open(const char *ns, int mode, nvs_handle_t *h) {
    int id = namespace_index(ns);
    if (id < 0) return ESP_ERR_NVS_NOT_FOUND;
    if (mode == NVS_READONLY) {
        bool found = false;
        for (int i = 0; i < counts[active]; ++i) if (records[active][i].ns == id) found = true;
        if (!found) return ESP_ERR_NVS_NOT_FOUND;
    }
    *h = id + 1; return ESP_OK;
}
void nvs_close(nvs_handle_t h) { (void)h; }
esp_err_t nvs_commit(nvs_handle_t h) { (void)h; return ESP_OK; }
esp_err_t nvs_erase_all(nvs_handle_t h) {
    int ns = h - 1;
    for (int i = 0; i < counts[active];)
        if (records[active][i].ns == ns) {
            memmove(&records[active][i], &records[active][i + 1],
                    (size_t)(--counts[active] - i) * sizeof(records[active][i]));
        } else ++i;
    return ESP_OK;
}
esp_err_t nvs_get_u8(nvs_handle_t h, const char *key, uint8_t *v) { size_t n=1; return get(h-1,key,NVS_TYPE_U8,v,&n); }
esp_err_t nvs_set_u8(nvs_handle_t h, const char *key, uint8_t v) { return put(h-1,key,NVS_TYPE_U8,&v,1); }
esp_err_t nvs_get_u32(nvs_handle_t h, const char *key, uint32_t *v) { size_t n=4; return get(h-1,key,NVS_TYPE_U32,v,&n); }
esp_err_t nvs_set_u32(nvs_handle_t h, const char *key, uint32_t v) { return put(h-1,key,NVS_TYPE_U32,&v,4); }
esp_err_t nvs_get_str(nvs_handle_t h, const char *key, char *v, size_t *n) { return get(h-1,key,NVS_TYPE_STR,v,n); }
esp_err_t nvs_set_str(nvs_handle_t h, const char *key, const char *v) { return put(h-1,key,NVS_TYPE_STR,v,strlen(v)+1); }
esp_err_t nvs_get_blob(nvs_handle_t h, const char *key, void *v, size_t *n) { return get(h-1,key,NVS_TYPE_BLOB,v,n); }
esp_err_t nvs_set_blob(nvs_handle_t h, const char *key, const void *v, size_t n) { return put(h-1,key,NVS_TYPE_BLOB,v,n); }
esp_err_t nvs_entry_find_in_handle(nvs_handle_t h, nvs_type_t type, nvs_iterator_t *it) {
    assert(type == NVS_TYPE_ANY);
    for (int i = 0; i < counts[active]; ++i) if (records[active][i].ns == h - 1) {
        *it = malloc(sizeof(**it)); assert(*it); **it = (struct nvs_iterator_opaque){h-1,i}; return ESP_OK;
    }
    *it = NULL; return ESP_ERR_NVS_NOT_FOUND;
}
esp_err_t nvs_entry_next(nvs_iterator_t *it) {
    for (int i = (*it)->index + 1; i < counts[active]; ++i)
        if (records[active][i].ns == (*it)->ns) { (*it)->index = i; return ESP_OK; }
    free(*it); *it = NULL; return ESP_ERR_NVS_NOT_FOUND;
}
esp_err_t nvs_entry_info(nvs_iterator_t it, nvs_entry_info_t *info) {
    record_t *r = &records[active][it->index];
    memset(info,0,sizeof(*info)); strcpy(info->key,r->key); info->type=r->type; return ESP_OK;
}
void nvs_release_iterator(nvs_iterator_t it) { free(it); }


esp_err_t nvs_entry_find(const char *part,const char *ns,nvs_type_t type,nvs_iterator_t *it) {
    (void)part;(void)type;nvs_handle_t h;esp_err_t err=nvs_open(ns,NVS_READONLY,&h);
    return err==ESP_OK?nvs_entry_find_in_handle(h,NVS_TYPE_ANY,it):err;
}
esp_err_t nvs_erase_key(nvs_handle_t h,const char *key) {
    record_t *r=find_record(h-1,key);if(!r)return ESP_ERR_NVS_NOT_FOUND;
    int i=r-records[active];memmove(r,r+1,(--counts[active]-i)*sizeof(*r));return ESP_OK;
}
#include <sys/stat.h>
int main(void) {
    mkdir(BOOK_STATE_SD_ROOT,0775);mkdir(BOOK_STATE_FLASH_ROOT,0775);
    const char *a="/sdcard/books/a.epub",*b="/sdcard/books/b.epub";
    book_progress_t v={.file_size=900,.chapter=3,.byte_off=420,.px=32,.pct=40},out;
    assert(book_progress_save(a,&v)==ESP_OK);assert(book_progress_set_last_path(a)==ESP_OK);
    assert(book_state_write(BOOK_STATE_SD_ROOT,a,"profile","profile",8)==ESP_OK);
    bool empty=true;assert(book_history_backup_empty(&empty)==ESP_OK&&!empty);
    FILE *f=tmpfile();assert(f);assert(book_history_backup_write(f)==ESP_OK);
    rewind(f);assert(book_history_backup_validate(f));
    assert(book_progress_save(b,&v)==ESP_OK);
    assert(book_state_write(BOOK_STATE_SD_ROOT,b,"profile","other",6)==ESP_OK);
    v.byte_off=800;assert(book_progress_save(a,&v)==ESP_OK);
    rewind(f);assert(book_history_backup_restore(f)==ESP_OK);
    assert(book_progress_load(a,900,&out)&&out.byte_off==420);
    assert(!book_progress_load(b,900,&out));
    char data[32];size_t size=sizeof(data);
    assert(book_state_read(BOOK_STATE_SD_ROOT,a,"profile",data,&size)==ESP_OK&&size==8&&!strcmp(data,"profile"));
    size=sizeof(data);assert(book_state_read(BOOK_STATE_SD_ROOT,b,"profile",data,&size)==ESP_OK&&size==0);
    char last[288];assert(book_progress_last_path(last,sizeof(last))&&!strcmp(last,a));
    fseek(f,12,SEEK_SET);fputc(0xfe,f);rewind(f);
    assert(book_history_backup_restore(f)==ESP_ERR_INVALID_RESPONSE);
    assert(book_progress_load(a,900,&out)&&out.byte_off==420);
    fclose(f);
    uint32_t old_sequence=4;assert(put(0,"seq",NVS_TYPE_U32,&old_sequence,4)==ESP_OK);
    assert(book_progress_reset_files()==ESP_OK);
    assert(book_history_backup_empty(&empty)==ESP_OK&&empty);
    puts("PASS effective SD history/profile backup, replacement restore and corrupt backup rejection");return 0;
}
