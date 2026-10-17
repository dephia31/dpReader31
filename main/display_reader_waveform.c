/* SPDX-License-Identifier: Apache-2.0
 * 中文：阅读采用默认灰阶表相序（厂家 2.0.76 起完整 GL48），等值保持由派生表保证。
 * English: Reading uses the default gray-table order (full GL48 since vendor 2.0.76)
 *         with equal pairs held by the derived table.
 */
#include "display_reader_waveform.h"
#include "e0470_epaper_waveform.h"
const EpdWaveform* display_reader_text_waveform(void) { return &E0470_TEXTTURN_WAVEFORM; }
