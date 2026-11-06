/* SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
/// S31基线JPEG硬解、渐进式完整软解；失败由调用者重置文件后走原解码器。
/// S31 baseline hardware / full progressive software; callers rewind before legacy fallback.
bool metalio_jpeg_gray(const uint8_t *data, size_t size, FILE *file,
                       unsigned width, unsigned height, uint8_t *out);
