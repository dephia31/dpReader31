/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * Log export and diagnostics module for kiikoread / dpReader.
 * Captures console ESP_LOG output in an in-memory ring buffer
 * and exports complete system diagnostic reports to SD card.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize log capture hook (esp_log_set_vprintf).
 * Safe to call multiple times (idempotent).
 */
void log_export_init(void);

/**
 * Export complete system diagnostic report and logs to /sdcard/dpreader_log.txt.
 * @param out_path Buffer to receive the exported file path (can be NULL).
 * @param out_path_size Capacity of out_path buffer.
 * @param err_msg Buffer to receive error description if failed (can be NULL).
 * @param err_msg_size Capacity of err_msg buffer.
 * @return ESP_OK on success, or error code.
 */
esp_err_t log_export_to_sd(char *out_path, size_t out_path_size, char *err_msg, size_t err_msg_size);

/**
 * Retrieve recent log lines from the ring buffer for display.
 * @param out Buffer to fill with text.
 * @param max_len Size of out buffer.
 * @return Number of bytes written.
 */
int log_export_get_recent(char *out, size_t max_len);

/**
 * Clear the in-memory log buffer.
 */
void log_export_clear(void);

#ifdef __cplusplus
}
#endif

