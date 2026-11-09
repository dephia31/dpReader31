#include "book_progress.h"
/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：把阅读进度、时长、书签、收藏、自定义书名和移出书架状态流式备份到 TF 卡。
 * English: Stream progress, reading time, bookmarks, favorites, titles and shelf removals to TF.
 */
#include "book_history_backup.h"
#include <stdlib.h>
#include <string.h>
#include "nvs.h"

#if defined(ESP_PLATFORM) && DPREADER_BOARD_METALIO47
#include "esp_heap_caps.h"
#define history_alloc(size) heap_caps_malloc((size), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
#else
#define history_alloc(size) malloc(size)
#endif

#if (defined(ESP_PLATFORM) && DPREADER_BOARD_METALIO47) || defined(BOOK_STATE_STORE_TEST)
#include "book_state_store.h"
#define HISTORY_FILES 1
#else
#define HISTORY_FILES 0
#endif

#define HISTORY_MAX_RECORDS 16384
#define HISTORY_MAX_VALUE 4096
#define HISTORY_RECORD_HEADER 20

static const char *const namespaces[] = {
#if DPREADER_BOARD_METALIO47
    "mt_books", "mt_ticket", "mt_marks", "mt_favs", "mt_titles", "mt_shelf"
#else
    "rp_books", "rp_ticket", "rp_marks", "rp_favs", "rp_titles", "rp_shelf"
#endif
};

#if HISTORY_FILES
static bool history_nonempty(const char *path,const book_progress_t *v,void *ctx) {
    (void)path;(void)v;*(bool*)ctx=false;return false;
}
static bool history_profile_nonempty(const char *path,const void *data,size_t size,void *ctx) {
    (void)path;(void)data;if(size)*(bool*)ctx=false;return !size;
}
#endif
esp_err_t book_history_backup_empty(bool *empty) {
    if (!empty) return ESP_ERR_INVALID_ARG;
    *empty = true;
#if HISTORY_FILES
    esp_err_t state=book_progress_list(history_nonempty,empty);
    if(state!=ESP_OK||!*empty)return state;
    const char *roots[]={BOOK_STATE_SD_ROOT,BOOK_STATE_FLASH_ROOT};
    for(unsigned i=0;i<2;++i)if(book_state_ready(roots[i])) {
        state=book_state_list(roots[i],"profile",history_profile_nonempty,empty);
        if(state!=ESP_OK||!*empty)return state;
    }
#endif
    for (unsigned ns = 0; ns < sizeof(namespaces) / sizeof(namespaces[0]); ++ns) {
#if HISTORY_FILES
        if(ns==0)continue;
#endif
        nvs_handle_t h;
        esp_err_t err = nvs_open(namespaces[ns], NVS_READONLY, &h);
        if (err == ESP_ERR_NVS_NOT_FOUND) continue;
        if (err != ESP_OK) return err;
        nvs_iterator_t it = NULL;
        err = nvs_entry_find_in_handle(h, NVS_TYPE_ANY, &it);
        nvs_release_iterator(it);
        nvs_close(h);
        if (err == ESP_OK) { *empty = false; return ESP_OK; }
        if (err != ESP_ERR_NVS_NOT_FOUND) return err;
    }
    return ESP_OK;
}

static uint32_t hash_bytes(uint32_t hash, const void *data, size_t len) {
    const uint8_t *bytes = data;
    for (size_t i = 0; i < len; ++i) hash = (hash ^ bytes[i]) * UINT32_C(16777619);
    return hash;
}

static bool book_key(const char *key, char prefix) {
    if (key[0] != prefix || key[1] != '_' || strlen(key) != 10) return false;
    for (int i = 2; i < 10; ++i)
        if (!((key[i] >= '0' && key[i] <= '9') || (key[i] >= 'a' && key[i] <= 'f'))) return false;
    return true;
}

static bool valid_record(unsigned ns, uint8_t type, const char *key, const uint8_t *data, size_t len) {
#if HISTORY_FILES
    if(ns==6) {
        return type==NVS_TYPE_BLOB&&!strcmp(key,"profile")&&len>=BOOK_STATE_KEY_MAX&&len<=HISTORY_MAX_VALUE&&
            strnlen((const char*)data,BOOK_STATE_KEY_MAX)<BOOK_STATE_KEY_MAX&&
            (!strncmp((const char*)data,"/sdcard/",8)||!strncmp((const char*)data,"/flash/books/",13));
    }
#endif
    if (ns >= sizeof(namespaces) / sizeof(namespaces[0]) || !key[0] ||
        strnlen(key, 16) == 16 || !len || len > HISTORY_MAX_VALUE) return false;
    if (ns == 0) {
        if (!strcmp(key, "seq")) return type == NVS_TYPE_U32 && len == 4;
        if (!strcmp(key, "last")) return type == NVS_TYPE_STR && len >= 1 &&
            data[len - 1] == 0 && strnlen((const char *)data, len) == len - 1;
        return book_key(key, 'b') && type == NVS_TYPE_BLOB && len >= 26 &&
            len <= 24 + 288 && !memcmp(data, "RPB", 3) && (data[3] == 1 || data[3] == 2) &&
            ((size_t)data[22] | ((size_t)data[23] << 8)) == len - 24 &&
            strnlen((const char *)data + 24, len - 24) == len - 25;
    }
    if (ns == 1) {
        uint32_t magic = 0;
        if (len >= 4) memcpy(&magic, data, 4);
        if (!strcmp(key, "stats")) return type == NVS_TYPE_BLOB && len == 32 && magic == UINT32_C(0x52505431);
        if (!strcmp(key, "heat32")) return type == NVS_TYPE_BLOB && len == 260 && magic == UINT32_C(0x52504832);
        if (!strcmp(key, "heatmap")) return type == NVS_TYPE_BLOB && len == 244 && magic == UINT32_C(0x52504831);
        return false;
    }
    if (ns == 2) return book_key(key, 'm') && type == NVS_TYPE_BLOB && len >= 300;
    if (ns == 3) return book_key(key, 'f') && type == NVS_TYPE_U8 && len == 1 && data[0] <= 1;
    if (ns == 4) return book_key(key, 't') && type == NVS_TYPE_BLOB && len >= 4;
    // 新命名空间追加在末尾，旧备份的索引保持不变；完整路径与键必须属于同一本书。
    // Append the namespace to preserve legacy indices; the full path must match its hashed key.
    if (!book_key(key, 'h') || type != NVS_TYPE_STR || len < 2 || len > 288 ||
        data[0] != '/' || data[len - 1] != 0 || strnlen((const char *)data, len) != len - 1)
        return false;
    char expected[11];
    snprintf(expected, sizeof(expected), "h_%08lx",
             (unsigned long)hash_bytes(UINT32_C(2166136261), data, len - 1));
    return !strcmp(key, expected);
}

static esp_err_t read_value(nvs_handle_t h, const nvs_entry_info_t *info, uint8_t **value, size_t *len) {
    *value = NULL;
    *len = 0;
    esp_err_t err;
    if (info->type == NVS_TYPE_U8) *len = 1;
    else if (info->type == NVS_TYPE_U32) *len = 4;
    else if (info->type == NVS_TYPE_STR) err = nvs_get_str(h, info->key, NULL, len);
    else if (info->type == NVS_TYPE_BLOB) err = nvs_get_blob(h, info->key, NULL, len);
    else return ESP_ERR_INVALID_ARG;
    if ((info->type == NVS_TYPE_STR || info->type == NVS_TYPE_BLOB) && err != ESP_OK) return err;
    if (!*len || *len > HISTORY_MAX_VALUE) return ESP_ERR_INVALID_SIZE;
    *value = history_alloc(*len);
    if (!*value) return ESP_ERR_NO_MEM;
    if (info->type == NVS_TYPE_U8) err = nvs_get_u8(h, info->key, *value);
    else if (info->type == NVS_TYPE_U32) err = nvs_get_u32(h, info->key, (uint32_t *)*value);
    else if (info->type == NVS_TYPE_STR) err = nvs_get_str(h, info->key, (char *)*value, len);
    else err = nvs_get_blob(h, info->key, *value, len);
    if (err != ESP_OK) { free(*value); *value = NULL; }
    return err;
}

#if HISTORY_FILES
typedef struct {FILE *file;uint32_t *hash;uint16_t *count;esp_err_t error;} history_files_t;
static bool history_file_record(history_files_t *ctx,unsigned ns,uint8_t type,const char *key,const void *data,size_t size) {
    if(*ctx->count==HISTORY_MAX_RECORDS||!valid_record(ns,type,key,data,size)){ctx->error=ESP_ERR_INVALID_STATE;return false;}
    uint8_t head[HISTORY_RECORD_HEADER]={(uint8_t)ns,type};
    memcpy(head+2,key,strlen(key));head[18]=size;head[19]=size>>8;
    if(fwrite(head,1,sizeof(head),ctx->file)!=sizeof(head)||fwrite(data,1,size,ctx->file)!=size){ctx->error=ESP_FAIL;return false;}
    *ctx->hash=hash_bytes(hash_bytes(*ctx->hash,head,sizeof(head)),data,size);++*ctx->count;return true;
}
static void history_put32(uint8_t *p,uint32_t n){for(unsigned i=0;i<4;++i)p[i]=n>>(8*i);}
static uint32_t history_get32(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static bool history_progress(const char *path,const book_progress_t *v,void *opaque) {
    uint8_t data[24+288]={0};size_t len=strlen(path)+1;if(len>288)return false;
    memcpy(data,"RPB\2",4);history_put32(data+4,v->file_size);data[8]=v->chapter;data[9]=v->chapter>>8;
    history_put32(data+10,v->byte_off);data[14]=v->px;data[15]=v->pct;history_put32(data+16,v->last_open_s);
    data[22]=len;data[23]=len>>8;memcpy(data+24,path,len);
    char key[16];snprintf(key,sizeof(key),"b_%08lx",(unsigned long)hash_bytes(2166136261u,path,len-1));
    return history_file_record(opaque,0,NVS_TYPE_BLOB,key,data,24+len);
}
static bool history_profile(const char *path,const void *data,size_t size,void *opaque) {
    history_files_t *ctx=opaque;
    if(!size)return true;
    if(size>HISTORY_MAX_VALUE-BOOK_STATE_KEY_MAX){ctx->error=ESP_ERR_INVALID_SIZE;return false;}
    uint8_t *buffer=history_alloc(BOOK_STATE_KEY_MAX+size);
    if(!buffer){ctx->error=ESP_ERR_NO_MEM;return false;}
    memset(buffer,0,BOOK_STATE_KEY_MAX);strcpy((char*)buffer,path);memcpy(buffer+BOOK_STATE_KEY_MAX,data,size);
    bool ok=history_file_record(ctx,6,NVS_TYPE_BLOB,"profile",buffer,BOOK_STATE_KEY_MAX+size);free(buffer);return ok;
}
static esp_err_t history_restore_file(unsigned ns,const char *key,const uint8_t *data,size_t size) {
    if(ns==6)return book_state_write(book_state_root((const char*)data),(const char*)data,"profile",data+BOOK_STATE_KEY_MAX,size-BOOK_STATE_KEY_MAX);
    if(!strcmp(key,"seq"))return ESP_OK;
    if(!strcmp(key,"last"))return book_progress_set_last_path((const char*)data);
    book_progress_t v={.file_size=history_get32(data+4),.chapter=data[8]|(uint16_t)data[9]<<8,
        .byte_off=history_get32(data+10),.px=data[14],.pct=data[15],.last_open_s=data[3]==2?history_get32(data+16):0};
    return book_progress_import((const char*)data+24,&v);
}
#endif

esp_err_t book_history_backup_write(FILE *file) {
    if (!file) return ESP_ERR_INVALID_ARG;
    static const char magic[8] = {'R','P','H','I','S','T',HISTORY_FILES ? '2' : '1',0};
    if (fwrite(magic, 1, 8, file) != 8) return ESP_FAIL;
    uint32_t hash = UINT32_C(2166136261);
    uint16_t count = 0;
    for (unsigned ns = 0; ns < sizeof(namespaces) / sizeof(namespaces[0]); ++ns) {
#if HISTORY_FILES
        if(ns==0)continue;
#endif
        nvs_handle_t h;
        esp_err_t err = nvs_open(namespaces[ns], NVS_READONLY, &h);
        if (err == ESP_ERR_NVS_NOT_FOUND) continue;
        if (err != ESP_OK) return err;
        nvs_iterator_t it = NULL;
        err = nvs_entry_find_in_handle(h, NVS_TYPE_ANY, &it);
        while (err == ESP_OK) {
            nvs_entry_info_t info;
            err = nvs_entry_info(it, &info);
            if (err != ESP_OK) break;
            uint8_t *value = NULL;
            size_t len = 0;
            err = read_value(h, &info, &value, &len);
            if (err == ESP_OK && (!valid_record(ns, info.type, info.key, value, len) || count == HISTORY_MAX_RECORDS))
                err = ESP_ERR_INVALID_STATE;
            if (err == ESP_OK) {
                uint8_t head[HISTORY_RECORD_HEADER] = {(uint8_t)ns, (uint8_t)info.type};
                memcpy(head + 2, info.key, strlen(info.key));
                head[18] = (uint8_t)len; head[19] = (uint8_t)(len >> 8);
                if (fwrite(head, 1, sizeof(head), file) != sizeof(head) ||
                    fwrite(value, 1, len, file) != len) err = ESP_FAIL;
                else {
                    hash = hash_bytes(hash, head, sizeof(head));
                    hash = hash_bytes(hash, value, len);
                    ++count;
                }
            }
            free(value);
            if (err != ESP_OK) break;
            err = nvs_entry_next(&it);
        }
        nvs_release_iterator(it);
        nvs_close(h);
        if (err != ESP_ERR_NVS_NOT_FOUND) return err;
    }
#if HISTORY_FILES
    history_files_t files={file,&hash,&count,ESP_OK};
    esp_err_t result=book_progress_list(history_progress,&files);
    if(result!=ESP_OK)return result;
    if(files.error!=ESP_OK)return files.error;
    char last[288];
    if(book_progress_last_path(last,sizeof(last))&&!history_file_record(&files,0,NVS_TYPE_STR,"last",last,strlen(last)+1))return files.error;
    const char *roots[]={BOOK_STATE_SD_ROOT,BOOK_STATE_FLASH_ROOT};
    for(unsigned i=0;i<2;++i)if(book_state_ready(roots[i])) {
        result=book_state_list(roots[i],"profile",history_profile,&files);
        if(result!=ESP_OK)return result;
        if(files.error!=ESP_OK)return files.error;
    }
#endif
    uint8_t end[HISTORY_RECORD_HEADER] = {0xff};
    uint8_t seal[6] = {(uint8_t)count, (uint8_t)(count >> 8),
        (uint8_t)hash, (uint8_t)(hash >> 8), (uint8_t)(hash >> 16), (uint8_t)(hash >> 24)};
    return fwrite(end, 1, sizeof(end), file) == sizeof(end) &&
           fwrite(seal, 1, sizeof(seal), file) == sizeof(seal) ? ESP_OK : ESP_FAIL;
}

static esp_err_t read_stream(FILE *file, bool restore) {
    char magic[8];
    if (fread(magic, 1, sizeof(magic), file) != sizeof(magic) || (memcmp(magic, "RPHIST1", 8) && (!HISTORY_FILES || memcmp(magic, "RPHIST2", 8))))
        return ESP_ERR_INVALID_RESPONSE;
    uint32_t hash = UINT32_C(2166136261);
    uint16_t count = 0;
    nvs_handle_t h = 0;
    int open_ns = -1;
    esp_err_t result = ESP_OK;
    for (;;) {
        uint8_t head[HISTORY_RECORD_HEADER];
        if (fread(head, 1, sizeof(head), file) != sizeof(head)) { result = ESP_ERR_INVALID_RESPONSE; break; }
        if (head[0] == 0xff) {
            for (unsigned i = 1; i < sizeof(head); ++i)
                if (head[i]) { result = ESP_ERR_INVALID_RESPONSE; break; }
            break;
        }
        unsigned ns = head[0];
        uint8_t type = head[1];
        char key[16]; memcpy(key, head + 2, sizeof(key));
        size_t len = (size_t)head[18] | ((size_t)head[19] << 8);
        if (result != ESP_OK || ns >= sizeof(namespaces) / sizeof(namespaces[0]) + HISTORY_FILES ||
            strnlen(key, sizeof(key)) == sizeof(key) || !len || len > HISTORY_MAX_VALUE ||
            count == HISTORY_MAX_RECORDS) { result = ESP_ERR_INVALID_RESPONSE; break; }
        uint8_t *value = history_alloc(len);
        if (!value) { result = ESP_ERR_NO_MEM; break; }
        if (fread(value, 1, len, file) != len || !valid_record(ns, type, key, value, len)) {
            free(value); result = ESP_ERR_INVALID_RESPONSE; break;
        }
        hash = hash_bytes(hash, head, sizeof(head));
        hash = hash_bytes(hash, value, len);
        ++count;
        if (restore) {
#if HISTORY_FILES
            if(ns==0||ns==6)result=history_restore_file(ns,key,value,len);
            else {
#endif
            if (open_ns != (int)ns) {
                if (open_ns >= 0) { result = nvs_commit(h); nvs_close(h); }
                open_ns = -1;
                if (result == ESP_OK) result = nvs_open(namespaces[ns], NVS_READWRITE, &h);
                if (result == ESP_OK) open_ns = (int)ns;
            }
            if (result == ESP_OK) {
                if (type == NVS_TYPE_U8) result = nvs_set_u8(h, key, value[0]);
                else if (type == NVS_TYPE_U32) { uint32_t v; memcpy(&v, value, 4); result = nvs_set_u32(h, key, v); }
                else if (type == NVS_TYPE_STR) result = nvs_set_str(h, key, (const char *)value);
                else result = nvs_set_blob(h, key, value, len);
            }
        }
#if HISTORY_FILES
        }
#endif
        free(value);
        if (result != ESP_OK) break;
    }
    if (open_ns >= 0) {
        esp_err_t commit = result == ESP_OK ? nvs_commit(h) : result;
        nvs_close(h);
        if (result == ESP_OK) result = commit;
        if (restore) book_progress_notify_changed();
    }
    if (result != ESP_OK) return result;
    uint8_t seal[6];
    if (fread(seal, 1, sizeof(seal), file) != sizeof(seal)) return ESP_ERR_INVALID_RESPONSE;
    uint16_t stored_count = (uint16_t)seal[0] | (uint16_t)seal[1] << 8;
    uint32_t stored_hash = (uint32_t)seal[2] | (uint32_t)seal[3] << 8 |
                           (uint32_t)seal[4] << 16 | (uint32_t)seal[5] << 24;
    return stored_count == count && stored_hash == hash && fgetc(file) == EOF && !ferror(file)
        ? ESP_OK : ESP_ERR_INVALID_RESPONSE;
}

bool book_history_backup_validate(FILE *file) { return read_stream(file, false) == ESP_OK; }
esp_err_t book_history_backup_restore(FILE *file) {
    if (!file) return ESP_ERR_INVALID_ARG;
    long start = ftell(file);
    if (start < 0 || !book_history_backup_validate(file) || fseek(file, start, SEEK_SET))
        return ESP_ERR_INVALID_RESPONSE;
    // Recovery replaces the old reading library. Merging leaves stale "last"
    // and per-book records behind, so the restored shelf can appear unchanged.
    // 恢复应替换旧阅读资料；合并会留下备份外的旧书进度与最近阅读排序。
#if HISTORY_FILES
    esp_err_t reset=book_progress_reset_files();
    if(reset!=ESP_OK)return reset;
#endif
    for (unsigned ns = 0; ns < sizeof(namespaces) / sizeof(namespaces[0]); ++ns) {
#if HISTORY_FILES
        if(ns==0)continue;
#endif
        nvs_handle_t h;
        esp_err_t err = nvs_open(namespaces[ns], NVS_READWRITE, &h);
        if (err != ESP_OK) return err;
        err = nvs_erase_all(h);
        if (err == ESP_OK) err = nvs_commit(h);
        nvs_close(h);
        book_progress_notify_changed();
        if (err != ESP_OK) return err;
    }
    return read_stream(file, true);
}
