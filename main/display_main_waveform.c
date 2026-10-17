/* SPDX-License-Identifier: Apache-2.0
 * 中文：主页与水波纹复用默认灰阶表（厂家 2.0.76 起为完整 GL48，裁剪开关打开才是 GL37）；
 *       快刷用原 DU20，只屏蔽等值迁移。
 * English: Main/ripple reuse the default gray table (full GL48 since vendor 2.0.76,
 *          GL37 only with trimming on); fast pages use original DU20 with equal pairs held.
 * 官方来源：CloudZao/Metalio-E-INK4-Plus b400605 与厂家 2.0.76-debug 源码包 /
 *          display/waveforms/my_waveform.c。
 * Vendor source: CloudZao/Metalio-E-INK4-Plus b400605 and the 2.0.76-debug source
 *          snapshot / display/waveforms/my_waveform.c.
 */
#include "display_main_waveform.h"
#include "e0470_epaper_waveform.h"
#include <stdbool.h>
#include <string.h>
static uint8_t s_gray[E0470_GL16_FRAMES][16][4];
static const EpdWaveformPhases s_gray_phases={E0470_GL16_FRAMES,(const uint8_t*)s_gray,NULL};
static const EpdWaveformPhases *s_gray_ranges[]={&s_gray_phases};
static const EpdWaveformMode s_gray_mode={5,1,s_gray_ranges};
static const EpdWaveformMode *s_gray_modes[]={&s_gray_mode};
static EpdWaveform s_gray_waveform={.num_modes=1,.num_temp_ranges=1,.mode_data=s_gray_modes};
static bool s_gray_ready;
static uint8_t s_bw[E0470_FULL_DU_FRAMES][16][4];
static const EpdWaveformPhases s_bw_phases={E0470_FULL_DU_FRAMES,(const uint8_t*)s_bw,NULL};
static const EpdWaveformPhases *s_bw_ranges[]={&s_bw_phases};
static const EpdWaveformMode s_bw_mode={1,1,s_bw_ranges};
static const EpdWaveformMode *s_bw_modes[]={&s_bw_mode};
static EpdWaveform s_bw_waveform={.num_modes=1,.num_temp_ranges=1,.mode_data=s_bw_modes};
static bool s_bw_ready;
const EpdWaveform* display_main_gray_waveform(void) {
    if(!s_gray_ready) {
        const EpdWaveformPhases *original=e0470_waveform_phases(&E0470_WAVEFORM,MODE_GL16);
        if(!original||!original->luts||original->phases!=E0470_GL16_FRAMES||original->phase_times)
            return &E0470_WAVEFORM;
        memcpy(s_gray,original->luts,sizeof(s_gray));
        for(int f=0;f<E0470_GL16_FRAMES;++f)for(int v=0;v<16;++v)
            s_gray[f][v][v/4]&=(uint8_t)~(3u<<(6-2*(v%4)));
        s_gray_waveform.temp_intervals=E0470_WAVEFORM.temp_intervals;s_gray_ready=true;
    }
    return &s_gray_waveform;
}
const EpdWaveform* display_main_bw_waveform(void) {
    if(!s_bw_ready) {
        const EpdWaveformPhases *original=e0470_waveform_phases(&E0470_WAVEFORM,MODE_DU);
        if(!original||!original->luts||original->phases!=E0470_FULL_DU_FRAMES||original->phase_times)
            return &E0470_WAVEFORM;
        memcpy(s_bw,original->luts,sizeof(s_bw));
        for(int f=0;f<E0470_FULL_DU_FRAMES;++f)for(int v=0;v<16;++v)
            s_bw[f][v][v/4]&=(uint8_t)~(3u<<(6-2*(v%4)));
        s_bw_waveform.temp_intervals=E0470_WAVEFORM.temp_intervals;s_bw_ready=true;
    }
    return &s_bw_waveform;
}
// 取消旧的额外四相擦白和中灰调序，使用同一份官方迁移与历史帧。
// Remove four extra whitening phases and gray reordering; share vendor transitions/history.
const EpdWaveform* display_main_shelf_exit_waveform(void) { return display_main_bw_waveform(); }
const EpdWaveform* display_main_water_waveform(void) { return display_main_gray_waveform(); }
const EpdWaveform* display_main_normal_waveform(void) { return display_main_gray_waveform(); }
