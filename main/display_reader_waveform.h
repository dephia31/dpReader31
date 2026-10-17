/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include "epdiy.h"

/// 普通纯文字翻页：默认灰阶表相序（厂家 2.0.76 起完整 GL48），屏蔽等值迁移，不额外调序。
/// Plain text turns: default gray-table order (full GL48 since vendor 2.0.76), equal pairs held.
const EpdWaveform* display_reader_text_waveform(void);
