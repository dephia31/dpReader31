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
#include "e0470_waveform_trim.h"
#include <stdlib.h>
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
// 条带引擎按GL16类型读取LUT，这里只包装原DU20相位指针，不生成新序列。
// The band engine looks up GL16 LUTs; wrap the original DU20 phase pointer without generating a sequence.
const EpdWaveform* display_main_bw_turn_waveform(void) {
    static EpdWaveformMode mode;
    static const EpdWaveformPhases* ranges[1];
    static const EpdWaveformMode* modes[]={&mode};
    static EpdWaveform wave;
    const EpdWaveform *bw=display_main_bw_waveform();
    ranges[0]=e0470_waveform_phases(bw,MODE_DU);
    if(!ranges[0])return bw;
    mode=(EpdWaveformMode){MODE_GL16,1,ranges};
    wave=*bw;wave.num_modes=1;wave.mode_data=modes;
    return &wave;
}

// 底栏恢复：端点保留整份DU20，浅灰从白用既有FOLLOW；旧非白先按原DU擦白再绘灰。
// Navigation restore: retain full DU20 endpoints; use existing FOLLOW from white, or original DU whitening before gray from nonwhite.
// 不使用灰阶表的黑复位，不量化底栏，不修改原表/扫描相周期；灰色光学值仍需实屏确认。
// Avoid gray-table black resets and navigation quantization; retain vendor tables/cadence, with optical tones subject to panel validation.
const EpdWaveform* display_main_navigation_waveform(void) {
    enum { PHASES=E0470_FULL_DU_FRAMES+E0470_FOLLOW_FRAMES };
    static uint8_t data[PHASES][16][4];
    static const EpdWaveformPhases phases={PHASES,(const uint8_t*)data,NULL};
    static const EpdWaveformPhases* ranges[]={&phases};
    static const EpdWaveformMode mode={MODE_GL16,1,ranges};
    static const EpdWaveformMode* modes[]={&mode};
    static EpdWaveform wave={.num_modes=1,.num_temp_ranges=1,.mode_data=modes};
    static bool ready;
    if(ready)return &wave;
    const EpdWaveformPhases *bw=e0470_waveform_phases(display_main_bw_waveform(),MODE_DU);
    const EpdWaveformPhases *follow=e0470_waveform_phases(&E0470_FOLLOW_WAVEFORM,MODE_DU);
    if(!bw||!follow||!bw->luts||!follow->luts||bw->phase_times||follow->phase_times||
       bw->phases!=E0470_FULL_DU_FRAMES||follow->phases!=E0470_FOLLOW_FRAMES)
        return display_main_shelf_soft_waveform();
    for(int f=0;f<PHASES;++f)for(int to=0;to<16;++to)for(int from=0;from<16;++from){
        int action=0;
        if(to!=from){
            if(to==0||to==15){
                if(f<bw->phases)action=e0470_phase_action(bw,f,to,from);
            }else if(from==15){
                if(f<follow->phases)action=e0470_phase_action(follow,f,to,15);
            }else if(f<bw->phases)action=e0470_phase_action(bw,f,15,from);
            else action=e0470_phase_action(follow,f-bw->phases,to,15);
        }
        data[f][to][from/4]|=(uint8_t)(action<<(6-2*(from%4)));
    }
    wave.temp_intervals=E0470_WAVEFORM.temp_intervals;ready=true;
    return &wave;
}

// 取消旧的额外四相擦白和中灰调序，使用同一份官方迁移与历史帧。
// Remove four extra whitening phases and gray reordering; share vendor transitions/history.
const EpdWaveform* display_main_shelf_exit_waveform(void) { return display_main_bw_waveform(); }
const EpdWaveform* display_main_water_waveform(void) { return display_main_gray_waveform(); }
// 普通主页与已验证的书架外框共用端点迁移；中灰保留完整 GL48，白底不先反向压黑。
// Ordinary roots share the tested shelf-chrome endpoints, retaining full GL48 midgrays without reverse-blackening white targets.
const EpdWaveform* display_main_normal_waveform(void) { return display_main_shelf_soft_waveform(); }

// 普通主页与书架外框共用；阅读、水波纹及厂家原表保持原样。
// Shared by ordinary roots and shelf chrome; retain reader/ripple paths and vendor LUTs.
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

// 书架与主页周期整理保留完整灰阶，留白只向白推动，不再先压黑。
// Shelf transitions and root cleanup retain calibrated grays; whitespace drives only white without reverse blackening.
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

// 仅面板实验借用官方11/5/0/3裁剪，保留灰阶尾和三相保持；原48相数组不动。
// Panel experiments alone use vendor11/5/0/3 trimming, retaining gray tails/three holds and untouched48-phase arrays.
const EpdWaveform* display_reader_panel37_waveform(void) {
#if KIIKO_S31_READER_PANEL37
    static bool attempted;
    static EpdWaveformPhases phases;
    static const EpdWaveformPhases *ranges[]={&phases};
    static const EpdWaveformMode mode={MODE_GL16,1,ranges};
    static const EpdWaveformMode *modes[]={&mode};
    static EpdWaveform wave={.num_modes=1,.num_temp_ranges=1,.mode_data=modes};
    if(phases.luts)return &wave;
    if(attempted)return NULL;
    attempted=true;
    const EpdWaveformPhases *source=e0470_waveform_phases(&E0470_FULL_WAVEFORM,MODE_GL16);
    if(!source||!source->luts||source->phases!=48||source->phase_times||
       E0470_FULL_WAVEFORM.num_temp_ranges!=1||!E0470_FULL_WAVEFORM.temp_intervals)return NULL;
    uint8_t (*data)[16][4]=heap_caps_malloc(48u*16u*4u,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!data)return NULL;
    const e0470_trim_t trim={.erase_max=11,.sat_cut=5,.white_sat_cut=0,.hold=3};
    int count=e0470_waveform_trim(source,&trim,data);
    if(count!=37){free(data);return NULL;}
    // 控件按真实旧灰阶差分，所有等值保持，绝不把官方白底补推带进拖动反馈。
    // Controls use actual old-gray differences; hold all equal pairs without importing white-background maintenance.
    for(int f=0;f<count;++f)for(int v=0;v<16;++v)
        data[f][v][v/4]&=(uint8_t)~(3u<<(6-2*(v%4)));
    phases=(EpdWaveformPhases){count,(const uint8_t*)data,NULL};
    wave.temp_intervals=E0470_FULL_WAVEFORM.temp_intervals;
    return &wave;
#else
    return NULL;
#endif
}
