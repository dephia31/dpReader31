/* SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
 * 中文：当前页面上的透明PNG书封；输入framebuffer即底图，不加载书籍。
 * English: Transparent PNG cover over the current framebuffer; never load a book for its background.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "epdiy.h"
/// 输出中途卡I/O失败可能留下未完成像素；返回false时调用方必须绘制锁屏兜底，不能再次叠图。
/// Mid-output SD errors may leave incomplete pixels; on false callers must draw fallback art rather than composite again.
bool book_lock_bookmark_draw(uint8_t *fb,const char *png);
void book_lock_bookmark_preview_paint(uint8_t *fb,const uint8_t *canvas,EpdRect area);
