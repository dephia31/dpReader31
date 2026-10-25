/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
static bool test_dma_alignment_error;
static inline int esp_cache_get_alignment(unsigned caps,size_t* alignment){(void)caps;*alignment=64;return test_dma_alignment_error?ESP_FAIL:ESP_OK;}
