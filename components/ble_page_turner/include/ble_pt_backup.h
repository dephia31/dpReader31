/* SPDX-License-Identifier: Apache-2.0 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

/* Backup holds two learned actions for each of the four saved accessories. */
typedef struct {
    char key[11];
    uint8_t frames[2][17];
} ble_pt_backup_entry_t;
typedef struct {
    ble_pt_backup_entry_t entries[4];
} ble_pt_bindings_backup_t;

esp_err_t ble_pt_bindings_backup_capture(ble_pt_bindings_backup_t *backup);
bool ble_pt_bindings_backup_valid(const ble_pt_bindings_backup_t *backup);
esp_err_t ble_pt_bindings_backup_restore(const ble_pt_bindings_backup_t *backup);
