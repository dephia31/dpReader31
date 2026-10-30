/* SPDX-License-Identifier: Apache-2.0
 * 中文：用户要求避免频繁全刷；AA正文对字迹像素复用原GC48，等白保持现有GL补白。
 * English: AA text trial rebuilds ink pixels with native GC48, retaining the
 *         existing GL white maintenance for unchanged white (no black background flash).
 * Revision2026-10-30: reader AA overrides Fast BW on subsequent turns, matching
 * initial rendering; only an explicit AA-off choice permits target binarization.
 */
#include "display_reader_waveform.h"
#include "e0470_epaper_waveform.h"
#include "settings.h"
#include <stddef.h>
#ifdef ESP_PLATFORM
#include "esp_attr.h"
#endif
#if KIIKO_S31_FAST_READER_PANELS
#ifdef ESP_PLATFORM
#include "esp_attr.h"
#define PANEL_LUT_ATTR EXT_RAM_BSS_ATTR
#else
#define PANEL_LUT_ATTR
#endif
#endif
const EpdWaveform* display_reader_text_waveform(void) {
    if(!app_settings_book_antialias())return &E0470_TEXTTURN_WAVEFORM;
    /* User revision: avoid frequent global clears. Rebuild only ink footprints
     * with the original GC sequence; unchanged white keeps existing GL pulses.
     * No phase trimming, fabricated history or additional refresh pass. */
#ifdef ESP_PLATFORM
    static EXT_RAM_BSS_ATTR uint8_t pixels[E0470_FULL_GC16_FRAMES][16][4];
#else
    static uint8_t pixels[E0470_FULL_GC16_FRAMES][16][4];
#endif
    static bool ready;
    static EpdWaveformPhases phase;
    static const EpdWaveformPhases* ranges[1];
    static EpdWaveformMode mode;
    static const EpdWaveformMode* modes[1];
    static EpdWaveform waveform;
    if(!ready) {
        const EpdWaveformPhases *gc=e0470_waveform_phases(&E0470_FULL_WAVEFORM,MODE_GC16);
        const EpdWaveformPhases *gl=e0470_waveform_phases(&E0470_TEXTTURN_WAVEFORM,MODE_GL16);
        if(!gc||!gl||!gc->luts||!gl->luts||gc->phase_times||gl->phase_times||
           gc->phases!=E0470_FULL_GC16_FRAMES||gl->phases!=gc->phases)return &E0470_TEXTTURN_WAVEFORM;
        for(int f=0;f<gc->phases;++f)for(int to=0;to<16;++to)for(int group=0;group<4;++group)
            pixels[f][to][group]=gc->luts[f*64+to*4+group];
        for(int f=0;f<gc->phases;++f)
            pixels[f][15][3]=(uint8_t)((pixels[f][15][3]&0xfc)|(gl->luts[f*64+63]&3));
        phase=(EpdWaveformPhases){.phases=gc->phases,.luts=(const uint8_t*)pixels,.phase_times=NULL};
        ranges[0]=&phase;
        mode=(EpdWaveformMode){.type=MODE_GL16,.temp_ranges=1,.range_data=ranges};
        modes[0]=&mode;waveform=E0470_TEXTTURN_WAVEFORM;waveform.mode_data=modes;
        ready=true;
    }
    return &waveform;
}

const EpdWaveform* display_reader_fast_text_waveform(void) {
    static const EpdWaveformPhases* ranges[1];
    static const EpdWaveformMode mode={.type=MODE_GL16,.temp_ranges=1,.range_data=ranges};
    static const EpdWaveformMode* modes[]={&mode};
    static EpdWaveform waveform;
    const EpdWaveformPhases* original=e0470_waveform_phases(&E0470_FULL_WAVEFORM,MODE_DU);
    if(!original||!original->luts||original->phase_times||original->phases!=20)return NULL;
    // 仅为现有整轮提交接口提供模式别名，逐字节复用原厂DU20，不生成或裁剪脉冲。
    // Alias the mode for the batch API; reuse the exact vendor DU20 bytes without generating or trimming pulses.
    ranges[0]=original;waveform=E0470_FULL_WAVEFORM;
    waveform.num_modes=1;waveform.mode_data=modes;return &waveform;
}

static inline uint8_t prepare_ui_byte(uint8_t value) {
    static const uint8_t tones[16]={0,0,0,5,5,5,5,5,10,10,10,10,10,10,10,15};
    return (uint8_t)(tones[value&15]|(tones[value>>4]<<4));
}
static bool prepare_target(uint8_t* target,EpdRect area,bool four_gray) {
    if(!target||area.width<=0||area.height<=0)return false;
    const int pw=epd_width(),ph=epd_height();
    if(pw<=0||ph<=0||(pw&1))return false;
    enum EpdRotation rotation=epd_get_rotation();
    bool portrait=rotation==EPD_ROT_PORTRAIT||rotation==EPD_ROT_INVERTED_PORTRAIT;
    int lw=portrait?ph:pw,lh=portrait?pw:ph;
    int64_t right=(int64_t)area.x+area.width,bottom=(int64_t)area.y+area.height;
    int x=area.x<0?0:area.x,y=area.y<0?0:area.y;
    if(right>lw)right=lw;
    if(bottom>lh)bottom=lh;
    if(right<=x||bottom<=y)return false;
    int w=(int)right-x,h=(int)bottom-y;EpdRect phys;
    switch(rotation) {
        case EPD_ROT_LANDSCAPE:phys=(EpdRect){x,y,w,h};break;
        case EPD_ROT_PORTRAIT:phys=(EpdRect){pw-y-h,x,h,w};break;
        case EPD_ROT_INVERTED_LANDSCAPE:phys=(EpdRect){pw-x-w,ph-y-h,w,h};break;
        case EPD_ROT_INVERTED_PORTRAIT:phys=(EpdRect){y,ph-x-w,h,w};break;
        default:return false;
    }
    // 只改目标矩形；UI的13/14底色留浅灰，不能沿用文字映射把它们吞成白色。
    // Change only the target crop; retain UI13/14 fills as light gray instead of text-style white collapse.
    for(int row=phys.y;row<phys.y+phys.height;++row) {
        uint8_t* p=target+(size_t)row*(pw/2);
        int col=phys.x,end=col+phys.width;
        if(col&1){uint8_t v=p[col/2];p[col++/2]=(uint8_t)((v&15)|(four_gray?(prepare_ui_byte(v)&0xf0):((v&0x80)?0xf0:0)));}
        for(;col+1<end;col+=2){uint8_t v=p[col/2];p[col/2]=four_gray?prepare_ui_byte(v):(uint8_t)(((v&8)?15:0)|((v&0x80)?0xf0:0));}
        if(col<end){uint8_t v=p[col/2];p[col/2]=(uint8_t)((v&0xf0)|(four_gray?(prepare_ui_byte(v)&15):((v&8)?15:0)));}
    }
    return true;
}

bool display_reader_fast_text_prepare(EpdiyHighlevelState* hl,EpdRect area) {
    // A fast waveform cannot reproduce gray glyph edges. Leave both buffers
    // untouched so the caller retains its native grayscale waveform.
    if(app_settings_book_antialias())return false;
    if(!hl||!hl->front_fb||!hl->back_fb||hl->front_fb==hl->back_fb||!display_reader_fast_text_waveform())return false;
    return prepare_target(hl->front_fb,area,false);
}
bool display_reader_panel_gray4_prepare(uint8_t* target,EpdRect area) {
    return prepare_target(target,area,true);
}
bool display_reader_fast_panels_enabled(void) {
#if KIIKO_S31_FAST_READER_PANELS
    return true;
#else
    return false;
#endif
}
const EpdWaveform* display_reader_panel_gray4_waveform(bool erase_underlay) {
#if KIIKO_S31_FAST_READER_PANELS
    enum{FAST_PHASES=16,ERASE_WHITE=18,ENTRY_PHASES=26};
    static PANEL_LUT_ATTR uint8_t fast[FAST_PHASES][16][4],entry[ENTRY_PHASES][16][4];
    static const EpdWaveformPhases phases[]={
        {FAST_PHASES,(const uint8_t*)fast,NULL},{ENTRY_PHASES,(const uint8_t*)entry,NULL}};
    static const EpdWaveformPhases *ranges[][1]={{&phases[0]},{&phases[1]}};
    static const EpdWaveformMode mode[]={{MODE_GL16,1,ranges[0]},{MODE_GL16,1,ranges[1]}};
    static const EpdWaveformMode *modes[][1]={{&mode[0]},{&mode[1]}};
    static EpdWaveform wave[]={
        {.num_modes=1,.num_temp_ranges=1,.mode_data=modes[0]},
        {.num_modes=1,.num_temp_ranges=1,.mode_data=modes[1]}};
    static bool ready;
    if(!ready){
        const EpdWaveformPhases *follow=e0470_waveform_phases(&E0470_FOLLOW_WAVEFORM,MODE_DU);
        const EpdWaveformPhases *native=e0470_waveform_phases(&E0470_FULL_WAVEFORM,MODE_DU);
        if(!follow||!follow->luts||follow->phase_times||follow->phases!=8)return NULL;
        if(!native||!native->luts||native->phase_times||native->phases!=20)return NULL;
        for(int f=0;f<20;++f)
            if(e0470_phase_action(native,f,15,0)!=(f<ERASE_WHITE?2:0))return NULL;
        int black=0,white=0,dark=0,light=0;
        for(int f=0;f<8;++f){
            black+=e0470_phase_action(follow,f,0,15)==1;
            white+=e0470_phase_action(follow,f,15,0)==2;
            dark+=e0470_phase_action(follow,f,5,15)==1;
            light+=e0470_phase_action(follow,f,10,15)==1;
        }
        if(black!=7||white!=8||dark!=5||light!=3)return NULL;
        // 初次进入把每个中灰目标统一擦白后再推灰，包含等灰旧字和白起点；同面板反馈仍16相。
        // Entry resets every midgray target, including equal-gray glyphs and white origins, before FOLLOW gray pushes; feedback stays16 phases.
        // 一轮最多18白+5黑+3保持，不加前置清屏或白到白维护，也不裁原厂表。
        // One scan contains at most18 white+5 black+3 holds, without a preclear, unchanged-white maintenance or raw-table trimming.
        for(int profile=0;profile<2;++profile){
            uint8_t (*data)[16][4]=profile?entry:fast;
            const int reset=profile?ERASE_WHITE:8;
            for(int from=0;from<16;++from)for(int to=0;to<=15;to+=5){
                bool gray=to==5||to==10;
                if(from==to&&!(profile&&gray))continue;
                int offset=gray&&(profile||from!=15)?reset:0;
                if(offset||to==15)for(int f=0;f<reset;++f){
                    int a=e0470_phase_action(profile?native:follow,f,15,0);
                    data[f][to][from/4]|=(uint8_t)(a<<(6-2*(from%4)));
                }
                if(to!=15)for(int f=0;f<8;++f){
                    int a=e0470_phase_action(follow,f,to,15);
                    data[offset+f][to][from/4]|=(uint8_t)(a<<(6-2*(from%4)));
                }
            }
        }
        wave[0].temp_intervals=wave[1].temp_intervals=E0470_FULL_WAVEFORM.temp_intervals;ready=true;
    }
    return &wave[erase_underlay?1:0];
#else
    (void)erase_underlay;
    return NULL;
#endif
}
