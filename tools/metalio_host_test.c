/* 中文：生产代码回归：完整波形像素顺序、白边与 FT6336U 报告。
 * English: Production regressions for waveform pixel order, borders and FT6336U reports. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "epdiy.h"
#include "e0470_epaper_waveform.h"
#include "metalio_touch_decode.h"
extern bool epd_pixel_order_selftest(const EpdWaveform *waveform);
static void point(uint8_t *r,unsigned i,unsigned x,unsigned y,unsigned id,unsigned event) {
    uint8_t *p=r+3+6*i;
    p[0]=(event<<6)|(x>>8); p[1]=x;
    p[2]=(id<<4)|(y>>8); p[3]=y;
}
extern uint32_t epd_interlace_4bpp_line_VE(const uint8_t*,const uint8_t*,uint8_t*,uint8_t*,int);
static void check_swar(void) {
    const int widths[]={1,7,8,9,16,21};
    uint8_t to[16],from[16],actual[32],expected[32],dirty[16],want_dirty[16];
    for(unsigned a=0;a<256;++a)for(unsigned b=0;b<256;++b)for(unsigned w=0;w<6;++w) {
        int width=widths[w];
        for(int i=0;i<16;++i){to[i]=(uint8_t)(a+i*13);from[i]=(uint8_t)(b-i*7);}
        memset(actual,0xcc,sizeof(actual));memset(expected,0xcc,sizeof(expected));
        memset(dirty,0x90,sizeof(dirty));memset(want_dirty,0x90,sizeof(want_dirty));
        bool changed=false;
        for(int x=0;x<width;++x){int shift=(x&1)*4;
            unsigned t=(to[x/2]>>shift)&15,f=(from[x/2]>>shift)&15;
            expected[x]=(uint8_t)((t<<4)|f);want_dirty[x/2]|=(uint8_t)((t^f)<<shift);changed|=t!=f;}
        assert(!!epd_interlace_4bpp_line_VE(to,from,actual,dirty,width)==changed);
        assert(!memcmp(actual,expected,sizeof(actual))&&!memcmp(dirty,want_dirty,sizeof(dirty)));
    }
}
int main(void) {
    check_swar();
    e0470_waveform_init();
    assert(e0470_waveform_phases(&E0470_WAVEFORM,MODE_GC16)->phases==E0470_GC16_FRAMES);
    assert(e0470_waveform_phases(&E0470_WAVEFORM,MODE_GL16)->phases==E0470_GL16_FRAMES);
    assert(e0470_waveform_phases(&E0470_WAVEFORM,MODE_DU)->phases==20);
    // 滑块严格黑白时，应用DU20的每相必须等于未修改的原厂DU表。
    // For binary sliders, every application DU20 phase equals the unmodified vendor table.
    const EpdWaveformPhases *du=e0470_waveform_phases(&E0470_WAVEFORM,MODE_DU);
    const EpdWaveformPhases *native=e0470_waveform_phases(&E0470_FULL_WAVEFORM,MODE_DU);
    assert(native->phases==20);
    for(int phase=0;phase<20;phase++)for(int to=0;to<=15;to+=15)for(int from=0;from<16;from++)
        if(to!=15||from!=15)assert(e0470_phase_action(du,phase,to,from)==e0470_phase_action(native,phase,to,from));
        else assert(e0470_phase_action(du,phase,to,from)==0||e0470_phase_action(du,phase,to,from)==2);
    assert(epd_pixel_order_selftest(&E0470_WAVEFORM));
    assert(epd_pixel_order_selftest(&E0470_FULL_WAVEFORM));
    assert(epd_pixel_order_selftest(&E0470_TEXTTURN_WAVEFORM));
    assert(epd_pixel_order_selftest(&E0470_GRAY8_WAVEFORM));
    uint8_t raw[15]={0}; metalio_touch_point_t p[2]; unsigned count;
    assert(metalio_touch_decode(raw,sizeof(raw),p,&count)&&count==0);
    raw[2]=1; point(raw,0,341,608,3,0);
    assert(metalio_touch_decode(raw,sizeof(raw),p,&count)&&count==1);
    assert(p[0].x==341 && p[0].y==608 && p[0].id==3 && p[0].event==0);
    point(raw,0,1100,200,0,2);
    assert(metalio_touch_decode(raw,sizeof(raw),p,&count)&&count==1 && p[0].x==683 && p[0].y==200);
    // Right edge overshoot must never change orientation, and the bottom row remains reachable.
    for(unsigned y=10;y<=1225;y+=27) {
        point(raw,0,684,y,0,2);
        assert(metalio_touch_decode(raw,sizeof(raw),p,&count)&&count==1);
        assert(p[0].x==683&&p[0].y==(y>1215?1215:y));
    }
    point(raw,0,0,1225,0,0);
    assert(metalio_touch_decode(raw,sizeof(raw),p,&count)&&p[0].x==0&&p[0].y==1215);
    point(raw,0,200,1300,0,0);
    assert(metalio_touch_decode(raw,sizeof(raw),p,&count)&&count==0);
    for(unsigned key=100;key<=300;key+=100) {
        point(raw,0,key,1600,0,0);
        assert(metalio_touch_decode(raw,sizeof(raw),p,&count)&&count==1 && p[0].x==key && p[0].y==1600);
    }
    point(raw,0,0,0,0,0);
    assert(metalio_touch_decode(raw,sizeof(raw),p,&count)&&p[0].x==0&&p[0].y==0);
    point(raw,0,683,1215,0,0);
    assert(metalio_touch_decode(raw,sizeof(raw),p,&count)&&p[0].x==683&&p[0].y==1215);
    point(raw,0,341,608,0,1);
    assert(metalio_touch_decode(raw,sizeof(raw),p,&count)&&count==0);
    raw[2]=2; point(raw,0,10,30,1,0); point(raw,1,600,1100,2,2);
    assert(metalio_touch_decode(raw,sizeof(raw),p,&count)&&count==2&&p[1].id==2&&p[1].y==1100);
    assert(!metalio_touch_decode(raw,14,p,&count));
    raw[2]=3; assert(!metalio_touch_decode(raw,sizeof(raw),p,&count));
    assert(!metalio_touch_decode(NULL,sizeof(raw),p,&count));
    puts("Metalio: all gray transitions and phases; pixel subsets/masks; white-border hold/whiten; FT6336U fixed portrait/edge clamp/cover keys/multitouch/truncated reports passed");
    return 0;
}
