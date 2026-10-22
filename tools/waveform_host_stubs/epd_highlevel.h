/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include <stdint.h>
#include "epdiy.h"
// 波形测试只访问前后目标；扫描器由独立测试覆盖。/ Waveform tests access targets/history only; separate tests own scanning.
typedef struct {uint8_t* front_fb;uint8_t* back_fb;} EpdiyHighlevelState;
