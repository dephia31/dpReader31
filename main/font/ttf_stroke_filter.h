/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include <stdint.h>

// 按覆盖率的距离外推轮廓，每档1/16像素；不在两个位图间混合出第二层灰边。
// Offset the coverage contour in 1/16-pixel steps, without blending a second gray halo.
// 源目标不可重叠，每边预留ceil(abs(steps)/16)像素。/ Buffers must not overlap; pad each edge by ceil(abs(steps)/16).
static inline void ttf_stroke_filter_fine(uint8_t *dst, const uint8_t *src, int width, int height,
                                        int px, int steps) {
    (void)px;
    if (steps < -32) steps = -32;
    if (steps > 32) steps = 32;
    int amount = steps < 0 ? -steps : steps, radius = (amount + 15) / 16;
    int stride = width + 2 * radius, offset = (amount * 255 + 8) / 16;
    static const int distance[3][3] = {{0,255,510},{255,361,570},{510,570,721}};
    for (int y = -radius; y < height + radius; ++y) {
        for (int x = -radius; x < width + radius; ++x) {
            int best = -1024;
            for (int dy = -radius; dy <= radius; ++dy) {
                for (int dx = -radius; dx <= radius; ++dx) {
                    int nx = x + dx, ny = y + dy;
                    int value = nx >= 0 && nx < width && ny >= 0 && ny < height
                        ? src[ny * width + nx] : 0;
                    if (steps < 0) value = 255 - value;
                    if (!value) continue;
                    int d = distance[dy < 0 ? -dy : dy][dx < 0 ? -dx : dx];
                    if (value - d > best) best = value - d;
                }
            }
            int coverage = best + offset;
            if (coverage < 0) coverage = 0;
            if (coverage > 255) coverage = 255;
            dst[(y + radius) * stride + x + radius] = (uint8_t)(steps < 0 ? 255 - coverage : coverage);
        }
    }
}
static inline void ttf_stroke_filter(uint8_t *dst,const uint8_t *src,int width,int height,int px,int level){
    if(level < -2)level=-2;
    if(level > 2)level=2;
    ttf_stroke_filter_fine(dst,src,width,height,px,level*16);
}
