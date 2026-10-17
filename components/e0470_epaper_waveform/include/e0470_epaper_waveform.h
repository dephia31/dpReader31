/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * Metalio E0470A14-AF（34pin）的 epdiy 波形；使用官方运行时裁剪参数。
 *   E0470_WAVEFORM         官方运行裁剪：GC36 / GL37 / 阈值 DU20
 *   E0470_FULL_WAVEFORM    完整 GC48 / GL48，供清理、插图、锁屏和恢复
 *   E0470_GRAY8_WAVEFORM   旧接口兼容名，S31仍指向完整48相，未减少灰阶
 *   E0470_FOLLOW_WAVEFORM  既有软件短刷8相，逐步跟随；不是官方DU20
 * GL16既有白底推送兼容保留；必须先调用e0470_waveform_init()。
 *
 * Metalio E0470A14-AF(34-pin): vendor GC36/GL37/DU20 runtime trim;
 * retain full48 for quality/recovery. GRAY8 is a compatibility alias of
 * full48. FOLLOW8 is the existing software short update, not vendor DU20.
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
#define E0470_TRIM_ERASE_MAX 11
#define E0470_TRIM_SAT_CUT 5
#define E0470_TRIM_WHITE_SAT_CUT 0
#define E0470_TRIM_HOLD 3
#define E0470_GC16_FRAMES 36
#define E0470_GL16_FRAMES 37

/// S31驱动相周期下限12ms；实际仍按完整面板扫描时长。
/// S31 minimum phase12ms; actual cadence includes the full physical scan.
#define E0470_WAVEFORM_FRAME_US 12000
extern const EpdWaveform E0470_WAVEFORM;

/// 普通文字采用官方GL37相序，仅屏蔽相同灰阶迁移；需真实4bpp历史，保留周期清理。
/// Text uses vendor GL37 pulse order with equal-pixel hold; require true4bpp history and periodic cleanup.
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

/// 生成 E0470_WAVEFORM 与 E0470_FOLLOW_WAVEFORM 的表。必须在第一次刷新之前调用一次。
/// / Build E0470_WAVEFORM and E0470_FOLLOW_WAVEFORM. Call once before the first refresh.
void e0470_waveform_init(void);

#ifdef __cplusplus
}
#endif
