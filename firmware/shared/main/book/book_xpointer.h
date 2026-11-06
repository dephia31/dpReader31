/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#define BOOK_XPOINTER_CAP 768
/* Map a CRE-style EPUB body/text locator to original XHTML bytes and back.
 * Bounded chapter-local parsing; unsupported/malformed paths fail closed. */
esp_err_t book_xpointer_export(
    const char* html, size_t len, uint16_t chapter, size_t source, char* out, size_t cap);
esp_err_t book_xpointer_import(
    const char* html, size_t len, uint16_t chapter, const char* pointer, size_t* source);
bool book_xpointer_chapter(const char* pointer, uint16_t* chapter);

/* Unknown/custom whitespace CSS: accept only text nodes whose coordinates are
 * identical under normal/pre whitespace, rather than guessing a styled offset. */
esp_err_t book_xpointer_export_checked(
    const char* html, size_t len, uint16_t chapter, size_t source, char* out, size_t cap,
    bool canonical_only);
esp_err_t book_xpointer_import_checked(
    const char* html, size_t len, uint16_t chapter, const char* pointer, size_t* source,
    bool canonical_only);

/* Compatible element locator when character normalization is ambiguous. */
esp_err_t book_xpointer_export_element(
    const char* html, size_t len, uint16_t chapter, size_t source, char* out, size_t cap);
