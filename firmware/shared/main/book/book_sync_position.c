/* SPDX-License-Identifier: Apache-2.0 */
#include "book_sync_position.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "book_epub.h"
#include "html_text.h"
static bool epub_path(const char* path) {
    const char* dot = path ? strrchr(path, '.') : NULL;
    return dot && !strcasecmp(dot, ".epub");
}
static uint32_t chapter_end(book_epub_t* b, size_t i) {
    return i + 1 < book_epub_chapter_count(b) ? book_epub_chapter_byte_offset(b, i + 1)
                                              : book_epub_total_bytes(b);
}
esp_err_t book_sync_export(
    const char* path, uint16_t chapter, uint32_t offset, char* pointer, size_t cap, double* ratio,
    bool* precise) {
    if (!pointer || !cap || !ratio || !epub_path(path)) return ESP_ERR_NOT_SUPPORTED;
    pointer[0] = 0;
    if (precise) *precise = false;
    book_epub_t* book = NULL;
    char* html = NULL;
    size_t len = 0;
    html_text_t text = { 0 };
    esp_err_t err = book_epub_open(path, &book);
    if (err != ESP_OK) return err;
    err = book_epub_chapter_markup(book, chapter, &html, &len);
    if (err == ESP_OK) {
        html_source_map_t map = { .source_target = SIZE_MAX, .text_target = offset };
        err = html_to_blocks_mapped(html, len, &map, &text);
        if (err == ESP_OK) {
            uint32_t start = book_epub_chapter_byte_offset(book, chapter),
                     end = chapter_end(book, chapter), total = book_epub_total_bytes(book);
            size_t source = map.text_found ? map.source_result : 0;
            *ratio = total ? (double)(start + source) / (double)total : 0;
            if (!map.text_found) {
                *ratio = total ? (double)(start + (uint64_t)(end - start) *
                                                      (offset < text.len ? offset : text.len) /
                                                      (text.len ? text.len : 1)) /
                                     (double)total
                               : 0;
                err = ESP_ERR_NOT_FOUND;
            } else {
                err = book_xpointer_export_checked(
                    html, len, chapter, source, pointer, cap,
                    !book_epub_sync_style_safe(book, chapter, html, len));
                if (err == ESP_OK) {
                    if (precise) *precise = true;
                } else if (err == ESP_ERR_NOT_SUPPORTED)
                    err = book_xpointer_export_element(html, len, chapter, source, pointer, cap);
            }
        }
    }
    html_text_free(&text);
    free(html);
    book_epub_close(book);
    return err;
}
esp_err_t book_sync_import(
    const char* path, const char* pointer, double ratio, uint16_t* chapter, uint32_t* offset,
    bool* exact) {
    if (!chapter || !offset || !exact || !epub_path(path) || !isfinite(ratio) || ratio < 0 ||
        ratio > 1)
        return ESP_ERR_INVALID_ARG;
    *exact = false;
    book_epub_t* book = NULL;
    char* html = NULL;
    size_t len = 0;
    html_text_t text = { 0 };
    esp_err_t err = book_epub_open(path, &book);
    if (err != ESP_OK) return err;
    uint16_t selected = 0;
    bool has_chapter = book_xpointer_chapter(pointer, &selected);
    size_t count = book_epub_chapter_count(book);
    if (!has_chapter || selected >= count) {
        uint32_t target = (uint32_t)(ratio * book_epub_total_bytes(book));
        size_t lo = 0, hi = count;
        while (lo + 1 < hi) {
            size_t mid = lo + (hi - lo) / 2;
            if (book_epub_chapter_byte_offset(book, mid) <= target)
                lo = mid;
            else
                hi = mid;
        }
        selected = (uint16_t)lo;
    }
    err = book_epub_chapter_markup(book, selected, &html, &len);
    if (err == ESP_OK) {
        size_t source = SIZE_MAX;
        bool resolved = has_chapter && selected < count &&
                        book_xpointer_import_checked(
                            html, len, selected, pointer, &source,
                            !book_epub_sync_style_safe(book, selected, html, len)) == ESP_OK;
        html_source_map_t map = { .source_target = resolved ? source : SIZE_MAX,
                                  .text_target = SIZE_MAX };
        err = html_to_blocks_mapped(html, len, &map, &text);
        if (err == ESP_OK) {
            *chapter = selected;
            if (resolved && map.source_found) {
                *offset = (uint32_t)map.text_result;
                *exact = true;
            } else {
                uint32_t start = book_epub_chapter_byte_offset(book, selected),
                         end = chapter_end(book, selected);
                double target = ratio * book_epub_total_bytes(book),
                       part = end > start ? (target - start) / (end - start) : 0;
                if (part < 0) part = 0;
                if (part > 1) part = 1;
                size_t at = (size_t)(part * text.len);
                if (at >= text.len && text.len) at = text.len - 1;
                while (at && at < text.len && (((unsigned char)text.utf8[at] & 0xc0) == 0x80)) --at;
                *offset = (uint32_t)at;
            }
        }
    }
    html_text_free(&text);
    free(html);
    book_epub_close(book);
    return err;
}
