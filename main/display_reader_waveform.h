/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include "epdiy.h"
#include <stdbool.h>

/// 阅读文字翻页的板级序列：Pico 做柔和相序调序，Metalio 用默认灰阶表相序（厂家 2.0.76 起完整 GL48）。
/// Reader text sequence per board: the Pico re-orders soft phases, Metalio keeps the default gray order.
const EpdWaveform* display_reader_text_waveform(void);
