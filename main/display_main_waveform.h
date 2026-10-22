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

/// 书架普通切页柔和题头/分页；使用本机DU20端点，中灰GL48保持等值，无额外相位。
/// Soft shelf chrome uses this panel's DU20 endpoints and equal-held GL48 midgrays, without extra phases.
#if KIIKO_BOARD_METALIO47
/// S31 书架题头/横条专用派生表；Pico 复用普通翻页序列。
/// S31-only shelf chrome table; the Pico reuses the ordinary page sequence.
const EpdWaveform* display_main_shelf_soft_waveform(void);
#endif
/// 封面/材质保留完整本机GL/GC48，白色留白沿用柔和擦白；手动全刷不使用此派生表。
/// Art retains complete local GL/GC48; white space uses soft erase. Manual full refresh keeps the original tables.
const EpdWaveform* display_main_shelf_waveform(enum EpdDrawMode mode);

/// 整页输出保留15→15补白；固定底栏及局部控件继续使用原等值保持表。
/// Page profiles retain white-to-white maintenance; frozen navigation and local controls use equal-held profiles.
const EpdWaveform* display_main_page_gray_waveform(void);
const EpdWaveform* display_main_page_bw_waveform(void);
/// 设置列表滚动保留抗锯齿灰阶，变白使用原DU擦除，不叠加书架的四相补白；低内存回退现有柔和表。
/// Settings scrolling retains AA grays with native DU whitening, omitting the shelf's four extra white phases; low memory keeps the existing soft table.
const EpdWaveform* display_settings_scroll_waveform(void);
/// 系统底色恢复原完整柔和灰阶，白底同向擦除；底栏由调用方裁剪保护，不改原始波形。
/// Restore complete original soft system grays with directional white erasure; callers crop navigation and vendor tables remain intact.
const EpdWaveform* display_main_ui_waveform(void);
