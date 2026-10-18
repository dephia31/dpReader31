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
#include "esp_heap_caps.h"
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

// 只供书架普通模式，保持其他主页/阅读/水波纹及厂家原表不变。
// Shelf ordinary mode only; retain other main/reader/ripple paths and vendor LUTs.
const EpdWaveform* display_main_shelf_soft_waveform(void) {
    static EpdWaveformPhases phases;
    static const EpdWaveformPhases *ranges[]={&phases};
    static const EpdWaveformMode mode={MODE_GL16,1,ranges};
    static const EpdWaveformMode *modes[]={&mode};
    static EpdWaveform wave={.num_modes=1,.num_temp_ranges=1,.mode_data=modes};
    if(phases.luts)return &wave;
    const EpdWaveformPhases *gray=e0470_waveform_phases(display_main_gray_waveform(),MODE_GL16);
    const EpdWaveformPhases *bw=e0470_waveform_phases(&E0470_FULL_WAVEFORM,MODE_DU);
    if(!gray||!bw||!gray->luts||!bw->luts||gray->phase_times||bw->phase_times||bw->phases>gray->phases)
        return display_main_gray_waveform();
    uint8_t *data=heap_caps_malloc((size_t)gray->phases*16*4,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!data)return display_main_gray_waveform();
    memcpy(data,gray->luts,(size_t)gray->phases*16*4);
    for(int f=0;f<gray->phases;++f)for(int to=0;to<=15;to+=15)for(int from=0;from<16;++from){
        int action=to==from||f>=bw->phases?0:e0470_phase_action(bw,f,to,from);
        int offset=(f*16+to)*4+from/4,shift=6-2*(from%4);
        data[offset]=(uint8_t)((data[offset]&~(3u<<shift))|(action<<shift));
    }
    phases=(EpdWaveformPhases){gray->phases,data,NULL};
    wave.temp_intervals=E0470_WAVEFORM.temp_intervals;
    return &wave;
}

// 仅书架普通切页：封面/材质灰阶完整保留，留白只向白推动，不再先压黑。
// Ordinary shelf transitions retain calibrated art grays; whitespace drives only white without reverse blackening.
const EpdWaveform* display_main_shelf_waveform(enum EpdDrawMode draw_mode) {
    static EpdWaveformPhases phases[2];
    static const EpdWaveformPhases *ranges[2][1]={{&phases[0]},{&phases[1]}};
    static EpdWaveformMode modes[2]={{MODE_GL16,1,ranges[0]},{MODE_GC16,1,ranges[1]}};
    static const EpdWaveformMode *mode_ptrs[2][1]={{&modes[0]},{&modes[1]}};
    static EpdWaveform waves[2]={
        {.num_modes=1,.num_temp_ranges=1,.mode_data=mode_ptrs[0]},
        {.num_modes=1,.num_temp_ranges=1,.mode_data=mode_ptrs[1]}};
    int index=draw_mode==MODE_GC16?1:0;
    if(phases[index].luts)return &waves[index];
    const EpdWaveform* soft_wave=display_main_shelf_soft_waveform();
    // 内存不足时不缓存由回退表产生的半成品，下次资源充足可重试。
    // Do not cache a partially derived fallback on OOM; retry when resources recover.
    if(soft_wave==display_main_gray_waveform())return &E0470_FULL_WAVEFORM;
    const EpdWaveformPhases *original=e0470_waveform_phases(&E0470_FULL_WAVEFORM,draw_mode);
    const EpdWaveformPhases *soft=e0470_waveform_phases(soft_wave,MODE_GL16);
    if(!original||!soft||!original->luts||!soft->luts||original->phase_times||soft->phase_times||original->phases>soft->phases)
        return &E0470_WAVEFORM;
    size_t size=(size_t)original->phases*16*4;
    uint8_t *data=heap_caps_malloc(size,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!data)return &E0470_WAVEFORM;
    memcpy(data,original->luts,size);
    for(int f=0;f<original->phases;++f)memcpy(data+f*16*4+15*4,soft->luts+f*16*4+15*4,4);
    // 中部白底保留本机补白；外侧题头/横条依然用等值保持，不闪动固定底栏。
    // Keep local white maintenance in art; equal-held outer chrome/marker avoids flashing fixed navigation.
    for(int f=0;f<original->phases;++f){
        int action=e0470_phase_action(e0470_waveform_phases(&E0470_FULL_WAVEFORM,MODE_GL16),f,15,15);
        data[(f*16+15)*4+3]=(uint8_t)((data[(f*16+15)*4+3]&~3u)|action);
    }
    phases[index]=(EpdWaveformPhases){original->phases,data,NULL};
    waves[index].temp_intervals=E0470_WAVEFORM.temp_intervals;
    return &waves[index];
}

// 整页补白与固定底栏/局部反馈分开；保持厂商表和相数。
// Separate page white maintenance from frozen navigation/local feedback; preserve vendor arrays and phase counts.
static uint8_t s_page_gray[E0470_GL16_FRAMES][16][4];
static const EpdWaveformPhases s_page_gray_phases={E0470_GL16_FRAMES,(const uint8_t*)s_page_gray,NULL};
static const EpdWaveformPhases *s_page_gray_ranges[]={&s_page_gray_phases};
static const EpdWaveformMode s_page_gray_mode={5,1,s_page_gray_ranges};
static const EpdWaveformMode *s_page_gray_modes[]={&s_page_gray_mode};
static EpdWaveform s_page_gray_waveform={.num_modes=1,.num_temp_ranges=1,.mode_data=s_page_gray_modes};
static bool s_page_gray_ready;
static uint8_t s_page_bw[E0470_FULL_DU_FRAMES][16][4];
static const EpdWaveformPhases s_page_bw_phases={E0470_FULL_DU_FRAMES,(const uint8_t*)s_page_bw,NULL};
static const EpdWaveformPhases *s_page_bw_ranges[]={&s_page_bw_phases};
static const EpdWaveformMode s_page_bw_mode={1,1,s_page_bw_ranges};
static const EpdWaveformMode *s_page_bw_modes[]={&s_page_bw_mode};
static EpdWaveform s_page_bw_waveform={.num_modes=1,.num_temp_ranges=1,.mode_data=s_page_bw_modes};
static bool s_page_bw_ready;
const EpdWaveform* display_main_page_gray_waveform(void) {
    if(!s_page_gray_ready) {
        const EpdWaveformPhases *original=e0470_waveform_phases(&E0470_WAVEFORM,MODE_GL16);
        if(!original||!original->luts||original->phases!=E0470_GL16_FRAMES||original->phase_times)
            return &E0470_WAVEFORM;
        memcpy(s_page_gray,original->luts,sizeof(s_page_gray));
        for(int f=0;f<E0470_GL16_FRAMES;++f)for(int v=0;v<15;++v)
            s_page_gray[f][v][v/4]&=(uint8_t)~(3u<<(6-2*(v%4)));
        s_page_gray_waveform.temp_intervals=E0470_WAVEFORM.temp_intervals;s_page_gray_ready=true;
    }
    return &s_page_gray_waveform;
}
const EpdWaveform* display_main_page_bw_waveform(void) {
    if(!s_page_bw_ready) {
        const EpdWaveformPhases *original=e0470_waveform_phases(&E0470_WAVEFORM,MODE_DU);
        if(!original||!original->luts||original->phases!=E0470_FULL_DU_FRAMES||original->phase_times)
            return &E0470_WAVEFORM;
        memcpy(s_page_bw,original->luts,sizeof(s_page_bw));
        for(int f=0;f<E0470_FULL_DU_FRAMES;++f)for(int v=0;v<15;++v)
            s_page_bw[f][v][v/4]&=(uint8_t)~(3u<<(6-2*(v%4)));
        // 白底 15→15 补一相擦白：全像素快刷时白底要重新驱动，否则强光下白底发灰。
        // Give unchanged white one erase tick so full-pixel fast pages re-drive the
        // white background instead of only holding it (strong light greys it out).
        e0470_waveform_white_tick(s_page_bw,E0470_FULL_DU_FRAMES);
        s_page_bw_waveform.temp_intervals=E0470_WAVEFORM.temp_intervals;s_page_bw_ready=true;
    }
    return &s_page_bw_waveform;
}
