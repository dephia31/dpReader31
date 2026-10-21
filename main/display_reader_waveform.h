/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include "epdiy.h"
#include <stdbool.h>

/// 普通纯文字翻页：白底到中灰的少量回浅脉冲提前，其余迁移保留原表。
/// Plain text turns: advance a few gray-edge settling pulses; retain all other transitions.
const EpdWaveform* display_reader_text_waveform(void);

#if KIIKO_BOARD_METALIO47
#include "epd_highlevel.h"
/// S31速读使用原厂DU20原相位，仅输出黑白；不改变普通灰阶模式。/ S31 fast text reuses exact vendor DU20 for BW output, keeping ordinary grayscale separate.
const EpdWaveform* display_reader_fast_text_waveform(void);
/// 只归一文字区域目标，保留真实back和区域外灰阶。/ Normalize only the text target, retaining real history and outside grays.
bool display_reader_fast_text_prepare(EpdiyHighlevelState* hl,EpdRect area);
/// 本地普通/水波纹共用的22相是否可用；常规构建恒false。/ Shared local Ordinary/Ripple22-phase availability; false in regular builds.
bool display_reader_fast_ripple_enabled(void);
#endif
