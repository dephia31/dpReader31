#pragma once
#include <stdbool.h>
#include <stddef.h>

static inline bool book_title_get(const char *path, char *out, size_t cap) {
    (void)path;
    if (cap) out[0] = 0;
    return false;
}

static inline int book_title_set(const char *path, const char *title) {
    (void)path;
    (void)title;
    return 0;
}
static inline int book_title_clear(const char *path) {(void)path;return 0;}
