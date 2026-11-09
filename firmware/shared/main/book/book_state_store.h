/* SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#ifndef BOOK_STATE_SD_ROOT
#define BOOK_STATE_SD_ROOT "/sdcard/.dpreader"
#endif
#ifndef BOOK_STATE_FLASH_ROOT
#define BOOK_STATE_FLASH_ROOT "/flash/.dpreader"
#endif
#define BOOK_STATE_KEY_MAX 288
#define BOOK_STATE_DATA_MAX 4096

/// Two checked slots retain the previous record until the replacement is synced.
/// Callers serialize writes with the reader, transfer and backup lifecycle.
esp_err_t book_state_read(const char *root, const char *key, const char *kind,
                          void *data, size_t *size);
esp_err_t book_state_write(const char *root, const char *key, const char *kind,
                           const void *data, size_t size);
const char *book_state_root(const char *book);
bool book_state_ready(const char *root);
typedef bool (*book_state_visit_fn)(const char *key, const void *data, size_t size, void *ctx);
esp_err_t book_state_list(const char *root, const char *kind, book_state_visit_fn visit, void *ctx);
