/* SPDX-License-Identifier: Apache-2.0 */
#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#define METALIO_PREFERENCES_BACKUP_BYTES 24
esp_err_t metalio_preferences_backup_capture(uint8_t out[METALIO_PREFERENCES_BACKUP_BYTES]);
bool metalio_preferences_backup_valid(const uint8_t data[METALIO_PREFERENCES_BACKUP_BYTES]);
esp_err_t metalio_preferences_backup_restore(const uint8_t data[METALIO_PREFERENCES_BACKUP_BYTES]);
