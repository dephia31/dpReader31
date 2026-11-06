/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "book_xpointer.h"
#include "esp_err.h"
/* File-local conversion; independent ZIP handles and no shared font engine calls. */
esp_err_t book_sync_export(
    const char* path, uint16_t chapter, uint32_t text_offset, char* pointer, size_t cap,
    double* ratio, bool* precise);
esp_err_t book_sync_import(
    const char* path, const char* pointer, double ratio, uint16_t* chapter, uint32_t* text_offset,
    bool* exact);
