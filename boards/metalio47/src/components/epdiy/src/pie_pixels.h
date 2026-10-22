// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <stdbool.h>
#include <stdint.h>

// 原C路径也是PIE的正确性/计时基准；调用者不得改变原像素顺序。
// The original C path is the PIE correctness/timing reference; retain pixel order.
uint32_t epd_interlace_4bpp_line_c(const uint8_t*, const uint8_t*, uint8_t*, uint8_t*, int);
void epd_apply_line_mask_c(uint8_t*, const uint8_t*, int);

// 只允许Core1任务调用；指针16B对齐，blocks>0，每块分别为16B和32像素。
// Core1 task only, with16-byte alignment and blocks>0:16 bytes/mask or32 pixels/diff.
void epd_pie_mask_blocks(uint8_t*, const uint8_t*, unsigned blocks);
uint32_t epd_pie_interlace_blocks(const uint8_t*, const uint8_t*, uint8_t*, uint8_t*, unsigned blocks);

#ifdef KIIKO_LOCAL_LATENCY
// 仅本地验证构建允许切换/测试；产品没有USB入口或基准任务。
// Only local verification builds expose toggles/tests; products have no USB bench.
void epd_pie_pixels_enable(bool enabled);
bool epd_pie_pixels_enabled(void);
void epd_pie_pixels_bench(void);
#endif
