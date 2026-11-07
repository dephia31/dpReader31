#pragma once
#include <stdint.h>
#include "esp_err.h"
typedef struct {
    uint64_t capacity_bytes, free_bytes;
    bool mounted, space_valid;
} read_pico_sd_info_t;
static bool test_sd_space_valid;
static unsigned test_sd_space_refreshes;
static inline esp_err_t read_pico_sd_get_info(read_pico_sd_info_t *info) {
    *info = (read_pico_sd_info_t){
        .capacity_bytes = 64ULL * 1024 * 1024 * 1024,
        .free_bytes = test_sd_space_valid ? 123456789 : 0,
        .mounted = true,
        .space_valid = test_sd_space_valid,
    };
    return ESP_OK;
}
static inline esp_err_t read_pico_sd_start_space_refresh(void) {
    ++test_sd_space_refreshes;
    return ESP_ERR_NOT_FINISHED;
}
