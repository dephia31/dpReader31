/*
 * SPDX-FileCopyrightText: 2026 dpReader
 * SPDX-License-Identifier: Apache-2.0
 *
 * Module duyệt và tải sách trực tiếp qua giao thức OPDS (Calibre-Web / OPDS 1.2 Atom XML).
 * Mặc định kết nối tới https://sach.vreader.cloud:8443/opds (cho phép tùy chỉnh).
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OPDS_MAX_ENTRIES 24

typedef struct {
    char title[128];        ///< Tiêu đề mục hoặc tên sách (UTF-8)
    char author[64];        ///< Tác giả (nếu có)
    char summary[160];      ///< Tóm tắt ngắn
    char href[256];         ///< Đường dẫn duyệt tiếp hoặc tải sách (đã chuẩn hóa URL đầy đủ)
    char ext[8];            ///< Định dạng sách: "epub", "txt" hoặc "" nếu là thư mục
    bool is_book;           ///< true: mục sách tải được; false: danh mục con (catalog)
} opds_entry_t;

typedef struct {
    char feed_title[96];                    ///< Tiêu đề trang OPDS hiện tại
    char next_url[256];                     ///< Đường dẫn trang kế tiếp (rel="next") nếu có
    size_t count;                           ///< Số lượng mục trong entries
    opds_entry_t entries[OPDS_MAX_ENTRIES]; ///< Danh sách các mục
} opds_feed_t;

typedef void (*opds_progress_cb_t)(size_t received_bytes, size_t total_bytes, void *user_ctx);

/// Tải và phân tích danh mục OPDS tại `url` (nếu `url == NULL` hoặc rỗng sẽ dùng URL gốc trong Cài đặt).
esp_err_t opds_fetch_feed(const char *url, opds_feed_t *out_feed, char *message, size_t message_cap);

/// Tải trực tiếp một cuốn sách từ `entry` về thư mục sách (`/sdcard/books` hoặc `/flash/books`).
esp_err_t opds_download_book(const opds_entry_t *entry, char *saved_path, size_t saved_path_cap,
                             opds_progress_cb_t progress_cb, void *user_ctx,
                             char *message, size_t message_cap);

#ifdef __cplusplus
}
#endif
