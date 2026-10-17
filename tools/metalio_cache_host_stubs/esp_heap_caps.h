#pragma once
#include <stddef.h>
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
void *heap_caps_malloc(size_t,unsigned);
void *heap_caps_calloc(size_t,size_t,unsigned);
size_t heap_caps_get_free_size(unsigned);

size_t heap_caps_get_largest_free_block(unsigned);
