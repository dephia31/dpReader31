#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#ifndef DPREADER_LOVE_BOOK
#define DPREADER_LOVE_BOOK 0
#endif
// 先检查描述字段终止，再比较固件分支。/ Check descriptor termination before comparing editions.
static inline bool firmware_same_edition(const char *a, size_t an, const char *b, size_t bn) {
    return a && b && memchr(a,0,an) && memchr(b,0,bn) &&
           ((strstr(a,"-love-vn")!=NULL)==(strstr(b,"-love-vn")!=NULL));
}
