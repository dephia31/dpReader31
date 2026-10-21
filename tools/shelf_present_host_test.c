/* SPDX-License-Identifier: Apache-2.0
 * 中文：验证真实同步分区引擎的逐相同时呈现、校准长度、底栏边界、OOM与失败基准。
 * English: Verify simultaneous region phases, calibration length, footer bounds, OOM and failure baselines in the real engine.
 */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "e0470_page_turn.h"
#include "e0470_epaper_waveform.h"

#define W 1216
#define H 684
#define BYTES (W*H/2)
static uint8_t front[BYTES], back[BYTES], old[BYTES], diff[W*H];
static bool dirty_lines[H];
static uint8_t dirty_columns[W/2];
static EpdiyHighlevelState hl={front,back,diff,dirty_lines,dirty_columns};
static const EpdWaveformPhases soft={.phases=37,.luts=(const uint8_t*)"soft"};
static const EpdWaveformPhases art={.phases=37,.luts=(const uint8_t*)"art"};
static const EpdWaveformPhases cleanup={.phases=36,.luts=(const uint8_t*)"cleanup"};
const EpdWaveform E0470_WAVEFORM={1}, E0470_APPLY_WAVEFORM={2};
static const EpdWaveform soft_wave={3};
static enum EpdRotation rotation=EPD_ROT_INVERTED_PORTRAIT;
static const uint8_t* const* mapped_luts;
static const int *mapped_starts,*mapped_ends;
static const int8_t* mapped_selectors;
static int mapped_count,scans,fail_at,differences,powerons;
static bool full_cleanup;
static EpdRect expected_area={0,0,H,1120},expected_art={0,192,H,800};
int water_test_alloc_fail;
int epd_width(void){return W;}
int epd_height(void){return H;}
enum EpdRotation epd_get_rotation(void){return rotation;}
EpdRect epd_full_screen(void){return (EpdRect){0,0,W,H};}
void epd_poweron(void){++powerons;}
int64_t esp_timer_get_time(void){return 0;}
void esp_rom_delay_us(uint32_t us){assert(!us);}
const EpdWaveformPhases* e0470_waveform_phases(const EpdWaveform* wave,int mode){
    if(wave==&soft_wave){assert(mode==MODE_GL16);return &soft;}
    assert(wave==&E0470_WAVEFORM);
    assert(mode==MODE_GL16||mode==MODE_GC16);
    return mode==MODE_GC16?&cleanup:&art;
}
void epd_build_1ppB_lut_1k(uint8_t* lut,const EpdWaveformPhases* phases,int phase){
    assert(phase>=0&&phase<phases->phases);
    assert(phases==&soft||phases==&art||phases==&cleanup);
    memset(lut,(phases==&soft?128:0)+phase+1,1024);
}
static int nibble(const uint8_t *fb,int x,int y){return (fb[y*(W/2)+x/2]>>((x&1)*4))&15;}
EpdRect epd_difference_image_cropped(const uint8_t* to,const uint8_t* from,EpdRect area,
    uint8_t* difference,bool* lines,uint8_t* columns){
    assert(to==front&&from==back&&difference==diff&&lines==dirty_lines&&columns==dirty_columns);
    assert(!memcmp(back,old,BYTES));
    assert(area.x==expected_area.y&&area.y==0&&area.width==expected_area.height&&area.height==H);
    ++differences;
    for(int y=0;y<H;++y)for(int x=area.x;x<area.x+area.width;++x)
        diff[y*W+x]=(uint8_t)((nibble(to,x,y)<<4)|nibble(from,x,y));
    return area;
}
void epd_clear_phase_luts(void){mapped_luts=NULL;mapped_selectors=NULL;mapped_count=0;}
void epd_set_line_phase_luts(const uint8_t* const* luts,const int8_t* lines){(void)luts;(void)lines;assert(false);}
void epd_set_col_phase_luts(const uint8_t* const* luts,const int* starts,const int* ends,
    const int8_t* selectors,int count){
    mapped_luts=luts;mapped_starts=starts;mapped_ends=ends;mapped_selectors=selectors;mapped_count=count;
}
enum EpdDrawError epd_draw_base(EpdRect area,const uint8_t* pixels,EpdRect crop,
    enum EpdDrawMode mode,int temperature,const bool* lines,const uint8_t* columns,
    const EpdWaveform* wave){
    assert(area.width==W&&area.height==H&&crop.width==W&&crop.height==H);
    assert(mode==(MODE_PACKING_1PPB_DIFFERENCE|MODE_DU)&&temperature==25&&wave==&E0470_APPLY_WAVEFORM);
    assert(pixels==diff&&lines==dirty_lines&&columns==dirty_columns);
    assert(mapped_luts&&mapped_count==3&&!memcmp(back,old,BYTES));
    assert(mapped_starts[0]==expected_area.y&&mapped_ends[0]==expected_art.y);
    assert(mapped_starts[1]==expected_art.y&&mapped_ends[1]==expected_art.y+expected_art.height);
    assert(mapped_starts[2]==mapped_ends[1]&&mapped_ends[2]==expected_area.y+expected_area.height);
    for(int y=0;y<H;++y)assert(lines[y]);
    for(int x=0;x<W;++x)assert(!!columns[x/2]==(x>=expected_area.y&&x<expected_area.y+expected_area.height));
    for(int y=0;y<H;y+=83){
        // 每一相均覆盖题头/中部/分页，输入必须来自同一真实前后帧。
        // Every phase covers header/art/pagination using the same real prior/target pair.
        const int points[]={0,1,expected_art.y-1,expected_art.y,expected_art.y+1,expected_art.y+expected_art.height-1,expected_art.y+expected_art.height,1097,1101,1119};
        for(unsigned i=0;i<sizeof(points)/sizeof(points[0]);++i){int x=points[i];assert(diff[y*W+x]==((nibble(front,x,y)<<4)|nibble(old,x,y)));}
    }
    for(int band=0;band<3;++band){
        assert(mapped_selectors[band]==(band==1?1:0));
        int expected=band==1?((!full_cleanup||scans<36)?scans+1:0):129+scans;
        for(int i=0;i<1024;++i)assert(mapped_luts[mapped_selectors[band]][i]==expected);
    }
    ++scans;
    return fail_at==scans?EPD_DRAW_OTHER_ERROR:EPD_DRAW_SUCCESS;
}
static void reset(void){
    scans=differences=powerons=fail_at=0;water_test_alloc_fail=0;
    epd_clear_phase_luts();rotation=EPD_ROT_INVERTED_PORTRAIT;
    for(int i=0;i<BYTES;++i){old[i]=back[i]=(uint8_t)(i*13);front[i]=(uint8_t)(i*17+5);}
}
static enum EpdDrawError run(void){
    return e0470_update_regions(&hl,expected_area,expected_art,&soft_wave,&E0470_WAVEFORM,full_cleanup?MODE_GC16:MODE_GL16);
}
int main(void){
    for(int layout=0;layout<2;++layout)for(int gc=0;gc<2;++gc)for(int fail=0;fail<=37;++fail){
        expected_art=layout?(EpdRect){0,160,H,928}:(EpdRect){0,192,H,800};
        reset();full_cleanup=gc;fail_at=fail;
        assert(run()==(fail?EPD_DRAW_OTHER_ERROR:EPD_DRAW_SUCCESS));
        assert(scans==(fail?fail:37)&&differences==1&&powerons==1&&!mapped_luts);
        for(int y=0;y<H;++y)for(int x=0;x<W;++x){
            bool committed=!fail&&x>=expected_area.y&&x<expected_area.y+expected_area.height;
            assert(nibble(back,x,y)==nibble(committed?front:old,x,y));
        }
    }
    reset();water_test_alloc_fail=1;assert(run()==EPD_DRAW_NO_PHASES_AVAILABLE);
    assert(!scans&&!differences&&!powerons&&!memcmp(old,back,BYTES));
    const EpdRect invalid[]={{0,0,H,1119},{1,0,H,1120},{0,-32,H,1120},{0,0,H,2147483647},{0,1184,H,64}};
    for(unsigned i=0;i<sizeof(invalid)/sizeof(invalid[0]);++i){
        reset();EpdRect valid=expected_area;expected_area=invalid[i];
        assert(run()==EPD_DRAW_INVALID_CROP);expected_area=valid;
        assert(!scans&&!differences&&!powerons);
    }
    const EpdRect invalid_art[]={{0,193,H,800},{0,192,H,801},{0,992,H,160},{1,192,H,800},{0,192,H,2147483647}};
    for(unsigned i=0;i<sizeof(invalid_art)/sizeof(invalid_art[0]);++i){
        reset();EpdRect valid=expected_art;expected_art=invalid_art[i];
        assert(run()==EPD_DRAW_INVALID_CROP);expected_art=valid;
        assert(!scans&&!differences&&!powerons);
    }
    for(int rot=0;rot<3;++rot){reset();rotation=rot;assert(run()==EPD_DRAW_INVALID_CROP);assert(!scans);}
    puts("simultaneous shelf/settings: shared chrome/art phases, original 37/36 lengths, footer, all phase failures, OOM and geometry passed");
    return 0;
}
