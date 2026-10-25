/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include <assert.h>
#include <stdlib.h>
#define MALLOC_CAP_INTERNAL 1
#define MALLOC_CAP_DMA 2
static bool test_dma_fail_alloc;
static unsigned test_dma_allocs,test_dma_frees;
static void* test_dma_live;
static inline void* heap_caps_malloc(size_t n,int caps){(void)caps;return malloc(n);}
static inline void* heap_caps_aligned_alloc(size_t a,size_t n,int caps){
    assert(!test_dma_live&&n==4096&&caps==(MALLOC_CAP_INTERNAL|MALLOC_CAP_DMA));
    if(test_dma_fail_alloc)return NULL;
    test_dma_live=aligned_alloc(a,n);assert(test_dma_live);++test_dma_allocs;return test_dma_live;
}
static inline void heap_caps_free(void* p){if(p&&p==test_dma_live){test_dma_live=NULL;++test_dma_frees;}free(p);}
