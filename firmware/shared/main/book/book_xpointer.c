/* SPDX-License-Identifier: Apache-2.0
 * Chapter-local, bounded XML locator walker. Does not borrow engine font state. */
#include "book_xpointer.h"
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "book_xpointer_entities.h"
#include "esp_heap_caps.h"
#define DEPTH 48
#define NAMES 24
#define TAG_CAP 32
#define CAPS (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
typedef struct {
    char name[TAG_CAP];
    uint32_t count;
} counter_t;
typedef struct {
    char name[TAG_CAP];
    uint32_t ordinal, texts;
    size_t source;
    counter_t children[NAMES];
    unsigned names;
    bool blank_text, comment_text;
} frame_t;
typedef struct {
    char name[TAG_CAP];
    uint32_t ordinal;
} step_t;
typedef struct {
    frame_t frames[DEPTH];
    step_t steps[DEPTH];
    unsigned depth, steps_n;
    uint32_t text_index, character;
    bool terminal_text, inside_body;
    uint16_t chapter;
} walker_t;
static bool space(unsigned char c) {
    return c == ' ' || c == '\n' || c == '\r' || c == '\t';
}
static bool name_char(unsigned char c) {
    return isalnum(c) || c == '_' || c == ':' || c == '-' || c == '.';
}
static bool number(const char** p, uint32_t* v) {
    const char* s = *p;
    uint32_t n = 0;
    if (*s < '0' || *s > '9') return false;
    do {
        unsigned d = *s++ - '0';
        if (n > (UINT32_MAX - d) / 10) return false;
        n = n * 10 + d;
    } while (*s >= '0' && *s <= '9');
    *p = s;
    *v = n;
    return true;
}
bool book_xpointer_chapter(const char* p, uint16_t* chapter) {
    const char prefix[] = "/body/DocFragment[";
    if (!p || strncmp(p, prefix, sizeof(prefix) - 1)) return false;
    p += sizeof(prefix) - 1;
    uint32_t n;
    if (!number(&p, &n) || *p != ']' || !n || n > 8192) return false;
    ++p;
    if (*p && strncmp(p, "/body", 5)) return false;
    if (chapter) *chapter = (uint16_t)(n - 1);
    return true;
}
static bool parse(walker_t* w, const char* p) {
    uint16_t chapter;
    if (!book_xpointer_chapter(p, &chapter) || chapter != w->chapter) return false;
    p = strchr(p, ']') + 1;
    if (!*p) return true;
    if (strncmp(p, "/body", 5)) return false;
    p += 5;
    while (*p) {
        if (*p++ != '/') return false;
        if (!strncmp(p, "text()", 6)) {
            p += 6;
            w->text_index = 1;
            if (*p == '[') {
                ++p;
                if (!number(&p, &w->text_index) || !w->text_index || *p++ != ']') return false;
            }
            if (*p++ != '.' || !number(&p, &w->character) || *p) return false;
            w->terminal_text = true;
            return true;
        }
        if (w->steps_n == DEPTH - 1) return false;
        step_t* s = &w->steps[w->steps_n++];
        size_t n = 0;
        while (name_char((unsigned char)*p)) {
            if (n + 1 >= sizeof(s->name)) return false;
            s->name[n++] = (char)tolower((unsigned char)*p++);
        }
        s->name[n] = 0;
        if (!n) return false;
        s->ordinal = 1;
        if (*p == '[') {
            ++p;
            if (!number(&p, &s->ordinal) || !s->ordinal || *p++ != ']') return false;
        }
    }
    return true;
}
static bool matches(const walker_t* w) {
    if (w->depth != w->steps_n + 1) return false;
    for (unsigned i = 0; i < w->steps_n; i++)
        if (strcmp(w->frames[i + 1].name, w->steps[i].name) ||
            w->frames[i + 1].ordinal != w->steps[i].ordinal)
            return false;
    return true;
}
static bool tag_name(const char* h, size_t len, size_t* at, char out[TAG_CAP]) {
    size_t start = *at;
    while (*at < len && name_char((unsigned char)h[*at])) ++*at;
    size_t local = start;
    for (size_t i = start; i < *at; i++)
        if (h[i] == ':') local = i + 1;
    size_t n = *at - local;
    if (!n || n >= TAG_CAP) return false;
    for (size_t i = 0; i < n; i++) out[i] = (char)tolower((unsigned char)h[local + i]);
    out[n] = 0;
    return true;
}
static bool is_void(const char* n) {
    return !strcmp(n, "br") || !strcmp(n, "img") || !strcmp(n, "hr") || !strcmp(n, "input") ||
           !strcmp(n, "meta") || !strcmp(n, "link") || !strcmp(n, "area") || !strcmp(n, "col") ||
           !strcmp(n, "embed") || !strcmp(n, "source") || !strcmp(n, "wbr");
}
static uint32_t next_child(frame_t* f, const char* n) {
    for (unsigned i = 0; i < f->names; i++)
        if (!strcmp(f->children[i].name, n)) return ++f->children[i].count;
    if (f->names == NAMES) return 0;
    counter_t* c = &f->children[f->names++];
    strcpy(c->name, n);
    return c->count = 1;
}
static bool decode(const char* h, size_t len, size_t* used, uint32_t* cp) {
    if (!len) return false;
    unsigned char c = h[0];
    *used = 1;
    if (c == '&') {
        size_t stop = 1;
        while (stop < len && stop < 24 && h[stop] != ';') stop++;
        if (stop == len || stop == 24) return false;
        if (h[1] == '#') {
            const char* p = h + 2;
            unsigned base = 10;
            if (*p == 'x' || *p == 'X') {
                base = 16;
                p++;
            }
            uint32_t n = 0;
            bool any = false;
            while (p < h + stop) {
                unsigned d;
                if (*p >= '0' && *p <= '9')
                    d = *p - '0';
                else if (
                    base == 16 && tolower((unsigned char)*p) >= 'a' &&
                    tolower((unsigned char)*p) <= 'f')
                    d = tolower((unsigned char)*p) - 'a' + 10;
                else
                    return false;
                if (n > (0x10ffff - d) / base) return false;
                n = n * base + d;
                p++;
                any = true;
            }
            if (!any || !n || n > 0x10ffff || (n >= 0xd800 && n <= 0xdfff)) return false;
            *cp = n;
        } else {
            char name[33];
            if (stop - 1 >= sizeof(name)) return false;
            memcpy(name, h + 1, stop - 1);
            name[stop - 1] = 0;
            size_t lo = 0, hi = sizeof(xpointer_entities) / sizeof(xpointer_entities[0]);
            bool found = false;
            while (lo < hi) {
                size_t mid = lo + (hi - lo) / 2;
                int cmp = strcmp(name, xpointer_entities[mid].name);
                if (!cmp) {
                    *cp = xpointer_entities[mid].cp;
                    found = true;
                    break;
                }
                if (cmp < 0)
                    hi = mid;
                else
                    lo = mid + 1;
            }
            if (!found) return false;
        }
        *used = stop + 1;
        return true;
    }
    if (c == '\r') {
        *cp = '\n';
        if (len > 1 && h[1] == '\n') *used = 2;
        return true;
    }
    if (c < 128) {
        *cp = c;
        return c != 0;
    }
    unsigned n = (c >= 0xc2 && c <= 0xdf)   ? 2
                 : (c >= 0xe0 && c <= 0xef) ? 3
                 : (c >= 0xf0 && c <= 0xf4) ? 4
                                            : 0;
    if (!n || n > len) return false;
    uint32_t v = c & ((1u << (7 - n)) - 1);
    for (unsigned i = 1; i < n; i++) {
        unsigned char b = h[i];
        if ((b & 0xc0) != 0x80) return false;
        v = (v << 6) | (b & 63);
    }
    if ((n == 2 && v < 128) || (n == 3 && v < 2048) || (n == 4 && v < 65536) || v > 0x10ffff ||
        (v >= 0xd800 && v <= 0xdfff))
        return false;
    *used = n;
    *cp = v;
    return true;
}
static bool format(const walker_t* w, uint32_t text, uint32_t ch, char* out, size_t cap) {
    int n = snprintf(out, cap, "/body/DocFragment[%u]/body", (unsigned)w->chapter + 1);
    if (n < 0 || (size_t)n >= cap) return false;
    size_t used = (size_t)n;
    for (unsigned i = 1; i < w->depth; i++) {
        n = snprintf(
            out + used, cap - used, "/%s[%u]", w->frames[i].name, (unsigned)w->frames[i].ordinal);
        if (n < 0 || (size_t)n >= cap - used) return false;
        used += (size_t)n;
    }
    if (text) {
        n = snprintf(out + used, cap - used, "/text()[%u].%u", (unsigned)text, (unsigned)ch);
        if (n < 0 || (size_t)n >= cap - used) return false;
    }
    return true;
}
static esp_err_t walk(
    const char* h, size_t len, uint16_t chapter, size_t wanted, const char* pointer, size_t* source,
    char* out, size_t cap, bool canonical_only, bool element_only) {
    if (!h || (!pointer && (!out || !cap))) return ESP_ERR_INVALID_ARG;
    walker_t* w = heap_caps_calloc(1, sizeof(*w), CAPS);
    if (!w) return ESP_ERR_NO_MEM;
    w->chapter = chapter;
    esp_err_t err = ESP_ERR_NOT_FOUND;
    bool importing = pointer != NULL, found = false;
    size_t result = 0;
    if (importing && !parse(w, pointer)) {
        err = ESP_ERR_NOT_SUPPORTED;
        goto done;
    }
    for (size_t pos = 0; pos < len;) {
        if (h[pos] == '<') {
            if (pos + 4 <= len && !memcmp(h + pos, "<!--", 4)) {
                if (w->inside_body && w->depth) w->frames[w->depth - 1].comment_text = true;
                size_t e = pos + 4;
                while (e + 3 <= len && memcmp(h + e, "-->", 3)) e++;
                if (e + 3 > len) {
                    err = ESP_ERR_INVALID_RESPONSE;
                    goto done;
                }
                pos = e + 3;
                continue;
            }
            // Unsupported CDATA is rejected rather than silently changing text-node coordinates.
            if (pos + 9 <= len && !memcmp(h + pos, "<![CDATA[", 9)) {
                err = ESP_ERR_NOT_SUPPORTED;
                goto done;
            }
            if (pos + 2 < len && (h[pos + 1] == '!' || h[pos + 1] == '?')) {
                char quote = 0;
                unsigned brackets = 0;
                size_t e = pos + 2;
                for (; e < len; e++) {
                    char c = h[e];
                    if (quote) {
                        if (c == quote) quote = 0;
                    } else if (c == '\'' || c == '"')
                        quote = c;
                    else if (c == '[')
                        brackets++;
                    else if (c == ']' && brackets)
                        brackets--;
                    else if (c == '>' && !brackets)
                        break;
                }
                if (e == len) {
                    err = ESP_ERR_INVALID_RESPONSE;
                    goto done;
                }
                pos = e + 1;
                continue;
            }
            size_t at = pos + 1;
            bool close = at < len && h[at] == '/';
            if (close) at++;
            char name[TAG_CAP];
            if (!tag_name(h, len, &at, name)) {
                err = ESP_ERR_INVALID_RESPONSE;
                goto done;
            }
            char quote = 0;
            size_t end = at;
            for (; end < len; end++) {
                char c = h[end];
                if (quote) {
                    if (c == quote) quote = 0;
                } else if (c == '\'' || c == '"')
                    quote = c;
                else if (c == '>')
                    break;
            }
            if (end == len) {
                err = ESP_ERR_INVALID_RESPONSE;
                goto done;
            }
            size_t last = end;
            while (last > at && space((unsigned char)h[last - 1])) last--;
            bool empty = (last > at && h[last - 1] == '/') || is_void(name);
            if (!w->inside_body) {
                if (!close && !strcmp(name, "body")) {
                    w->inside_body = true;
                    w->depth = 1;
                    strcpy(w->frames[0].name, "body");
                    w->frames[0].ordinal = 1;
                    w->frames[0].source = pos;
                    if (importing && !w->steps_n && !w->terminal_text) {
                        found = true;
                        result = pos;
                    }
                    if (!importing && wanted == pos) {
                        found = format(w, 0, 0, out, cap);
                        result = pos;
                    }
                }
                pos = end + 1;
                continue;
            }
            if (close) {
                if (!w->depth || strcmp(w->frames[w->depth - 1].name, name)) {
                    err = ESP_ERR_INVALID_RESPONSE;
                    goto done;
                }
                w->depth--;
                if (!w->depth) {
                    w->inside_body = false;
                    pos = end + 1;
                    break;
                }
            } else {
                if (w->depth == DEPTH) {
                    err = ESP_ERR_INVALID_SIZE;
                    goto done;
                }
                frame_t* parent = &w->frames[w->depth - 1];
                uint32_t ordinal = next_child(parent, name);
                if (!ordinal) {
                    err = ESP_ERR_INVALID_SIZE;
                    goto done;
                }
                frame_t* f = &w->frames[w->depth++];
                memset(f, 0, sizeof(*f));
                strcpy(f->name, name);
                f->ordinal = ordinal;
                f->source = pos;
                if (importing && !w->terminal_text && matches(w)) {
                    found = true;
                    result = pos;
                }
                if (!importing && wanted == pos) {
                    found = format(w, 0, 0, out, cap);
                    result = pos;
                }
                if (empty) w->depth--;
            }
            pos = end + 1;
            continue;
        }
        size_t end = pos;
        while (end < len && h[end] != '<') end++;
        if (w->inside_body && w->depth && (element_only || (importing && !w->terminal_text))) {
            if (element_only && wanted >= pos && wanted < end) {
                found = format(w, 0, 0, out, cap);
                result = pos;
            }
            pos = end;
            continue;
        }
        if (w->inside_body && w->depth) {
            frame_t* f = &w->frames[w->depth - 1];
            bool block =
                !strcmp(f->name, "body") || !strcmp(f->name, "p") || !strcmp(f->name, "div") ||
                !strcmp(f->name, "li") || !strcmp(f->name, "blockquote") ||
                !strcmp(f->name, "td") || !strcmp(f->name, "th") || !strcmp(f->name, "section") ||
                (f->name[0] == 'h' && f->name[1] >= '1' && f->name[1] <= '6' && !f->name[2]);
            bool visible = false;
            for (size_t j = pos; j < end;) {
                size_t n;
                uint32_t cp;
                if (!decode(h + j, end - j, &n, &cp)) {
                    err = ESP_ERR_NOT_SUPPORTED;
                    goto done;
                }
                if (cp > 127 || !space((unsigned char)cp)) visible = true;
                j += n;
            }
            if (visible) {
                uint32_t node = ++f->texts, units = 0;
                bool target = importing ? (w->terminal_text && matches(w) && node == w->text_index)
                                        : (wanted >= pos && wanted < end);
                if (target && f->comment_text) {
                    err = ESP_ERR_NOT_SUPPORTED;
                    goto done;
                }
                if (target && (canonical_only || !strcmp(f->name, "pre"))) {
                    bool previous_space = false, first = true, ambiguous = f->blank_text;
                    uint32_t last = 0;
                    for (size_t j = pos; j < end;) {
                        size_t n;
                        uint32_t cp;
                        if (!decode(h + j, end - j, &n, &cp)) {
                            err = ESP_ERR_NOT_SUPPORTED;
                            goto done;
                        }
                        bool ws = cp < 128 && space((unsigned char)cp);
                        if (ws && (first || previous_space || cp != ' ')) ambiguous = true;
                        previous_space = ws;
                        first = false;
                        last = cp;
                        j += n;
                    }
                    if (last < 128 && space((unsigned char)last)) ambiguous = true;
                    if (ambiguous) {
                        err = ESP_ERR_NOT_SUPPORTED;
                        goto done;
                    }
                }
                bool pending = false;
                bool trim_start = block && node == 1 && f->names == 0;
                for (size_t j = pos; j < end;) {
                    size_t n;
                    uint32_t cp;
                    if (!decode(h + j, end - j, &n, &cp)) {
                        err = ESP_ERR_NOT_SUPPORTED;
                        goto done;
                    }
                    if (cp < 128 && space((unsigned char)cp)) {
                        if (units || !trim_start) pending = true;
                        j += n;
                        continue;
                    }
                    if (pending) {
                        units++;
                        pending = false;
                    }
                    if (importing && w->terminal_text && matches(w) && node == w->text_index &&
                        units == w->character) {
                        found = true;
                        result = j;
                    }
                    if (!importing && wanted == j) {
                        found = format(w, node, units, out, cap);
                        result = j;
                    }
                    // Current CRE stores text in lString32: one Unicode scalar, not a UTF-8 byte or UTF-16 surrogate.
                    units++;
                    j += n;
                }
                if (importing && w->terminal_text && matches(w) && node == w->text_index &&
                    units == w->character) {
                    found = true;
                    result = end;
                }
            } else
                f->blank_text = true;
        }
        pos = end;
    }
    if (w->inside_body || w->depth) {
        err = ESP_ERR_INVALID_RESPONSE;
        goto done;
    }
    if (found) {
        if (source) *source = result;
        err = ESP_OK;
    }
done:
    free(w);
    if (err != ESP_OK && out && cap) out[0] = 0;
    return err;
}
esp_err_t book_xpointer_export(
    const char* h, size_t n, uint16_t ch, size_t source, char* out, size_t cap) {
    return walk(h, n, ch, source, NULL, NULL, out, cap, false, false);
}
esp_err_t book_xpointer_import(
    const char* h, size_t n, uint16_t ch, const char* p, size_t* source) {
    if (!source) return ESP_ERR_INVALID_ARG;
    return walk(h, n, ch, 0, p, source, NULL, 0, false, false);
}

esp_err_t book_xpointer_export_checked(
    const char* h, size_t n, uint16_t ch, size_t source, char* out, size_t cap, bool strict) {
    return walk(h, n, ch, source, NULL, NULL, out, cap, strict, false);
}
esp_err_t book_xpointer_import_checked(
    const char* h, size_t n, uint16_t ch, const char* p, size_t* source, bool strict) {
    if (!source) return ESP_ERR_INVALID_ARG;
    return walk(h, n, ch, 0, p, source, NULL, 0, strict, false);
}

esp_err_t book_xpointer_export_element(
    const char* h, size_t n, uint16_t ch, size_t source, char* out, size_t cap) {
    return walk(h, n, ch, source, NULL, NULL, out, cap, false, true);
}
