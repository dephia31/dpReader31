/* SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
 * 中文：当前页面上的透明PNG书封；输入framebuffer即底图，不加载书籍。
 * English: Transparent PNG cover over the current framebuffer; never load a book for its background.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "epdiy.h"
bool book_lock_bookmark_draw(uint8_t *fb,const char *png);
void book_lock_bookmark_preview_paint(uint8_t *fb,const uint8_t *canvas,EpdRect area);
