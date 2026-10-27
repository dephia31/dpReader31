/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：流式 TXT 编码识别、目录与按章转码。
 * English: Streaming TXT encoding detection, indexing and chapter conversion.
 * 冻结：原始偏移用于进度；目录上限 2048；不加载整本书。
 * Frozen: Progress uses source offsets; at most 2048 entries; never load the whole book.
 */
#include "book_source_internal.h"
#include "book_index_cache.h"
#include "gbk.h"
#include "esp_heap_caps.h"
#include "../font/vn_diacritics.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#define READ_BLOCK (64 * 1024)
#define PART_TARGET (48 * 1024)
#if KIIKO_BOARD_METALIO47
// 旧索引可能把UTF16当GBK；只重建TXT目录缓存，不改变文件与阅读进度。
// Old indexes may have guessed GBK for UTF16; rebuild only TXT indexes, retaining files/progress.
#define TXT_CACHE_VERSION 2u
#else
#define TXT_CACHE_VERSION 1u
#endif
typedef struct {
    uint32_t total;
    uint32_t bom;
    uint32_t count;
    uint8_t gbk;
    uint8_t reserved[3];
} txt_cache_payload_t;
typedef struct {
    FILE *file; ///< 只读句柄 / Read-only handle
    unsigned char bytes[READ_BLOCK + 4]; ///< 跨块字符余量 / Room for a cross-block character
    size_t pos, used; ///< 缓冲游标 / Buffer cursors
    uint32_t offset; ///< 原文件游标 / Source cursor
    bool gbk; ///< 编码 / Encoding
#if KIIKO_BOARD_METALIO47
    uint8_t utf16; ///< UTF16字节序 / UTF16 byte order
#endif
} reader_t;

// 验证一个 UTF-8 标量；无效时仅消费首字节。/ Validate one UTF-8 scalar; consume only its first byte on error.
static size_t utf8_one(const unsigned char *s, size_t n, uint32_t *cp) {
    *cp = 0xfffd;
    if (!n) return 0;
    if (s[0] < 0x80) { *cp = s[0]; return 1; }
    size_t k = s[0] >= 0xc2 && s[0] <= 0xdf ? 2 : s[0] >= 0xe0 && s[0] <= 0xef ? 3 : s[0] >= 0xf0 && s[0] <= 0xf4 ? 4 : 0;
    if (!k || k > n) return 1;
    uint32_t c = s[0] & (0x7f >> k);
    for (size_t j = 1; j < k; ++j) { if ((s[j] & 0xc0) != 0x80) return 1; c = (c << 6) | (s[j] & 63); }
    if ((k == 2 && c < 0x80) || (k == 3 && c < 0x800) || (k == 4 && c < 0x10000) || (c >= 0xd800 && c <= 0xdfff) || c > 0x10ffff) return 1;
    *cp = c; return k;
}
static size_t encode(uint32_t cp, char out[4]) {
    if (!cp) cp = 0xfffd;
    if (cp < 0x80) { out[0] = cp; return 1; }
    if (cp < 0x800) { out[0] = 0xc0 | (cp >> 6); out[1] = 0x80 | (cp & 63); return 2; }
    if (cp < 0x10000) { out[0] = 0xe0 | (cp >> 12); out[1] = 0x80 | ((cp >> 6) & 63); out[2] = 0x80 | (cp & 63); return 3; }
    out[0] = 0xf0 | (cp >> 18); out[1] = 0x80 | ((cp >> 12) & 63); out[2] = 0x80 | ((cp >> 6) & 63); out[3] = 0x80 | (cp & 63); return 4;
}
#if KIIKO_BOARD_METALIO47
static uint16_t utf16_unit(const unsigned char *s, bool be) {
    return be?((uint16_t)s[0]<<8)|s[1]:((uint16_t)s[1]<<8)|s[0];
}
// 不吞掉坏代理项后面的正常字符；奇数末字节也转为替换字符。
// Preserve ordinary characters following bad surrogates; replace an odd trailing byte too.
static size_t utf16_one(const unsigned char *s, size_t n, bool be, uint32_t *cp) {
    *cp=0xfffd;
    if(n<2)return n;
    uint32_t first=utf16_unit(s,be);
    if(first<0xd800||first>0xdfff){*cp=first;return 2;}
    if(first<=0xdbff&&n>=4){
        uint32_t second=utf16_unit(s+2,be);
        if(second>=0xdc00&&second<=0xdfff){*cp=0x10000+((first-0xd800)<<10)+(second-0xdc00);return 4;}
    }
    return 2;
}
static uint8_t utf16_probe(const unsigned char *s,size_t n) {
    unsigned le=0,be=0;
    for(size_t i=0;i+1<n;i+=2){
        unsigned a=s[i],b=s[i+1];
        le+=!b&&(a==9||a==10||a==13||(a>=32&&a<=126));
        be+=!a&&(b==9||b==10||b==13||(b>=32&&b<=126));
    }
    // 无BOM仅在同侧反复出现ASCII零字节结构时识别；模糊文件保留原UTF8/GBK规则。
    // Without a BOM require repeated same-side ASCII/NUL structure; ambiguous input keeps UTF8/GBK detection.
    uint8_t result=le>=4&&le>be*4?1:be>=4&&be>le*4?2:0;
    if(!result)return 0;
    unsigned errors=0,scalars=0;
    for(size_t i=0;i+1<n;){uint32_t cp;size_t k=utf16_one(s+i,n-i,result==2,&cp);
        errors+=cp==0xfffd&&utf16_unit(s+i,result==2)!=0xfffd;++scalars;i+=k;}
    return errors*100>scalars?0:result;
}
#endif
static bool next(reader_t *r, uint32_t *cp) {
    if (r->used - r->pos < 4) {
        size_t tail = r->used - r->pos;
        memmove(r->bytes, r->bytes + r->pos, tail);
        r->used = tail + fread(r->bytes + tail, 1, READ_BLOCK, r->file); r->pos = 0;
    }
    size_t n = r->used - r->pos;
    if (!n) return false;
    const unsigned char *s = r->bytes + r->pos; size_t k = 1;
#if KIIKO_BOARD_METALIO47
    if(r->utf16)k=utf16_one(s,n,r->utf16==2,cp);
    else
#endif
    if (!r->gbk) k = utf8_one(s, n, cp);
    else if (s[0] < 0x80) *cp = s[0];
    else {
        *cp = 0xfffd;
        if (s[0] >= 0x81 && s[0] <= 0xfe && n > 1 && s[1] >= 0x40 && s[1] <= 0xfe && s[1] != 0x7f) { *cp = gbk_codepoint(s[0], s[1]); k = 2; }
    }
    r->pos += k; r->offset += k; return true;
}
static esp_err_t reset(reader_t *r, book_txt_t *b, uint32_t offset) {
    memset(r, 0, sizeof(*r)); r->file = b->file; r->gbk = b->gbk; r->offset = offset;
#if KIIKO_BOARD_METALIO47
    r->utf16=b->utf16;
#endif
    clearerr(b->file);
    return fseek(b->file, (long)offset, SEEK_SET) ? ESP_FAIL : ESP_OK;
}
static bool heading(const char *s) {
    while (*s == ' ' || *s == '\t') ++s;
    static const char *const vn_prefixes[] = {
        "Chapter", "CHAPTER", "Chương", "CHƯƠNG", "Chuong", "CHUONG",
        "Hồi", "HỒI", "Phần", "PHẦN", "Quyển", "QUYỂN", "Tập", "TẬP"
    };
    for (size_t i = 0; i < sizeof(vn_prefixes) / sizeof(vn_prefixes[0]); ++i) {
        size_t plen = strlen(vn_prefixes[i]);
        if (!strncmp(s, vn_prefixes[i], plen)) {
            const char *p = s + plen;
            while (*p == ' ' || *p == '\t') ++p;
            return isdigit((unsigned char)*p) != 0 || *p == 'I' || *p == 'V' || *p == 'X';
        }
    }
    if (strncmp(s, "第", 3)) return false;
    s += 3; bool number = false;
    while (*s) {
        uint32_t cp; size_t n = utf8_one((const unsigned char *)s, strlen(s), &cp);
        char token[5] = {0}; memcpy(token, s, n);
        if ((cp >= '0' && cp <= '9') || (n == 3 && strstr("零一二三四五六七八九十百千万两", token))) { number = true; s += n; }
        else return number && n == 3 && strstr("章节卷回集部篇", token);
    }
    return false;
}
static bool add(book_txt_t *b, uint32_t offset, const char *title) {
    if (b->count == BOOK_CHAPTER_MAX) return false;
    book_entry_t *e = &b->entries[b->count++]; e->offset = offset;
    if (title) memcpy(e->title, title, strlen(title) + 1);
    else snprintf(e->title, sizeof(e->title), "Phần %u", (unsigned)b->count);
    return true;
}
static esp_err_t index_book(reader_t *r, book_txt_t *b) {
    esp_err_t err = reset(r, b, b->bom); if (err) return err;
    char line[41] = {0}; size_t len = 0; bool overflow = false, too_many = false;
    uint32_t start = b->bom, cp;
    while (next(r, &cp)) {
        if (cp == '\n') {
            if (!overflow && heading(line) && !add(b, start, line)) too_many = true;
            start = r->offset; len = 0; line[0] = 0; overflow = false;
        } else if (cp != '\r') {
            char bytes[4]; size_t n = encode(cp, bytes);
            if (len + n <= 40 && !overflow) { memcpy(line + len, bytes, n); len += n; line[len] = 0; }
            else overflow = true;
        }
    }
    if (ferror(b->file)) return ESP_FAIL;
    if (!overflow && heading(line) && !add(b, start, line)) too_many = true;
    bool fallback = b->count < 2 || too_many;
    if (b->count) b->entries[0].offset = 0;
    for (size_t i = 0; i < b->count; ++i) {
        uint32_t end = i + 1 < b->count ? b->entries[i + 1].offset : b->total;
        if (end - b->entries[i].offset > BOOK_CHAPTER_BYTES_MAX) fallback = true;
    }
    if (!fallback) return ESP_OK;
    b->count = 0; add(b, 0, NULL); err = reset(r, b, b->bom); if (err) return err;
    uint32_t part = 0;
    while (next(r, &cp)) {
        uint32_t size = r->offset - part;
        // 目标后首个换行切段；无换行时在硬上限前按字符切。/ Split at the first newline after the target, or before the hard cap at a character boundary.
        if (r->offset < b->total && ((size >= PART_TARGET && cp == '\n') || size >= BOOK_CHAPTER_BYTES_MAX - 4)) {
            if (!add(b, r->offset, NULL)) return ESP_ERR_INVALID_SIZE;
            part = r->offset;
        }
    }
    return ferror(b->file) ? ESP_FAIL : ESP_OK;
}

static bool txt_cache_load(const char* source, book_txt_t* b) {
    char path[112]; book_index_cache_header_t key;
    if (!book_index_cache_prepare(source, "txt", TXT_CACHE_VERSION, path, sizeof(path), &key)) return false;
    FILE* cache = book_index_cache_open_read(path, &key);
    if (!cache) return false;
    txt_cache_payload_t payload;
    bool ok = fread(&payload, 1, sizeof(payload), cache) == sizeof(payload) &&
        payload.total == b->total && payload.bom <= 3 && payload.bom <= payload.total &&
        payload.count > 0 && payload.count <= BOOK_CHAPTER_MAX && payload.gbk <= 1 &&
        fread(b->entries, sizeof(b->entries[0]), payload.count, cache) == payload.count &&
        fgetc(cache) == EOF;
#if KIIKO_BOARD_METALIO47
    ok=ok&&payload.reserved[0]<=2&&(!payload.reserved[0]||(!payload.gbk&&(payload.bom==0||payload.bom==2)));
#endif
    fclose(cache);
    uint32_t previous = 0;
    for (size_t i = 0; ok && i < payload.count; ++i) {
        const book_entry_t* entry = &b->entries[i];
        ok = entry->offset >= previous && entry->offset < payload.total &&
            memchr(entry->title, 0, sizeof(entry->title)) != NULL;
#if KIIKO_BOARD_METALIO47
        if(payload.reserved[0]&&entry->offset%2)ok=false;
#endif
        previous = entry->offset;
    }
    if (!ok) return false;
    b->bom = payload.bom; b->gbk = payload.gbk != 0; b->count = payload.count;
#if KIIKO_BOARD_METALIO47
    b->utf16=payload.reserved[0];
#endif
    return true;
}

static void txt_cache_save(const char* source, const book_txt_t* b) {
    char path[112], temp[120]; book_index_cache_header_t key;
    if (!book_index_cache_prepare(source, "txt", TXT_CACHE_VERSION, path, sizeof(path), &key)) return;
    FILE* cache = book_index_cache_open_write(path, &key, temp, sizeof(temp));
    if (!cache) return;
    txt_cache_payload_t payload = {
        .total = b->total, .bom = b->bom, .count = (uint32_t)b->count, .gbk = b->gbk,
    };
#if KIIKO_BOARD_METALIO47
    payload.reserved[0]=b->utf16;
#endif
    bool ok = fwrite(&payload, 1, sizeof(payload), cache) == sizeof(payload) &&
        fwrite(b->entries, sizeof(b->entries[0]), b->count, cache) == b->count;
    (void)book_index_cache_finish_write(cache, temp, path, ok);
}

esp_err_t book_txt_open(book_txt_t *b, const char *path) {
#if KIIKO_BOARD_METALIO47
    b->utf16=0;b->gbk=false;b->bom=0;
#endif
    b->file = fopen(path, "rb"); if (!b->file) return ESP_ERR_NOT_FOUND;
    if (fseek(b->file, 0, SEEK_END)) return ESP_FAIL;
    long size = ftell(b->file);
    if (size < 0 || (unsigned long)size > UINT32_MAX || size > LONG_MAX - 4) return ESP_ERR_INVALID_SIZE;
    b->total = (uint32_t)size;
    b->entries = heap_caps_malloc(sizeof(book_entry_t) * BOOK_CHAPTER_MAX, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!b->entries) return ESP_ERR_NO_MEM;
    if (txt_cache_load(path, b)) return ESP_OK;
    reader_t *r = heap_caps_malloc(sizeof(*r), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!r) return ESP_ERR_NO_MEM;
    esp_err_t err = reset(r, b, 0);
    if (!err) {
        size_t n = fread(r->bytes, 1, READ_BLOCK + 3, b->file);
        if (ferror(b->file)) err = ESP_FAIL;
#if KIIKO_BOARD_METALIO47
        else if(n>=2&&r->bytes[0]==0xff&&r->bytes[1]==0xfe){b->utf16=1;b->bom=2;}
        else if(n>=2&&r->bytes[0]==0xfe&&r->bytes[1]==0xff){b->utf16=2;b->bom=2;}
#endif
        else if (n >= 3 && !memcmp(r->bytes, "\xef\xbb\xbf", 3)) b->bom = 3;
#if KIIKO_BOARD_METALIO47
        else if((b->utf16=utf16_probe(r->bytes,n))!=0){}
#endif
        else {
            size_t valid_utf8_count = 0;
            size_t invalid_bytes = 0;
            for (size_t i = 0; i < n && i < READ_BLOCK;) {
                uint32_t cp; size_t k = utf8_one(r->bytes + i, n - i, &cp);
                if (k == 1 && r->bytes[i] >= 0x80) {
                    invalid_bytes++;
                    i++;
                } else {
                    if (cp >= 0x80) valid_utf8_count++;
                    i += k;
                }
            }
            if (invalid_bytes > 4 && invalid_bytes > valid_utf8_count) {
                b->gbk = true;
            }
        }
    }
    if (!err) err = index_book(r, b);
    if (!err) txt_cache_save(path, b);
    free(r); return err;
}
esp_err_t book_txt_load(book_txt_t *b, size_t i, char **utf8, size_t *len) {
    uint32_t start = b->entries[i].offset, end = i + 1 < b->count ? b->entries[i + 1].offset : b->total;
    if (!start) start = b->bom;
    size_t cap = (size_t)(end - start) * 3 + 1;
    char *out = heap_caps_malloc(cap, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    reader_t *r = heap_caps_malloc(sizeof(*r), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!out || !r) { free(out); free(r); return ESP_ERR_NO_MEM; }
    esp_err_t err = reset(r, b, start); size_t used = 0; uint32_t cp;
    if (!err) while (r->offset < end && next(r, &cp)) {
        char bytes[4]; size_t n = encode(cp, bytes);
        // 文件被外部改写时拒绝越过已索引边界。/ Reject crossing indexed bounds if the file changed externally.
        if (r->offset > end || n >= cap - used) { err = ESP_ERR_INVALID_SIZE; break; }
        memcpy(out + used, bytes, n); used += n;
    }
    if (ferror(b->file) || r->offset != end) err = ESP_FAIL;
    free(r);
    if (err) { free(out); return err; }
    used = vn_normalize_utf8_inplace(out, used);
    out[used] = 0; *utf8 = out; *len = used; return ESP_OK;
}
