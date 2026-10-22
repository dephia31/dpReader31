/* SPDX-License-Identifier: Apache-2.0
 * 中文：主页面灰阶保持未变像素；书架退出加强同向擦白，主页水波纹端点单向推动，中灰保持原序列。
 * English: Main-page grays hold unchanged pixels; shelf exit strengthens directional whitening and main water uses directional endpoints while retaining original midgray sequences.
 */
#pragma once
#include "epdiy.h"
const EpdWaveform* display_main_gray_waveform(void);
/// 离开书架单向变白并补四相，避免GC16反向压黑；其他迁移用原DU，等值保持。/ Shelf exit whitens directionally with four extra phases, avoiding GC16 reverse darkening; other transitions use original DU and equal values hold.
const EpdWaveform* display_main_shelf_exit_waveform(void);
/// 水波纹与普通切页共用：端点单向驱动，中灰变色保留完整序列，等灰保持；调用者决定是否错相。
/// Shared by water and ordinary transitions: directional endpoints, complete changed midgray sequences and held equal grays; callers choose staggering.
const EpdWaveform* display_main_water_waveform(void);
/// 快刷首帧保留原DU并保持所有等值像素，亚克力首帧不驱动。/ Fast first pass retains original DU while holding every equal pair, including skipped acrylic.
const EpdWaveform* display_main_bw_waveform(void);

/// 普通模式灰边交错，端点/相数/推动数量不变；OOM回退原柔和灰阶。
/// Ordinary gray-edge interleaving keeps endpoints/phase counts/pulse counts; OOM falls back to original soft grays.
const EpdWaveform* display_main_normal_waveform(void);

#if KIIKO_BOARD_METALIO47
/// 用原DU20相位供已有条带动画读取；仅适配mode元数据。/ Expose original DU20 phases to the existing band engine; adapt mode metadata only.
const EpdWaveform* display_main_bw_turn_waveform(void);
/// 底栏灰底轻量恢复：原DU20端点、白底FOLLOW灰推，非白先擦白，等灰保持；不改原厂表。
/// Quiet navigation restore: vendor DU20 endpoints, FOLLOW grays from white, whiten nonwhite origins first, hold equals; vendor tables remain intact.
const EpdWaveform* display_main_navigation_waveform(void);
/// 书架普通切页柔和题头/分页；使用本机DU20端点，中灰GL48保持等值，无额外相位。
/// Soft shelf chrome uses this panel's DU20 endpoints and equal-held GL48 midgrays, without extra phases.
const EpdWaveform* display_main_shelf_soft_waveform(void);
/// 封面/材质保留完整本机GL/GC48，白色留白沿用柔和擦白；手动全刷不使用此派生表。
/// Art retains complete local GL/GC48; white space uses soft erase. Manual full refresh keeps the original tables.
#else
/// 书架封面/材质保留校准灰阶，白色留白只向白推动；只供普通切页，手动全刷保留原表。
/// Shelf art keeps calibrated grays while white space drives only white; ordinary transitions only, preserving vendor manual full refresh.
#endif
const EpdWaveform* display_main_shelf_waveform(enum EpdDrawMode mode);
#if KIIKO_BOARD_METALIO47

/// 整页输出保留15→15补白；固定底栏及局部控件继续使用原等值保持表。
/// Page profiles retain white-to-white maintenance; frozen navigation and local controls use equal-held profiles.
const EpdWaveform* display_main_page_gray_waveform(void);
const EpdWaveform* display_main_page_bw_waveform(void);
#else
#endif

#if KIIKO_BOARD_METALIO47
#else
/// 设置滚动使用已有八相灰阶差分，只向目标推动；不二值化字缘，不申请额外波形内存。
/// Settings scrolling uses existing eight-phase directional gray differences, without thresholding glyph edges or allocating another waveform.
const EpdWaveform* display_settings_scroll_waveform(void);
/// 系统底色恢复原完整柔和灰阶，白底同向擦除；底栏由调用方裁剪保护，不改原始波形。
/// Restore complete original soft system grays with directional white erasure; callers crop navigation and vendor tables remain intact.
const EpdWaveform* display_main_ui_waveform(void);
#endif
