/* SPDX-License-Identifier: Apache-2.0
 * 中文：阅读采用默认灰阶表相序（厂家 2.0.76 起完整 GL48），等值保持由派生表保证。
 * English: Reading uses the default gray-table order (full GL48 since vendor 2.0.76)
 *         with equal pairs held by the derived table.
 */
#include "display_reader_waveform.h"
#include "e0470_epaper_waveform.h"
#include <stddef.h>
const EpdWaveform* display_reader_text_waveform(void) { return &E0470_TEXTTURN_WAVEFORM; }

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

bool display_reader_fast_text_prepare(EpdiyHighlevelState* hl,EpdRect area) {
    if(!hl||!hl->front_fb||!hl->back_fb||hl->front_fb==hl->back_fb||
        area.width<=0||area.height<=0||!display_reader_fast_text_waveform())return false;
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
    // 只改变本次文字/页脚目标，保留区域外像素和实际旧灰阶供原厂迁移表使用。
    // Threshold only the text/footer target; preserve outside pixels and real old grays for the vendor transition table.
    for(int row=phys.y;row<phys.y+phys.height;++row) {
        uint8_t* p=hl->front_fb+(size_t)row*(pw/2);
        int col=phys.x,end=col+phys.width;
        if(col&1){uint8_t v=p[col/2];p[col++/2]=(uint8_t)((v&15)|((v&0x80)?0xf0:0));}
        for(;col+1<end;col+=2){uint8_t v=p[col/2];p[col/2]=(uint8_t)(((v&8)?15:0)|((v&0x80)?0xf0:0));}
        if(col<end){uint8_t v=p[col/2];p[col/2]=(uint8_t)((v&0xf0)|((v&8)?15:0));}
    }
    return true;
}
