/* SPDX-License-Identifier: Apache-2.0
 * Bảng âm tiết gợi ý nhanh nguyên âm có dấu Tiếng Việt cho bàn phím ảo dpReader.
 */
#pragma once
#include <stdint.h>

static const char search_syllables[] =
    "a\0"
    "aa\0"
    "aw\0"
    "d\0"
    "dd\0"
    "e\0"
    "ee\0"
    "i\0"
    "o\0"
    "oo\0"
    "ow\0"
    "u\0"
    "uw\0"
    "y\0";

static const uint16_t search_syllable_offsets[] = {
    0, 2, 5, 8, 10, 13, 15, 18, 20, 22, 25, 28, 30, 33
};
