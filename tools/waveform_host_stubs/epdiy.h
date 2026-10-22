/* SPDX-License-Identifier: Apache-2.0
 * 中文：波形装配原生测试仅需要真实波形结构。/ English: Native waveform assembly tests require only the actual waveform structures.
 */
#pragma once
#include "epd_waveform.h"
enum EpdDrawMode { MODE_DU = 1, MODE_GC16 = 2, MODE_GL16 = 5 };

typedef struct {int x,y,width,height;} EpdRect;
enum EpdRotation {EPD_ROT_LANDSCAPE,EPD_ROT_PORTRAIT,EPD_ROT_INVERTED_LANDSCAPE,EPD_ROT_INVERTED_PORTRAIT};
int epd_width(void);
int epd_height(void);
enum EpdRotation epd_get_rotation(void);
