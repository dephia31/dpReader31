/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * Metalio E0470A14-AF（34pin）的 epdiy 波形；默认使用厂家 2.0.76 的完整表。
 *   E0470_WAVEFORM         默认完整 GC48 / GL48 / 阈值 DU20（E0470_WAVEFORM_TRIM=0）
 *   E0470_FULL_WAVEFORM    同一组完整表，供清理、插图、锁屏和恢复
 *   E0470_GRAY8_WAVEFORM   旧接口兼容名，S31仍指向完整48相，未减少灰阶
 *   E0470_FOLLOW_WAVEFORM  既有软件短刷8相，逐步跟随；不是官方DU20
 * GL16既有白底推送兼容保留；必须先调用e0470_waveform_init()。
 *
 * 厂家 2.0.76-debug（2026-10-07 源码包）把 MY_WAVEFORM_TRIM 默认改回 0：
 * 之前开机裁剪出的 GC36/GL37 把黑目标饱和期从 18 帧减到 13 帧，强光下墨色偏淡、
 * 原生灰阶还有级别合并和非单调跳变；DU20 及三份原始数组逐字节相同，问题出在裁剪。
 * 这里跟厂家一致默认用完整 48 相；确需更快的翻页可把 E0470_WAVEFORM_TRIM 改 1 回退。
 *
 * Metalio E0470A14-AF(34-pin): default to the vendor 2.0.76 full tables
 * (GC48/GL48/DU20). The 2.0.76-debug source turned trimming off: the trimmed
 * GC36/GL37 cut the black saturation from 18 to 13 frames, which left ink pale
 * in strong light and made the native gray ladder merge/non-monotonic; DU20 and
 * all three raw arrays are byte-identical, so trimming was the problem. Set
 * E0470_WAVEFORM_TRIM to 1 for the faster trimmed page tables.
 * Initialize once before the first refresh; retain existing GL white hold.
 */

#pragma once

#include <stdint.h>

#include "epd_waveform.h"

#ifdef __cplusplus
extern "C" {
#endif

/// Metalio E0470A14-AF：原始 DU20 / GC48 / GL48。
/// Reference Metalio E0470A14-AF DU20 / GC48 / GL48 tables.
#define E0470_FULL_DU_FRAMES 20
#define E0470_FULL_GC16_FRAMES 48
#define E0470_FULL_GL16_FRAMES 48
extern const EpdWaveform E0470_FULL_WAVEFORM;

/// 旧 API 兼容名；S31 本地版沿用参考表，不启用 Pico 的 8 灰阶表。
/// Legacy compatibility name: S31 uses the reference tables without Pico 8-gray calibration.
#define E0470_GRAY8_GC16_FRAMES 48
#define E0470_GRAY8_GL16_FRAMES 48
extern const EpdWaveform E0470_GRAY8_WAVEFORM;

/// 用户授权采用 Metalio b400605 官方裁剪；原始 48 相表仍完整保留。
/// User-authorized Metalio b400605 trim; the original 48-phase tables remain intact.
/// 裁剪按板选择：Pico 真机一直用裁剪后的快页表；Metalio 从 commit 71e9adf 起改用厂家 2.0.76
/// 的完整 48 相（裁剪会让强光下墨色偏淡、灰阶合并）。两块板的面板型号不同，不能统一。
/// The trim is board-specific: the Pico has always shipped the trimmed fast page tables, while
/// Metalio switched to the vendor's full 48 phases in commit 71e9adf (trimming left ink pale in
/// strong light and merged the gray ladder). Two different panel parts, so this cannot be shared.
#if defined(KIIKO_BOARD_METALIO47) && KIIKO_BOARD_METALIO47
#define E0470_WAVEFORM_TRIM 0
#else
#define E0470_WAVEFORM_TRIM 1
#endif

#define E0470_TRIM_ERASE_MAX 11
#define E0470_TRIM_SAT_CUT 5
#define E0470_TRIM_WHITE_SAT_CUT 0
#define E0470_TRIM_HOLD 3
#if E0470_WAVEFORM_TRIM
#define E0470_GC16_FRAMES 36
#define E0470_GL16_FRAMES 37
#else
#define E0470_GC16_FRAMES E0470_FULL_GC16_FRAMES
#define E0470_GL16_FRAMES E0470_FULL_GL16_FRAMES
#endif

/// S31驱动相周期下限12ms；实际仍按完整面板扫描时长。
/// S31 minimum phase12ms; actual cadence includes the full physical scan.
#define E0470_WAVEFORM_FRAME_US 12000
extern const EpdWaveform E0470_WAVEFORM;

/// 普通文字沿用 E0470_WAVEFORM 的 GL 相序，仅屏蔽相同灰阶迁移；需真实4bpp历史，保留周期清理。
/// Text keeps the E0470_WAVEFORM GL order with equal-pixel hold; require true4bpp history and periodic cleanup.
extern const EpdWaveform E0470_TEXTTURN_WAVEFORM;

/// 触摸笔迹跟手用的 8 帧短 DU；S31 保留每相至少 12ms。
/// 推动次数按 |to-from| 比例分配，满幅迁移往黑推 7 相、往白推 8 相。
/// / 8-frame short DU for touch ink; S31 retains at least 12 ms per phase. Push count
/// scales with |to-from|; full travel is 7 black / 8 white.
#define E0470_FOLLOW_BLACK_FRAMES 7
#define E0470_FOLLOW_WHITE_FRAMES 8
#define E0470_FOLLOW_FRAMES 8
extern const EpdWaveform E0470_FOLLOW_WAVEFORM;

/// 单相位执行表供水波纹的错相 GL16 扫描使用；像素动作来自逐带相位 LUT。
/// One-phase apply waveform for staggered GL16; per-band phase LUTs supply pixel actions.
extern const EpdWaveform E0470_APPLY_WAVEFORM;

/// 按上面的公式生成 frames 帧的跟随表写进 dst（容量 frames×16×4 字节）。
/// / Build a follow table of `frames` into dst (frames×16×4 bytes).
void e0470_follow_lut_build(int frames, uint8_t (*dst)[16][4]);

/// 在一条 EpdWaveform 里按模式（MODE_GC16 / MODE_GL16 / MODE_DU）找唯一温度档的相位表。
/// / Look up the single temp-range phase table for MODE_GC16 / MODE_GL16 / MODE_DU.
const EpdWaveformPhases* e0470_waveform_phases(const EpdWaveform* waveform, int mode);

/// 查 (from→to) 在第 phase 相的 2bit 动作：0 保持、1 压黑、2 擦白。
/// / 2-bit action for (from→to) at `phase`: 0 hold, 1 darken, 2 erase.
int e0470_phase_action(const EpdWaveformPhases* phases, int phase, int to, int from);

/// 给"白→白"等值迁移补一相擦白（不增加相数），全像素推屏时白底才会被重新驱动；
/// 否则白底只靠电荷保持，强光照射后逐渐变灰。返回补入的相序号，-1 表示没找到白相。
/// / Add one erase pulse to the unchanged-white transition without adding phases, so a
/// full-pixel pass re-drives the white background instead of only holding it; otherwise
/// strong light greys it out. Returns the phase index written, or -1 when none was found.
int e0470_waveform_white_tick(uint8_t (*data)[16][4], int frames);

/// 在表尾再补 extra 帧擦白（只写 15→15 这一格，不增加相数），把白像素充得更深，
/// 强光下更不容易漏灰。返回实际补入的帧数。
/// / Add `extra` tail erase pulses on the unchanged-white cell only, without adding
/// phases, so white pixels hold deeper charge and grey out later under strong light.
int e0470_waveform_white_boost(uint8_t (*data)[16][4], int frames, int extra);

/// 生成 E0470_WAVEFORM 与 E0470_FOLLOW_WAVEFORM 的表。必须在第一次刷新之前调用一次。
/// / Build E0470_WAVEFORM and E0470_FOLLOW_WAVEFORM. Call once before the first refresh.
void e0470_waveform_init(void);

#ifdef __cplusplus
}
#endif
