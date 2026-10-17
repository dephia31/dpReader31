/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * Metalio V1.5 E0470A14-AF：原始表保留，普通/动画采用 b400605 官方裁剪。
 * Retain reference tables; ordinary/ripple output uses vendor b400605 runtime trimming.
 */

#include "e0470_epaper_waveform.h"

#include <assert.h>
#include <string.h>

#include "e0470_waveform_trim.h"
#include "my_waveform_du.h"
#include "my_waveform_gc.h"
#include "my_waveform_gl.h"

// 温度档 0-50°C。在此固件中没有温度分档的演示。
// / One 0–50°C temp range. This firmware has no multi-range demo.
static const EpdWaveformTempInterval e0470_intervals[] = {
    { .min = 0, .max = 50 },
};

// 把 (from, to) 的一个 2bit 动作写进 epdiy 的表：data[frame][to][from/4]，高位是 from0。
// / Write one 2-bit (from, to) action into the epdiy table: data[frame][to][from/4], MSB is from0.
static inline void lut_or(uint8_t (*data)[16][4], int f, int to, int from, int action) {
    data[f][to][from / 4] |= (uint8_t)(action << (6 - 2 * (from % 4)));
}

static inline int lut_get(const uint8_t (*data)[16][4], int f, int to, int from) {
    return (data[f][to][from / 4] >> (6 - 2 * (from % 4))) & 3;
}

void e0470_follow_lut_build(int frames, uint8_t (*dst)[16][4]) {
    memset(dst, 0, (size_t)frames * 16 * 4);
    // 两个方向各有自己的满推次数；短表（连续 DU 单帧、连调 dufr n）按表长封顶。
    // / Each direction has its own full-push count; short tables (1-frame
    // continuous DU, live-tune dufr n) cap at the table length.
    const int black_max = frames < E0470_FOLLOW_BLACK_FRAMES ? frames : E0470_FOLLOW_BLACK_FRAMES;
    const int white_max = frames < E0470_FOLLOW_WHITE_FRAMES ? frames : E0470_FOLLOW_WHITE_FRAMES;
    for (int to = 0; to < 16; to++) {
        for (int from = 0; from < 16; from++) {
            if (to == from) continue;
            const int diff = to > from ? to - from : from - to;
            const int action = to > from ? 2 : 1;  // 往白推 0b10，往黑推 0b01 / 0b10 erase, 0b01 darken
            const int budget = action == 2 ? white_max : black_max;
            // 推动次数 = ceil(diff · 满推 / 15)，至少 1：差得远多推，差得近少推。
            // / Push count = ceil(diff · full / 15), at least 1: far travels more, near travels less.
            const int pushes = (diff * budget + 14) / 15;
            for (int f = 0; f < pushes; f++) lut_or(dst, f, to, from, action);
        }
    }
}

/* ---- 跟随 DU：开机按公式生成 / Follow DU: built at boot ---- */
static uint8_t e0470_follow_data[E0470_FOLLOW_FRAMES][16][4];
static const EpdWaveformPhases e0470_follow_phases = {
    .phases = E0470_FOLLOW_FRAMES,
    .phase_times = NULL,
    .luts = (const uint8_t*)&e0470_follow_data[0],
};
static const EpdWaveformPhases* e0470_follow_ranges[] = { &e0470_follow_phases };
static const EpdWaveformMode e0470_follow_mode = {
    .type = 1,  // MODE_DU / MODE_DU
    .temp_ranges = 1,
    .range_data = &e0470_follow_ranges[0],
};
static const EpdWaveformMode* e0470_follow_modes[] = { &e0470_follow_mode };

const EpdWaveform E0470_FOLLOW_WAVEFORM = {
    .num_modes = 1,
    .num_temp_ranges = 1,
    .mode_data = e0470_follow_modes,
    .temp_intervals = e0470_intervals,
};

/* ---- 错相 GL16 执行表 / Staggered GL16 apply waveform ---- */
static uint8_t e0470_apply_data[1][16][4];
static const EpdWaveformPhases e0470_apply_phases = {
    .phases = 1,
    .phase_times = NULL,
    .luts = (const uint8_t*)&e0470_apply_data[0],
};
static const EpdWaveformPhases* e0470_apply_ranges[] = { &e0470_apply_phases };
static const EpdWaveformMode e0470_apply_mode = {
    .type = 1,
    .temp_ranges = 1,
    .range_data = &e0470_apply_ranges[0],
};
static const EpdWaveformMode* e0470_apply_modes[] = { &e0470_apply_mode };
const EpdWaveform E0470_APPLY_WAVEFORM = {
    .num_modes = 1,
    .num_temp_ranges = 1,
    .mode_data = e0470_apply_modes,
    .temp_intervals = e0470_intervals,
};

/* ---- 阈值 DU / Threshold DU ---- */
// 源表只认目标 0/15。中间灰按 50/50 切开，暗的走整段到黑、亮的走整段到白。
// from 不切片，沿用源表对真实起点的时间序列，上一帧残留的浅墨也会被推到黑或白。
// / Source tables only drive dest 0/15. Mid grays split 50/50: dark runs
// the full path to black, light the full path to white. from is not sliced;
// the source time series for the real start is reused, so leftover ink
// from the last frame is also pushed to black or white.
static uint8_t e0470_complete_du_data[E0470_FULL_DU_FRAMES][16][4];
static const EpdWaveformPhases e0470_complete_du_phases = {
    .phases = E0470_FULL_DU_FRAMES,
    .phase_times = NULL,
    .luts = (const uint8_t*)&e0470_complete_du_data[0],
};
static const EpdWaveformPhases* e0470_complete_du_ranges[] = {
    &e0470_complete_du_phases,
};
static const EpdWaveformMode e0470_complete_du_mode = {
    .type = 1,
    .temp_ranges = 1,
    .range_data = &e0470_complete_du_ranges[0],
};

static void e0470_complete_du_build(void) {
    memset(e0470_complete_du_data, 0, sizeof(e0470_complete_du_data));
    for (int to = 0; to < 16; to++) {
        const int to_bin = to < 8 ? 0 : 15;
        for (int from = 0; from < 16; from++) {
            for (int f = 0; f < E0470_FULL_DU_FRAMES; f++) {
                const int action = lut_get(my_du_25_0_data, f, to_bin, from);
                if (action != 0) lut_or(e0470_complete_du_data, f, to, from, action);
            }
        }
    }
}

// 白底 15→15 源表全保持。挂在已经「往白推」的那一相上再推 1 帧，不增加相数。
// 差分会跳过未变白像素，GL16 必须走全像素这帧才打到白底。
// / Source 15→15 is all-hold. Hang one extra white push on an already-white
// phase without adding phases. Diff skips unchanged white; GL16 must be
// full-pixel for this tick to hit the white background.
static void e0470_gl16_white_tick(uint8_t (*data)[16][4], int frames) {
    int tick = -1;
    for (int f = frames - 1; f >= 0; f--) {
        for (int from = 0; from < 15; from++) {
            if (lut_get(data, f, 15, from) == 2) {
                tick = f;
                break;
            }
        }
        if (tick >= 0) break;
    }
    if (tick < 0) tick = frames > 2 ? frames - 3 : 0;
    lut_or(data, tick, 15, 15, 2);
}

/* ---- 完整表 / Full tables ---- */
// DU 20 相，GC16 48 相；GL16 用 RAM 副本以便白底补 1 帧。
// / DU 20, GC16 48; GL16 uses a RAM copy so the white-bg tick can be added.
static uint8_t e0470_full_gl16_live[E0470_FULL_GL16_FRAMES][16][4];
static const EpdWaveformPhases e0470_full_gl16_live_phases = {
    .phases = E0470_FULL_GL16_FRAMES,
    .phase_times = NULL,
    .luts = (const uint8_t*)&e0470_full_gl16_live[0],
};
static const EpdWaveformPhases* e0470_full_gl16_live_ranges[] = {
    &e0470_full_gl16_live_phases,
};
static const EpdWaveformMode e0470_full_gl16_live_mode = {
    .type = 5, .temp_ranges = 1, .range_data = &e0470_full_gl16_live_ranges[0],
};
static const EpdWaveformMode* e0470_full_modes[] = {
    &my_wm_du,
    &my_wm_gc,
    &e0470_full_gl16_live_mode,
};

const EpdWaveform E0470_FULL_WAVEFORM = {
    .num_modes = 3,
    .num_temp_ranges = 1,
    .mode_data = e0470_full_modes,
    .temp_intervals = e0470_intervals,
};

/* ---- 8 灰阶表 / 8-gray tables ---- */
// 兼容旧 API：本地 S31 版保持参考 48 相，未提供独立 8 灰阶标定。
// Compatibility API: this S31 build keeps the reference 48 phases; no separate 8-gray calibration.
static const EpdWaveformMode* e0470_gray8_modes[] = {
    &e0470_complete_du_mode,
    &my_wm_gc,
    &my_wm_gl,
};

const EpdWaveform E0470_GRAY8_WAVEFORM = {
    .num_modes = 3,
    .num_temp_ranges = 1,
    .mode_data = e0470_gray8_modes,
    .temp_intervals = e0470_intervals,
};

/* ---- 默认表 / Default tables ---- */
// 厂家 2.0.76 起默认完整 48 相；只有 E0470_WAVEFORM_TRIM=1 才在开机裁成 GC36/GL37。
// / Vendor 2.0.76 defaults to the full 48 phases; trimming only runs with
// E0470_WAVEFORM_TRIM=1.
static uint8_t e0470_gc16_data[E0470_FULL_GC16_FRAMES][16][4];
static uint8_t e0470_gl16_data[E0470_FULL_GL16_FRAMES][16][4];
static const EpdWaveformPhases e0470_gc16_phases = {
    .phases = E0470_GC16_FRAMES,
    .phase_times = NULL,
    .luts = (const uint8_t*)&e0470_gc16_data[0],
};
static const EpdWaveformPhases e0470_gl16_phases = {
    .phases = E0470_GL16_FRAMES,
    .phase_times = NULL,
    .luts = (const uint8_t*)&e0470_gl16_data[0],
};
static const EpdWaveformPhases* e0470_gc16_ranges[] = { &e0470_gc16_phases };
static const EpdWaveformPhases* e0470_gl16_ranges[] = { &e0470_gl16_phases };
static const EpdWaveformMode e0470_gc16_mode = {
    .type = 2, .temp_ranges = 1, .range_data = &e0470_gc16_ranges[0],
};
static const EpdWaveformMode e0470_gl16_mode = {
    .type = 5, .temp_ranges = 1, .range_data = &e0470_gl16_ranges[0],
};
static const EpdWaveformMode* e0470_modes[] = {
    &e0470_complete_du_mode,
    &e0470_gc16_mode,
    &e0470_gl16_mode,
};

const EpdWaveform E0470_WAVEFORM = {
    .num_modes = 3,
    .num_temp_ranges = 1,
    .mode_data = e0470_modes,
    .temp_intervals = e0470_intervals,
};

static uint8_t e0470_textturn_gl16_data[E0470_GL16_FRAMES][16][4];
static const EpdWaveformPhases e0470_textturn_gl16_phases = {
    .phases = E0470_GL16_FRAMES,
    .phase_times = NULL,
    .luts = (const uint8_t*)&e0470_textturn_gl16_data[0],
};
static const EpdWaveformPhases* e0470_textturn_gl16_ranges[] = { &e0470_textturn_gl16_phases };
static const EpdWaveformMode e0470_textturn_gl16_mode = {
    .type = 5, .temp_ranges = 1, .range_data = &e0470_textturn_gl16_ranges[0],
};
static const EpdWaveformMode* e0470_textturn_modes[] = {
    &e0470_textturn_gl16_mode,
};

const EpdWaveform E0470_TEXTTURN_WAVEFORM = {
    .num_modes = 1,
    .num_temp_ranges = 1,
    .mode_data = e0470_textturn_modes,
    .temp_intervals = e0470_intervals,
};

const EpdWaveformPhases* e0470_waveform_phases(const EpdWaveform* waveform, int mode) {
    if (waveform == NULL) return NULL;
    const int type = mode & 0x3F;
    for (int i = 0; i < waveform->num_modes; i++) {
        if (waveform->mode_data[i]->type == type) return waveform->mode_data[i]->range_data[0];
    }
    return NULL;
}

int e0470_phase_action(const EpdWaveformPhases* phases, int phase, int to, int from) {
    if (phases == NULL || phases->luts == NULL) return 0;
    if (phase < 0 || phase >= phases->phases) return 0;
    if ((unsigned)to > 15 || (unsigned)from > 15) return 0;
    const uint8_t* cell = phases->luts + ((size_t)phase * 16 + to) * 4 + from / 4;
    return (*cell >> (6 - 2 * (from % 4))) & 3;
}

void e0470_waveform_init(void) {
    e0470_follow_lut_build(E0470_FOLLOW_FRAMES, e0470_follow_data);
    e0470_follow_lut_build(1, e0470_apply_data);
    e0470_complete_du_build();

#if E0470_WAVEFORM_TRIM
    // 参数和逐迁移算法对齐 Metalio 官方 my_waveform_init，不自行缩短脉宽。
    // Match the vendor's my_waveform_init parameters/transition algorithm without shortening pulses.
    const e0470_trim_t trim = {.erase_max=E0470_TRIM_ERASE_MAX,
        .sat_cut=E0470_TRIM_SAT_CUT, .white_sat_cut=E0470_TRIM_WHITE_SAT_CUT,
        .hold=E0470_TRIM_HOLD};
    int gc=e0470_waveform_trim(&my_gc_25_0,&trim,e0470_gc16_data);
    int gl=e0470_waveform_trim(&my_gl_25_0,&trim,e0470_gl16_data);
    assert(gc==E0470_GC16_FRAMES && gl==E0470_GL16_FRAMES);
#else
    // 厂家 2.0.76 默认完整表：裁剪会把黑目标饱和期从 18 帧减到 13 帧，强光下
    // 墨色偏淡、灰阶非单调；三份原始数组相同，问题出在裁剪本身。
    // Vendor 2.0.76 default: trimming cut the black saturation tail from 18 to
    // 13 frames, leaving pale ink in strong light and a non-monotonic gray
    // ladder; the raw arrays are identical across sources.
    memcpy(e0470_gc16_data, my_gc_25_0_data, sizeof(e0470_gc16_data));
    memcpy(e0470_gl16_data, my_gl_25_0_data, sizeof(e0470_gl16_data));
#endif

    memcpy(e0470_full_gl16_live, my_gl_25_0_data, sizeof(e0470_full_gl16_live));
    e0470_gl16_white_tick(e0470_full_gl16_live, E0470_FULL_GL16_FRAMES);
    // 完整表和裁剪表都要补白帧（厂家 2.0.76 相同处理）。
    // Both full and trimmed GL16 get the unchanged-white pulse (vendor 2.0.76).
    e0470_gl16_white_tick(e0470_gl16_data, E0470_GL16_FRAMES);
    // 文字沿用当前默认表的迁移顺序；只屏蔽等值像素，取消旧 S3 的调序/删擦白。
    // Text keeps the active GL order, masking equal pairs only.
    memcpy(e0470_textturn_gl16_data,e0470_gl16_data,sizeof(e0470_textturn_gl16_data));
    for (int f = 0; f < E0470_GL16_FRAMES; f++) {
        for (int v = 0; v < 16; v++) {
            e0470_textturn_gl16_data[f][v][v / 4] &= (uint8_t)~(3u << (6 - 2 * (v % 4)));
        }
    }

}
