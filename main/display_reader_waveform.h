/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include "epdiy.h"

/// 普通纯文字翻页：官方 GL37 顺序，屏蔽等值迁移，不额外调序。
/// Plain text turns: vendor GL37 ordering, equal pairs held, without extra reordering.
const EpdWaveform* display_reader_text_waveform(void);
