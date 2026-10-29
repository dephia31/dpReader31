/*
 * SPDX-FileCopyrightText: 2026 dpReader
 * SPDX-License-Identifier: Apache-2.0
 *
 * Module đồng bộ tiến độ đọc KOSync (KOReader Sync) cho dpReader.
 * Hỗ trợ xác thực MD5 user/key, tính KOReader partial MD5 của file sách,
 * đẩy (PUT) và kéo (GET) tiến độ đọc qua giao thức KOReader Sync v1.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "book_progress.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char document[36];      ///< Mã băm MD5 (32 ký tự hex) của tài liệu
    char progress[128];     ///< Chuỗi vị trí đọc (XPointer hoặc offset)
    double percentage;      ///< Tỷ lệ hoàn thành 0.0 .. 1.0
    char device[48];        ///< Tên thiết bị cập nhật cuối cùng
    char device_id[40];     ///< ID thiết bị cập nhật cuối cùng
    int64_t timestamp;      ///< Thời gian cập nhật trên server (nếu có)
} kosync_progress_t;

/// Băm chuỗi bất kỳ (ví dụ mật khẩu) thành chuỗi 32 ký tự hex MD5 chữ thường.
void kosync_md5_string(const char *input, char out_hex[33]);

/// Tính mã băm tài liệu theo đúng chuẩn `util.partialMD5` của KOReader.
esp_err_t kosync_compute_doc_md5(const char *file_path, char out_hex[33]);

/// Kiểm tra tài khoản KOSync hiện tại (hoặc đăng ký mới nếu `register_new == true`).
esp_err_t kosync_verify_account(bool register_new, char *message, size_t message_cap);

/// Đẩy tiến độ đọc của sách tại `file_path` lên máy chủ KOSync.
esp_err_t kosync_push_book_progress(const char *file_path, const book_progress_t *local_prog,
                                    double exact_ratio, char *message, size_t message_cap);

/// Kéo tiến độ đọc của sách tại `file_path` từ máy chủ KOSync về.
esp_err_t kosync_pull_book_progress(const char *file_path, kosync_progress_t *out_remote,
                                    uint16_t *out_chapter, uint32_t *out_byte_off,
                                    bool *has_exact_offset, char *message, size_t message_cap);

/// Đồng bộ 2 chiều tiến độ sách (so sánh và tự động kéo nếu remote mới hơn, hoặc đẩy lên nếu local mới hơn).
esp_err_t kosync_sync_book(const char *file_path, uint16_t *cur_chapter, uint32_t *cur_byte_off,
                           uint8_t *cur_pct, bool *pulled_newer, char *message, size_t message_cap);


/// 后台同步持有网络/文件时禁止睡眠与媒体重挂。/ Block sleep and media recovery while a sync owns network/files.
bool kosync_busy(void);
/// 复制进度快照，不在调用线程联网；同类待办合并。/ Copy progress without network IO on the caller; coalesce pending jobs by kind.
void kosync_auto_submit(const char *path,const book_progress_t *progress,double ratio,bool pull);
/// 主循环启动待办，启动失败退避。/ Start pending work from the loop, backing off on allocation failure.
void kosync_auto_poll(void);
/// 仅返回同书同位置的结果，用户已翻页则丢弃。/ Return only a same-book, unchanged-position result; discard after user movement.
bool kosync_auto_take_pull(const char *path,const book_progress_t *current,kosync_progress_t *remote,uint16_t *chapter,uint32_t *offset,bool *exact);

#ifdef __cplusplus
}
#endif
