/* SPDX-License-Identifier: Apache-2.0
 * 中文：阅读采用 Metalio 官方 GL37 顺序，等值保持由派生表保证。
 * English: Reading uses Metalio vendor GL37 ordering with equal pairs held.
 */
#include "display_reader_waveform.h"
#include "e0470_epaper_waveform.h"
const EpdWaveform* display_reader_text_waveform(void) { return &E0470_TEXTTURN_WAVEFORM; }
