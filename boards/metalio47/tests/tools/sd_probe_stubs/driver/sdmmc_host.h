/* SPDX-License-Identifier: Apache-2.0
 * SD 探测主机测试总线配置。/ Host SD-probe bus configuration stub.
 */
#pragma once

#include "esp_err.h"
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
esp_err_t test_host_init(void);
esp_err_t test_host_deinit(void);
typedef struct { int max_freq_khz, slot,flags; esp_err_t (*init)(void); esp_err_t (*deinit)(void);
    bool (*check_buffer_alignment)(int,const void*,size_t);
    void* dma_aligned_buffer; size_t unaligned_multi_block_rw_max_chunk_size;
} sdmmc_host_t;
static inline bool sdmmc_host_check_buffer_alignment(int slot,const void* p,size_t n){(void)slot;return (uintptr_t)p%4==0&&n%4==0;}
#define SDMMC_HOST_FLAG_ALLOC_ALIGNED_BUF 64
typedef struct {
    int width, clk, cmd, d0, d1, d2, d3, cd, wp, flags;
} sdmmc_slot_config_t;

#define SDMMC_HOST_DEFAULT() ((sdmmc_host_t){.init=test_host_init,.deinit=test_host_deinit})
#define SDMMC_SLOT_CONFIG_DEFAULT() ((sdmmc_slot_config_t){0})
#define SDMMC_FREQ_HIGHSPEED 40000
#define SDMMC_SLOT_FLAG_INTERNAL_PULLUP 1
#define GPIO_NUM_38 38
#define GPIO_NUM_42 42
#define GPIO_NUM_44 44
#define GPIO_NUM_NC -1

esp_err_t sdmmc_host_init_slot(int slot, const sdmmc_slot_config_t *config);

#define GPIO_NUM_20 20
#define GPIO_NUM_21 21
#define GPIO_NUM_22 22
#define GPIO_NUM_23 23
#define GPIO_NUM_24 24
#define GPIO_NUM_25 25
#define SDMMC_HOST_SLOT_0 0
