/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include "epdiy.h"
#include <stdbool.h>

/// 柔和文字序列：白底到中灰的少量回浅脉冲提前；具体阅读模式由板级出口选择。
/// Soft text phases advance a few gray-edge settling pulses; board output selects the reading modes that use them.
const EpdWaveform* display_reader_text_waveform(void);

#include "epd_highlevel.h"
/// 速读复用各板原厂DU相位：Pico用MODE_DU，S31使用批量接口别名。/ Fast BW retains board-native DU phases: MODE_DU on Pico, batch-mode alias on S31.
const EpdWaveform* display_reader_fast_text_waveform(void);
/// 只归一文字区域目标，保留真实back和区域外灰阶。/ Normalize only the text target, retaining real history and outside grays.
bool display_reader_fast_text_prepare(EpdiyHighlevelState* hl,EpdRect area);

#if KIIKO_BOARD_METALIO47
/// 本地普通/水波纹共用的22相是否可用；常规构建恒false。/ Shared local Ordinary/Ripple22-phase availability; false in regular builds.
bool display_reader_fast_ripple_enabled(void);
#endif
