/* SPDX-License-Identifier: Apache-2.0
 * 中文：主页面灰阶保持未变像素；书架退出加强同向擦白，主页水波纹端点单向推动，中灰保持原序列。
 * English: Main-page grays hold unchanged pixels; shelf exit strengthens directional whitening and main water uses directional endpoints while retaining original midgray sequences.
 */
#pragma once
#include "epdiy.h"
const EpdWaveform* display_main_gray_waveform(void);
/// 官方 DU20，等值迁移保持，无额外擦白相。/ Vendor DU20 with equal pairs held, no extra whitening phases.
const EpdWaveform* display_main_shelf_exit_waveform(void);
/// 默认灰阶表（厂家 2.0.76 起完整 GL48），等值迁移保持，条带调度由页面组件负责。
/// Default gray table (full GL48 since vendor 2.0.76), equal pairs held; bands belong to the page-turn component.
const EpdWaveform* display_main_water_waveform(void);
const EpdWaveform* display_main_bw_waveform(void);
const EpdWaveform* display_main_normal_waveform(void);
