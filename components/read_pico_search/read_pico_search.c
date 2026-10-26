/*
 * SPDX-FileCopyrightText: 2026 dpReader
 * SPDX-License-Identifier: Apache-2.0
 * Bộ tìm kiếm tên sách Tiếng Việt (khớp chính xác, khớp không dấu và khớp chữ cái đầu từng từ).
 * Không cấp phát bộ nhớ động (allocation-free), an toàn đa luồng (reentrant).
 */
#include "read_pico_search.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "search_table.h"
#include "search_candidates.h"

static unsigned char lower(unsigned char c) {
    return c >= 'A' && c <= 'Z' ? (unsigned char)(c + ('a' - 'A')) : c;
}
static bool letter(uint32_t c) { return c >= 'a' && c <= 'z'; }
static bool digit(uint32_t c) { return c >= '0' && c <= '9'; }
static bool space(uint32_t c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

static bool bounded_length(const char* s, size_t limit, size_t* length) {
    if (!s) return false;
    size_t n = 0;
    while (n <= limit && s[n]) ++n;
    *length = n;
    return n <= limit;
}

static bool utf8(const char* s, size_t length, size_t* index, uint32_t* out) {
    if (*index >= length) return false;
    unsigned char c = (unsigned char)s[(*index)++];
    if (c < 128) { *out = c; return true; }
    unsigned following;
    uint32_t value, minimum;
    if (c >= 0xc2 && c <= 0xdf) { following = 1; value = c & 31; minimum = 128; }
    else if (c >= 0xe0 && c <= 0xef) { following = 2; value = c & 15; minimum = 2048; }
    else if (c >= 0xf0 && c <= 0xf4) { following = 3; value = c & 7; minimum = 65536; }
    else return false;
    while (following--) {
        if (*index >= length) return false;
        c = (unsigned char)s[(*index)++];
        if ((c & 0xc0) != 0x80) return false;
        value = (value << 6) | (c & 63);
    }
    if (value < minimum || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff)) return false;
    *out = value;
    return true;
}

static bool valid_utf8(const char* s, size_t length) {
    size_t i = 0;
    uint32_t cp;
    while (i < length) if (!utf8(s, length, &i, &cp)) return false;
    return true;
}

static bool equal_ascii(const char* a, const char* b, size_t n) {
    for (size_t i = 0; i < n; ++i)
        if (lower((unsigned char)a[i]) != lower((unsigned char)b[i])) return false;
    return true;
}

static size_t stem_length(const char* name, size_t n) {
    if (n >= 4 && equal_ascii(name + n - 4, ".txt", 4)) return n - 4;
    if (n >= 5 && equal_ascii(name + n - 5, ".epub", 5)) return n - 5;
    return n;
}

/* Chuyển đổi toàn bộ ký tự Unicode Tiếng Việt có dấu về chữ cái ASCII thường a-z.
 * Trả về 0 nếu là dấu tổ hợp (combining mark) để bỏ qua.
 */
static uint32_t vn_fold(uint32_t cp) {
    if (cp < 128) return lower((unsigned char)cp);
    if (cp >= 0x0300 && cp <= 0x036F) return 0; /* Dấu tổ hợp Unicode */

    /* Bảng Latin-1 Supplement & Latin Extended-A/B */
    if ((cp >= 0x00C0 && cp <= 0x00C5) || (cp >= 0x00E0 && cp <= 0x00E5) ||
        cp == 0x0102 || cp == 0x0103) return 'a';
    if (cp == 0x0110 || cp == 0x0111 || cp == 0x00D0 || cp == 0x00F0) return 'd';
    if ((cp >= 0x00C8 && cp <= 0x00CB) || (cp >= 0x00E8 && cp <= 0x00EB)) return 'e';
    if ((cp >= 0x00CC && cp <= 0x00CF) || (cp >= 0x00EC && cp <= 0x00EF) ||
        cp == 0x0128 || cp == 0x0129) return 'i';
    if ((cp >= 0x00D2 && cp <= 0x00D6) || (cp >= 0x00F2 && cp <= 0x00F6) ||
        cp == 0x01A0 || cp == 0x01A1) return 'o';
    if ((cp >= 0x00D9 && cp <= 0x00DC) || (cp >= 0x00F9 && cp <= 0x00FC) ||
        cp == 0x0168 || cp == 0x0169 || cp == 0x01AF || cp == 0x01B0) return 'u';
    if (cp == 0x00DD || cp == 0x00FD || cp == 0x0178 || cp == 0x00FF) return 'y';

    /* Bảng Latin Extended Additional (Tiếng Việt U+1EA0 .. U+1EF9) */
    if (cp >= 0x1EA0 && cp <= 0x1EB7) return 'a';
    if (cp >= 0x1EB8 && cp <= 0x1EC7) return 'e';
    if (cp >= 0x1EC8 && cp <= 0x1ECB) return 'i';
    if (cp >= 0x1ECC && cp <= 0x1EE3) return 'o';
    if (cp >= 0x1EE4 && cp <= 0x1EF1) return 'u';
    if (cp >= 0x1EF2 && cp <= 0x1EF9) return 'y';

    return cp;
}

static bool advance(uint64_t* state, unsigned char c, const uint64_t masks[128], uint64_t finish) {
    uint64_t matched = *state & masks[c];
    *state = (matched << 1) | 1;
    return (matched & finish) != 0;
}

bool read_pico_search_match(const char* filename, const char* query) {
    size_t length, query_length;
    if (!bounded_length(filename, 255, &length) || !bounded_length(query, 64, &query_length) ||
        !valid_utf8(filename, length) || !valid_utf8(query, query_length)) return false;
    length = stem_length(filename, length);
    if (!query_length) return true;

    /* 1. Khớp chuỗi con chính xác hoặc không phân biệt hoa/thường ASCII */
    if (query_length <= length) {
        for (size_t i = 0; i <= length - query_length; ++i)
            if (equal_ascii(filename + i, query, query_length)) return true;
    }

    /* 2. Chuẩn hóa câu truy vấn sang không dấu Tiếng Việt (bỏ khoảng trắng) */
    uint64_t masks[128] = {0};
    size_t i = 0, n = 0;
    while (i < query_length) {
        uint32_t cp;
        if (!utf8(query, query_length, &i, &cp)) return false;
        if (space(cp)) continue;
        cp = vn_fold(cp);
        if (cp == 0) continue;
        if (cp >= 128 || (!letter(cp) && !digit(cp))) return false;
        if (n >= 64) return false;
        masks[cp] |= UINT64_C(1) << n++;
    }
    if (!n) return true;

    /* 3. Duyệt tên file: vừa khớp chuỗi không dấu liên tục (full), vừa khớp chữ cái đầu mỗi từ (initials) */
    uint64_t finish = UINT64_C(1) << (n - 1);
    uint64_t full = 1, initials = 1;
    bool in_word = false;
    i = 0;
    while (i < length) {
        uint32_t cp;
        if (!utf8(filename, length, &i, &cp)) return false;
        cp = vn_fold(cp);
        if (cp == 0) continue;
        if (cp < 128 && (letter(cp) || digit(cp))) {
            if (advance(&full, (unsigned char)cp, masks, finish)) return true;
            if ((!in_word || digit(cp)) && advance(&initials, (unsigned char)cp, masks, finish)) return true;
            in_word = letter(cp);
        } else {
            in_word = false;
        }
    }
    return false;
}

size_t read_pico_search_candidates(const char* syllable, uint32_t* out, size_t cap, size_t skip) {
    if (!syllable || !out || !cap) return 0;
    char normalized[9];
    size_t n = strnlen(syllable, sizeof(normalized));
    if (!n || n >= sizeof(normalized)) return 0;
    for (size_t i = 0; i < n; ++i) {
        unsigned char c = lower((unsigned char)syllable[i]);
        if (!letter(c)) return 0;
        normalized[i] = (char)c;
    }
    normalized[n] = 0;
    size_t total = sizeof(search_syllable_offsets) / sizeof(search_syllable_offsets[0]);
    size_t low = 0, high = total;
    while (low < high) {
        size_t mid = low + (high - low) / 2;
        int order = strcmp(search_syllables + search_syllable_offsets[mid], normalized);
        if (order < 0) low = mid + 1; else high = mid;
    }
    if (low >= total || strcmp(search_syllables + search_syllable_offsets[low], normalized)) return 0;
    size_t begin = ime_character_offsets[low], count = ime_character_offsets[low + 1] - begin;
    if (skip >= count) return 0;
    count -= skip;
    if (count > cap) count = cap;
    for (size_t i = 0; i < count; ++i) out[i] = ime_characters[begin + skip + i];
    return count;
}

const char *read_pico_search_syllable(size_t index) {
    return index < sizeof(search_syllable_offsets) / sizeof(search_syllable_offsets[0])
        ? search_syllables + search_syllable_offsets[index] : NULL;
}
