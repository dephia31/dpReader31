/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <time.h>
#include "esp_err.h"

typedef struct {
    bool enabled;
    uint16_t on_minute, off_minute; /* Minutes after midnight, Vietnam time. */
    uint8_t level; /* Per-channel percentage, 1..100. */
} metalio_light_schedule_t;

metalio_light_schedule_t metalio_light_schedule_get(bool warm);
esp_err_t metalio_light_schedule_save(bool warm, const metalio_light_schedule_t *schedule);
/* Update channel levels (0..240) only at a schedule boundary or after invalidation. */
bool metalio_light_schedule_apply(time_t utc, unsigned *white, unsigned *warm);
void metalio_light_schedule_invalidate(void);
