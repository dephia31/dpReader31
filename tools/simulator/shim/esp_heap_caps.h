#pragma once
#include <stdlib.h>
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
#define MALLOC_CAP_INTERNAL 4
#define MALLOC_CAP_DMA 8
#define MALLOC_CAP_DEFAULT 0
static inline void *heap_caps_malloc(size_t n,int c){(void)c;return malloc(n);}
static inline void *heap_caps_calloc(size_t n,size_t z,int c){(void)c;return calloc(n,z);}
static inline void *heap_caps_realloc(void *p,size_t n,int c){(void)c;return realloc(p,n);}
static inline void heap_caps_free(void *p){free(p);}
static inline size_t heap_caps_get_free_size(int c){(void)c;return 32*1024*1024;}
static inline size_t heap_caps_get_largest_free_block(int c){return heap_caps_get_free_size(c);}
static inline size_t heap_caps_get_minimum_free_size(int c){return heap_caps_get_free_size(c);}

static inline void *heap_caps_aligned_alloc(size_t a,size_t n,int c){(void)c;void *p=NULL;if(posix_memalign(&p,a,n))return NULL;return p;}
