/* SPDX-License-Identifier: Apache-2.0
 * 中文：倒置竖屏列表的黑白目标与参考帧同步，无分配，不触及固定题头/底栏。
 * English: Binary targets for inverted-portrait lists; allocation-free, preserving fixed chrome.
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static inline bool ui_follow_pixels(uint8_t *front, uint8_t *target, int width, int height,
                                    int top, int bottom) {
    if (!front || !target || width <= 0 || (width & 1) || height <= 0 ||
        top < 0 || bottom > width || bottom <= top || (top & 1) || (bottom & 1)) return false;
    for (int row = 0; row < height; ++row) {
        uint8_t *p = front + (size_t)row * (width / 2) + top / 2;
        int col = 0, bytes = (bottom-top)/2;
        for (; col + 4 <= bytes; col += 4) {
            uint32_t old;
            memcpy(&old, p + col, sizeof(old));
            uint32_t white = old & UINT32_C(0x88888888);
            white |= white >> 1 | white >> 2 | white >> 3;
            if(white!=old)memcpy(p + col, &white, sizeof(white));
        }
        for (; col < bytes; ++col) {
            uint8_t old = p[col];
            p[col] = (old & 8 ? 15 : 0) | (old & 128 ? 240 : 0);
        }
    }
    return true;
}
static inline void ui_follow_pixels_remember(const uint8_t *front, uint8_t *target,
                                             int width, int height, int top, int bottom) {
    for (int row = 0; row < height; ++row) {
        size_t offset = (size_t)row * (width/2) + top/2;
        memcpy(target+offset, front+offset, (size_t)(bottom-top)/2);
    }
}
