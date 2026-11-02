/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include <stdbool.h>
#include <stdio.h>
#include "esp_err.h"

// 阅读资料按记录流附在设置备份后，避免把大量书籍进度装入内存。
// Append reading data as a stream so a large library does not require a large allocation.
esp_err_t book_history_backup_write(FILE *file);
/// Only an empty library is eligible for automatic import; never replace surviving records.
esp_err_t book_history_backup_empty(bool *empty);
bool book_history_backup_validate(FILE *file);
esp_err_t book_history_backup_restore(FILE *file);
