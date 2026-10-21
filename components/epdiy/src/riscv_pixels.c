/**
 * @brief RISC-V：原 Xtensa SIMD（diff.S / lut.S）的等价 C 实现（收紧热路径）
 */
#include <esp_attr.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#if defined(CONFIG_IDF_TARGET_ESP32S31)
#include "esp_cpu.h"
#include "pie_pixels.h"
#ifdef KIIKO_LOCAL_LATENCY
static bool s_pie_enabled;
void epd_pie_pixels_enable(bool enabled) { __atomic_store_n(&s_pie_enabled,enabled,__ATOMIC_RELAXED); }
bool epd_pie_pixels_enabled(void) { return __atomic_load_n(&s_pie_enabled,__ATOMIC_RELAXED); }
#else
#ifdef KIIKO_S31_PIE_PIXELS
static const bool s_pie_enabled=true;
#else
static const bool s_pie_enabled=false;
#endif
#endif
static inline bool pie_core1(void) {
#ifdef KIIKO_LOCAL_LATENCY
    return epd_pie_pixels_enabled()&&esp_cpu_get_core_id()==1;
#else
    return s_pie_enabled&&esp_cpu_get_core_id()==1;
#endif
}
#endif

/**
 * @brief 将 to/from 两行 4bpp 交织为差分行，并更新列脏标记
 * @param to 目标帧一行（2 像素/字节）
 * @param from 当前帧一行（2 像素/字节）
 * @param interlaced 输出差分（1 像素/字节）
 * @param col_dirtyness 列脏字节（与 to 同行宽）
 * @param fb_width 像素宽度
 * @return 非 0 表示本行有差异
 */
static inline uint32_t expand_nibbles(uint32_t value) {
    value=(value|(value<<8))&UINT32_C(0x00ff00ff);
    return (value|(value<<4))&UINT32_C(0x0f0f0f0f);
}
__attribute__((optimize("O3"))) uint32_t IRAM_ATTR epd_interlace_4bpp_line_c(
    const uint8_t* to,
    const uint8_t* from,
    uint8_t* interlaced,
    uint8_t* col_dirtyness,
    int fb_width
) {
    uint32_t dirty = 0;
    if (to == NULL || from == NULL || interlaced == NULL || col_dirtyness == NULL || fb_width <= 0) {
        return 0;
    }
    // 两像素一字节：成对处理，少一次 x/2、x%2
    const int pairs = fb_width >> 1;
    int p=0;
    // S31 小端 32 位并行交织八像素；输出/脏标记逐字节等价，减少前台差分等待。
    // Interlace eight pixels per little-endian word; output/dirty masks remain byte-exact.
    for(;p+4<=pairs;p+=4) {
        uint32_t target,previous,columns;
        memcpy(&target,to+p,4);memcpy(&previous,from+p,4);
        uint32_t changed=target^previous;dirty|=changed;
        memcpy(&columns,col_dirtyness+p,4);columns|=changed;memcpy(col_dirtyness+p,&columns,4);
        uint32_t low=(expand_nibbles(target&0xffffu)<<4)|expand_nibbles(previous&0xffffu);
        uint32_t high=(expand_nibbles(target>>16)<<4)|expand_nibbles(previous>>16);
        memcpy(interlaced+2*p,&low,4);memcpy(interlaced+2*p+4,&high,4);
    }
    for (; p < pairs; p++) {
        const uint8_t tb = to[p];
        const uint8_t fb = from[p];
        const uint8_t t0 = (uint8_t)(tb & 0x0f);
        const uint8_t t1 = (uint8_t)(tb >> 4);
        const uint8_t f0 = (uint8_t)(fb & 0x0f);
        const uint8_t f1 = (uint8_t)(fb >> 4);
        const uint8_t d0 = (uint8_t)(t0 ^ f0);
        const uint8_t d1 = (uint8_t)(t1 ^ f1);
        col_dirtyness[p] |= (uint8_t)(d0 | (d1 << 4));
        dirty |= (uint32_t)(d0 | d1);
        interlaced[p * 2] = (uint8_t)((t0 << 4) | f0);
        interlaced[p * 2 + 1] = (uint8_t)((t1 << 4) | f1);
    }
    if (fb_width & 1) {
        const int x = fb_width - 1;
        uint8_t t = to[x / 2];
        uint8_t f = from[x / 2];
        t = (uint8_t)((x % 2) ? (t >> 4) : (t & 0x0f));
        f = (uint8_t)((x % 2) ? (f >> 4) : (f & 0x0f));
        col_dirtyness[x / 2] |= (uint8_t)((t ^ f) << (4 * (x % 2)));
        dirty |= (uint32_t)(t ^ f);
        interlaced[x] = (uint8_t)((t << 4) | f);
    }
    return dirty;
}

__attribute__((optimize("O3"))) void IRAM_ATTR
epd_apply_line_mask_c(uint8_t* line, const uint8_t* mask, int mask_len) {
    if (line == NULL || mask == NULL || mask_len <= 0) {
        return;
    }
    // 非对齐调用用逐字节回退；生产队列和mask均16B对齐。
    // Unaligned callers fall back bytewise; production queues/masks align to16B.
    if(((uintptr_t)line|(uintptr_t)mask)&3u) {
        for(int i=0;i<mask_len;++i)line[i]&=mask[i];
        return;
    }
    uint32_t* l32 = (uint32_t*)line;
    const uint32_t* m32 = (const uint32_t*)mask;
    const int words = mask_len >> 2;
    int i = 0;
    for (; i + 4 <= words; i += 4) {
        l32[i] &= m32[i];
        l32[i + 1] &= m32[i + 1];
        l32[i + 2] &= m32[i + 2];
        l32[i + 3] &= m32[i + 3];
    }
    for (; i < words; i++) {
        l32[i] &= m32[i];
    }
    for (i = words << 2; i < mask_len; i++) {
        line[i] &= mask[i];
    }
}

__attribute__((optimize("O3"))) uint32_t IRAM_ATTR epd_interlace_4bpp_line_VE(
    const uint8_t* to,const uint8_t* from,uint8_t* output,uint8_t* columns,int pixels
) {
#if defined(CONFIG_IDF_TARGET_ESP32S31)
    // 不强迁移Core0前台；对齐Core1任务才使用PIE，剩余像素保持C语义。
    // Never migrate the Core0 foreground; only aligned Core1 tasks use PIE, with C tails.
    if(pixels>=32&&to&&from&&output&&columns&&
       !(((uintptr_t)to|(uintptr_t)from|(uintptr_t)output|(uintptr_t)columns)&15u)&&pie_core1()) {
        unsigned blocks=(unsigned)pixels/32;
        uint32_t dirty=epd_pie_interlace_blocks(to,from,output,columns,blocks);
        int bytes=(int)blocks*16;
        return dirty|epd_interlace_4bpp_line_c(to+bytes,from+bytes,output+2*bytes,columns+bytes,pixels-2*bytes);
    }
#endif
    return epd_interlace_4bpp_line_c(to,from,output,columns,pixels);
}

__attribute__((optimize("O3"))) void IRAM_ATTR epd_apply_line_mask_VE(uint8_t* line,const uint8_t* mask,int length) {
#if defined(CONFIG_IDF_TARGET_ESP32S31)
    if(length>=16&&line&&mask&&!(((uintptr_t)line|(uintptr_t)mask)&15u)&&pie_core1()) {
        unsigned blocks=(unsigned)length/16;
        epd_pie_mask_blocks(line,mask,blocks);
        int bytes=(int)blocks*16;
        epd_apply_line_mask_c(line+bytes,mask+bytes,length-bytes);
        return;
    }
#endif
    epd_apply_line_mask_c(line,mask,length);
}

/**
 * @brief 1ppB / 1K VE 格式 LUT 查表（与 lut.c lookup_pixels_in_VE_LUT 一致）
 * @note S31 与 S3 字节内 4px 高低位对调：板级/LCD 沿 X 先吃高位 2bit，非 CPU 端序差异。
 *       VE LUT 每项占 4 字节，值在 LE 低字节；按字节取比整字 load 更省带宽。
 */
__attribute__((optimize("O3"))) void IRAM_ATTR
calc_epd_input_1ppB_1k_S3_VE_aligned(
    const uint32_t* ld, uint8_t* epd_input, const uint8_t* conversion_lut, uint32_t epd_width
) {
    if (ld == NULL || epd_input == NULL || conversion_lut == NULL || epd_width < 4) {
        return;
    }

    const uint8_t* lut = conversion_lut;
    const uint32_t n = epd_width >> 2; // 输出字节数 = 像素/4
    uint32_t j = 0;

#if defined(CONFIG_IDF_TARGET_ESP32S31) && defined(ESP_PLATFORM)
    extern bool epd_pipeline_enabled(void);
    if(epd_pipeline_enabled()) {
#define READY4(v) (lut[((v)&255u)*4+3]|lut[(((v)>>8)&255u)*4+2]|lut[(((v)>>16)&255u)*4+1]|lut[((v)>>24)*4])
        for(;j+4<=n;j+=4) {
            const uint32_t a=ld[j],b=ld[j+1],c=ld[j+2],d=ld[j+3];
            epd_input[j]=READY4(a);epd_input[j+1]=READY4(b);
            epd_input[j+2]=READY4(c);epd_input[j+3]=READY4(d);
        }
        for(;j<n;++j)epd_input[j]=READY4(ld[j]);
#undef READY4
        return;
    }
#endif

#if defined(CONFIG_IDF_TARGET_ESP32S31)
#define LUT4(in)                                                                                   \
    ((uint8_t)(lut[((in) & 0xFFu) << 2] << 6 | lut[(((in) >> 8) & 0xFFu) << 2] << 4                \
               | lut[(((in) >> 16) & 0xFFu) << 2] << 2 | lut[((in) >> 24) << 2]))
#else
#define LUT4(in)                                                                                   \
    ((uint8_t)(lut[((in) >> 24) << 2] << 6 | lut[(((in) >> 16) & 0xFFu) << 2] << 4                 \
               | lut[(((in) >> 8) & 0xFFu) << 2] << 2 | lut[((in) & 0xFFu) << 2]))
#endif

    for (; j + 4 <= n; j += 4) {
        const uint32_t in0 = ld[j];
        const uint32_t in1 = ld[j + 1];
        const uint32_t in2 = ld[j + 2];
        const uint32_t in3 = ld[j + 3];
        epd_input[j] = LUT4(in0);
        epd_input[j + 1] = LUT4(in1);
        epd_input[j + 2] = LUT4(in2);
        epd_input[j + 3] = LUT4(in3);
    }
    for (; j < n; j++) {
        epd_input[j] = LUT4(ld[j]);
    }
#undef LUT4
}

#if defined(CONFIG_IDF_TARGET_ESP32S31)
#include "output_common/lut.h"
#include "output_common/white_border.h"
void epd_populate_line_mask(uint8_t*, const uint8_t*, int);

// Exercise the actual difference, LUT, mask and mask-application routines.
// All 16 four-pixel subsets, in both black/white directions, must preserve
// exactly the selected output pixels. This catches a LUT/mask order mismatch.
bool epd_pixel_order_selftest(const EpdWaveform *waveform) {
    if (!waveform) return false;
    uint8_t border_line[8];
    for (int whitening=0;whitening<2;++whitening) {
    for (int action=0;action<4;++action) {
        for (int y=0;y<32;++y) {
            memset(border_line,action*0x55,sizeof(border_line));
            epd_filter_white_border(border_line,y,8,32,8,whitening);
            for (int x=0;x<32;++x) {
                const bool edge=x<8 || x>=24 || y<8 || y>=24;
                const int expected=edge ? (whitening ? action&2 : 0) : action;
                if (((border_line[x/4]>>(6-2*(x&3)))&3)!=expected) return false;
            }
        }
    }
    }
    uint32_t lut[256] __attribute__((aligned(16))) = {0};
    uint8_t to[8] __attribute__((aligned(16)));
    uint8_t from[8] __attribute__((aligned(16)));
    uint8_t difference[16] __attribute__((aligned(16)));
    uint8_t dirty[8] __attribute__((aligned(16)));
    uint8_t output[4] __attribute__((aligned(16)));
    uint8_t mask[4] __attribute__((aligned(16)));
    lut[0x0f]=UINT32_C(0x40100401); lut[0xf0]=UINT32_C(0x80200802);
    for (int direction=0;direction<2;++direction) {
        for (int subset=0;subset<16;++subset) {
            memset(from,direction?0:255,sizeof(from));
            memcpy(to,from,sizeof(to)); memset(dirty,0,sizeof(dirty));
            for (int p=0;p<16;++p) {
                if (!(subset&(1<<(p&3)))) continue;
                const int color=direction?15:0;
                to[p/2]=(p&1)?(to[p/2]&15)|(color<<4):(to[p/2]&240)|color;
            }
            epd_interlace_4bpp_line_VE(to,from,difference,dirty,16);
            calc_epd_input_1ppB_1k_S3_VE_aligned((uint32_t*)difference,output,(uint8_t*)lut,16);
            epd_populate_line_mask(mask,dirty,4);
            epd_apply_line_mask_VE(output,mask,4);
            for (int p=0;p<16;++p) {
                const int expected=(subset&(1<<(p&3)))?direction+1:0;
                if (((output[p/4]>>(6-2*(p&3)))&3)!=expected) return false;
                const int expected_mask=(subset&(1<<(p&3)))?3:0;
                if (((mask[p/4]>>(6-2*(p&3)))&3)!=expected_mask) return false;
            }
        }
    }
    // Check the real assembled GC16/GL16 tables through the same difference
    // and LUT path used for display. Every old/target gray pair in all phases
    // must emit the exact two-bit waveform action, independently of optics.
    const LutFunctionPair pair=find_lut_functions(MODE_PACKING_1PPB_DIFFERENCE,1024);
    if (!pair.build_func || !pair.lookup_func) return false;
    for (int m=0;m<waveform->num_modes;++m) {
        const EpdWaveformMode* mode=waveform->mode_data[m];
        if (mode->type!=MODE_GC16 && mode->type!=MODE_GL16) continue;
        const EpdWaveformPhases* phases=mode->range_data[0];
        for (int f=0;f<phases->phases;++f) {
            pair.build_func((uint8_t*)lut,phases,f);
            for (int old=0;old<16;++old) {
                memset(from,old|(old<<4),sizeof(from));
                memset(dirty,0,sizeof(dirty));
                for (int p=0;p<8;++p) to[p]=(2*p)|((2*p+1)<<4);
                epd_interlace_4bpp_line_VE(to,from,difference,dirty,16);
                pair.lookup_func((uint32_t*)difference,output,(uint8_t*)lut,16);
                for (int target=0;target<16;++target) {
                    const uint8_t cell=phases->luts[(f*16+target)*4+old/4];
                    const int expected=(cell>>(6-2*(old&3)))&3;
                    if (((output[target/4]>>(6-2*(target&3)))&3)!=expected) return false;
                }
            }
        }
    }
    return true;
}
#endif
