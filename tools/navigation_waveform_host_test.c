/* SPDX-License-Identifier: Apache-2.0
 * 中文：检查实际短表每对灰阶只向目标驱动，不能把它当作实屏灰阶或残影验证。
 * English: Check that every actual short-table gray pair drives only toward the target; this does not validate optical grays or ghosting.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "e0470_epaper_waveform.h"
#include "display_main_waveform.h"
#include "ui/ui_image_dither.h"

int main(void) {
    e0470_waveform_init();
    const EpdWaveformPhases* short_table = e0470_waveform_phases(&E0470_FOLLOW_WAVEFORM, 1);
    const EpdWaveformPhases* gray_table = e0470_waveform_phases(&E0470_WAVEFORM, 5);
    assert(short_table && gray_table && short_table->phases == 8);
    const EpdWaveformPhases* original_bw = e0470_waveform_phases(&E0470_WAVEFORM, MODE_DU);
    uint8_t saved_bw[E0470_FULL_DU_FRAMES * 16 * 4], saved_gray[E0470_GL16_FRAMES * 16 * 4];
    assert(original_bw && original_bw->phases == E0470_FULL_DU_FRAMES);
    memcpy(saved_bw, original_bw->luts, sizeof(saved_bw));
    memcpy(saved_gray, gray_table->luts, sizeof(saved_gray));
    int gray_reverse_dark = 0;
    for (int from = 0; from < 16; ++from) {
        for (int to = 0; to < 16; ++to) {
            int pushes = 0;
            for (int f = 0; f < short_table->phases; ++f) {
                const int action = e0470_phase_action(short_table, f, to, from);
                if (from == to) assert(action == 0);
                else if (action) {
                    assert(action == (to > from ? 2 : 1));
                    ++pushes;
                }
            }
            if (from != to) assert(pushes > 0 && pushes <= 8);
            if (from == 0 && to == 15) assert(pushes == 8);
            if (from == 15 && to == 0) assert(pushes == 7);
            // GL16 中部分变浅迁移含反向压黑；短表所有对应迁移均已检查无反向动作。
            // Some GL16 brightening pairs include reverse darkening; every corresponding short-table pair above has no reverse action.
            if (to > from) {
                for (int f = 0; f < gray_table->phases; ++f)
                    gray_reverse_dark += e0470_phase_action(gray_table, f, to, from) == 1;
            }
        }
    }
    assert(gray_reverse_dark > 0);

    // 局部灰阶不裁短任何变色迁移，只有等灰动作清零；原始校准表不得被原地改写。
    // Local grays do not truncate changed transitions: only equal-gray actions are cleared; the original calibration table must not be modified in place.
    const EpdWaveform* local = display_main_gray_waveform();
    const EpdWaveformPhases* bands = e0470_waveform_phases(local, MODE_GL16);
    const EpdWaveformPhases* bw = e0470_waveform_phases(&E0470_WAVEFORM, MODE_DU);
    assert(local != &E0470_WAVEFORM && local->temp_intervals == E0470_WAVEFORM.temp_intervals);
    assert(bands && bands->phases == gray_table->phases && bands->phase_times == gray_table->phase_times);
    assert(bw && bw->phases == 20);
    for (int f = 0; f < bw->phases; ++f) {
        assert(e0470_phase_action(bw, f, 0, 0) == 0);
        assert(e0470_phase_action(bw, f, 15, 15) == 0);
    }
    int original_diagonal = 0;
    for (int f = 0; f < bands->phases; ++f)
        for (int from = 0; from < 16; ++from)
            for (int to = 0; to < 16; ++to) {
                int action = e0470_phase_action(bands, f, to, from);
                int original = e0470_phase_action(gray_table, f, to, from);
                if (from == to) {assert(action == 0); original_diagonal += original != 0;}
                else assert(action == original);
            }
    assert(original_diagonal > 0 && display_main_gray_waveform() == local);
    const EpdWaveformPhases *exit_table=e0470_waveform_phases(display_main_shelf_exit_waveform(),MODE_DU);
    assert(exit_table&&exit_table->phases==20);
    for(int f=0;f<20;++f)for(int from=0;from<16;++from)for(int to=0;to<16;++to)
        assert(e0470_phase_action(exit_table,f,to,from)==(from==to?0:e0470_phase_action(bw,f,to,from)));
    const EpdWaveform *profiles[]={display_main_gray_waveform(),display_main_water_waveform(),display_main_normal_waveform()};
    for(unsigned i=0;i<3;++i){
        const EpdWaveformPhases *table=e0470_waveform_phases(profiles[i],MODE_GL16);
        assert(table&&table->phases==37&&!table->phase_times);
        for(int f=0;f<37;++f)for(int from=0;from<16;++from)for(int to=0;to<16;++to)
            assert(e0470_phase_action(table,f,to,from)==(from==to?0:e0470_phase_action(gray_table,f,to,from)));
    }
    assert(!memcmp(saved_bw,bw->luts,sizeof(saved_bw))&&!memcmp(saved_gray,gray_table->luts,sizeof(saved_gray)));
    // 点阵亮度随输入单调增加且稳定，不引入中灰；阅读插图原16灰输出仍保留。
    // Dot brightness increases monotonically and remains stable with no intermediate grays; existing 16-gray reader output is retained.
    int previous_white = 0, previous_cover_white = 0;
    for (int gray = 0; gray <= 255; ++gray) {
        int white = 0, cover_white = 0;
        for (int y = 0; y < 4; ++y) for (int x = 0; x < 4; ++x) {
            int v = ui_image_dither_bw((uint8_t)gray, x, y);
            assert(v == 0 || v == 255);
            assert(v == ui_image_dither_bw((uint8_t)gray, x + 4, y + 4));
            assert(v == ui_image_dither_bw((uint8_t)gray, x - 4, y - 4));
            white += v == 255;
            int cover=ui_image_dither_cover_bw((uint8_t)gray,x,y);
            assert(cover==0 || cover==255);
            assert(cover==ui_image_dither_cover_bw((uint8_t)gray,x+4,y+4));
            assert(cover==ui_image_dither_cover_bw((uint8_t)gray,x-4,y-4));
            assert(cover>=v);
            cover_white+=cover==255;
        }
        assert(white >= previous_white); previous_white = white;
        assert(cover_white>=previous_cover_white); previous_cover_white=cover_white;
        if (gray == 0) assert(white == 0 && cover_white==1);
        if (gray == 128) assert(white == 8 && cover_white==8);
    }
    assert(previous_white == 16 && previous_cover_white==16 && ui_image_dither_gray(119, 0, 0) == 119);
    puts("navigation waveform: calibrated GL16, unchanged grays, shelf whitespace erase, original water midgrays, vendor tables and lighter covers passed; optical behavior requires hardware");
    return 0;
}
