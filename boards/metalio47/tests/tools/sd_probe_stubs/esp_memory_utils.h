/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
static const void* test_external_dma_ptr;
static inline bool esp_ptr_internal(const void* p){return p!=test_external_dma_ptr;}
