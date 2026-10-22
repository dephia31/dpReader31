/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：参考 MokuMMk/MokuReader 1d7951ec 的高斯模糊、边缘折射与亮边。
 * 处理关机、密码锁屏及顶部快捷控件；工作缓冲按次申请于PSRAM并释放，不写硬件或调用显示。
 * English: Gaussian blur, bezel refraction and highlights adapted from
 * MokuMMk/MokuReader commit 1d7951ec303c320a154282f4e6a579f687d0dc4f.
 * Power, PIN and top quick surfaces share it; PSRAM workspace is released after painting.
 * Frozen: no hardware writes, display calls, or retained framebuffer caches.
 */
#include "ui_glass.h"
#include "ui_kit.h"
#include "ui_image_dither.h"
#include "esp_heap_caps.h"
#include <math.h>
#include <string.h>
#define REFRACT_MAX 28
#define REFRACT_BEZEL 22
#define REFRACT_SAMPLES 128
#define REFRACT_N 1.5168f
#define REFRACT_T0 1.3f
#define GAUSS_RADIUS 3
#define GAUSS_TAPS (2 * GAUSS_RADIUS + 1)
#define UI_GLASS_FLOOR 2
#define UI_GLASS_INK_ALPHA 20
#if KIIKO_BOARD_METALIO47
#define UI_GLASS_MAX_PIXELS (UI_LOCK_WIDTH * 384u)
#else
#define UI_GLASS_MAX_PIXELS (UI_LOCK_WIDTH * 160u)
#endif
static const uint8_t s_gauss[GAUSS_TAPS]={9,28,55,72,55,28,9};
static const uint8_t s_reader_gauss[GAUSS_TAPS]={20,34,47,54,47,34,20};
static uint8_t s_disp[REFRACT_SAMPLES+1],s_spec[REFRACT_SAMPLES+1];
static bool s_refract_ready;
static void refract_prepare(void) {
    if (s_refract_ready) return;
    s_refract_ready = true;
    // 剖面与斜率：f(x) = (1-(1-x)^4)^(1/4)，x=0 在外沿、x=1 在平面起点。
    // Profile and slope: f(x) = (1-(1-x)^4)^(1/4), x=0 at the outer edge and x=1 where the flat starts.
    for (int i = 0; i <= REFRACT_SAMPLES; ++i) {
        const float x = (float)i / REFRACT_SAMPLES;
        const float u = 1.0f - x;
        const float h4 = 1.0f - u * u * u * u;
        const float f = h4 > 0.0f ? powf(h4, 0.25f) : 0.0f;
        // 解析导数 u³/f³ 在外沿发散，夹住即可：那里几乎不透过，位移随后被菲涅尔压掉。
        // The analytic derivative diverges at the rim; clamping is enough because almost nothing is
        // transmitted there and Fresnel fades the shift out anyway.
        const float slope = f > 0.02f ? (u * u * u) / (f * f * f) : 14.0f;
        const float theta_s = atanf(slope);
        const float theta_r = asinf(sinf(theta_s) / REFRACT_N);
        // 厚度向内变厚，位移是「厚度 × 偏折角」的乘积，所以峰值在外沿附近而不是最外一条线。
        // The glass thickens inward and the shift is thickness times deviation, which puts the peak
        // near the rim rather than on the outermost line.
        float d = (REFRACT_T0 + f) * tanf(theta_s - theta_r);
        // 菲涅尔：掠射的外沿几乎全反射，位移按透过率淡出，反射率本身留着做高光。
        // Fresnel: the grazing rim barely transmits, so fade the shift by transmittance, and keep the
        // reflectance itself to shape the bright rim.
        const float ci = cosf(theta_s);
        const float st = sinf(theta_s) / REFRACT_N;
        float refl = 1.0f;
        if (st < 1.0f) {
            const float ct = sqrtf(1.0f - st * st);
            const float rs = (ci - REFRACT_N * ct) / (ci + REFRACT_N * ct);
            const float rp = (REFRACT_N * ci - ct) / (REFRACT_N * ci + ct);
            refl = 0.5f * (rs * rs + rp * rp);
        }
        d *= 1.0f - refl;
        float px = d * (float)REFRACT_BEZEL;
        if (px < 0.0f) px = 0.0f;
        if (px > (float)REFRACT_MAX) px = (float)REFRACT_MAX;
        s_disp[i] = (uint8_t)(px + 0.5f);
        s_spec[i] = (uint8_t)(refl * 255.0f + 0.5f);
    }
    // 反射率在最外一像素上冲得极高，直接当高光就是一圈硬描边。做一次平滑摊到几像素上，
    // 才像弧形玻璃边缘那圈柔和的亮边。
    // Reflectance spikes on the outermost pixel; used raw it reads as a hard outline. One smoothing pass
    // spreads it over a few pixels, matching the soft rim of curved glass.
    static uint8_t smoothed[REFRACT_SAMPLES + 1];
    for (int i = 0; i <= REFRACT_SAMPLES; ++i) {
        int acc = 0, n = 0;
        for (int k = -16; k <= 16; ++k) {
            const int j = i + k;
            if (j < 0 || j > REFRACT_SAMPLES) continue;
            acc += s_spec[j];
            ++n;
        }
        // 再压到七成：物理反射率是按镜面算的，屏幕上不需要那么亮。
        // Scale to 70%: the physical value assumes a mirror finish, which a screen does not need.
        smoothed[i] = (uint8_t)(acc / n * 7 / 10);
    }
    memcpy(s_spec, smoothed, sizeof(s_spec));
}
static float refract_sample(const uint8_t *table, float x) {
    if (x <= 0.0f) return (float)table[0];
    if (x >= 1.0f) return (float)table[REFRACT_SAMPLES];
    const float at = x * REFRACT_SAMPLES;
    const int i = (int)at;
    const int j = i < REFRACT_SAMPLES ? i + 1 : REFRACT_SAMPLES;
    const float frac = at - (float)i;
    return (float)table[i] * (1.0f - frac) + (float)table[j] * frac;
}
// 固定七抽头的整数卷积允许展开；不改变浮点折射的计算次序。
// Unroll the fixed seven-tap integer convolution without changing floating-point refraction order.
#if defined(__GNUC__) && !defined(__clang__)
__attribute__((optimize("O3")))
#endif
static void panel_row_h(const uint8_t *source, int w, int row, uint8_t *dst,const uint8_t *kernel) {
    const uint8_t *pixels = source + (size_t)row * w;
    for (int x = 0; x < w; ++x) {
        int sum = 0;
        for (int k = 0; k < GAUSS_TAPS; ++k) {
            int sx = x + k - GAUSS_RADIUS;
            if (sx < 0) sx = 0; else if (sx >= w) sx = w - 1;
            sum += kernel[k] * pixels[sx];
        }
        dst[x] = (uint8_t)(sum >> 8);
    }
}
static float panel_sdf(float x, float y, float w, float h, float r) {
    const float qx = fabsf(x - w * 0.5f) - (w * 0.5f - r);
    const float qy = fabsf(y - h * 0.5f) - (h * 0.5f - r);
    const float mx = qx > 0.0f ? qx : 0.0f, my = qy > 0.0f ? qy : 0.0f;
    const float inner = qx > qy ? qx : qy;
    return ((mx == 0.0f && my == 0.0f) ? 0.0f : sqrtf(mx * mx + my * my)) +
        (inner < 0.0f ? inner : 0.0f) - r;
}
bool ui_glass_panel_frost(uint8_t *fb, EpdRect rect, int radius, int white, int refract,bool soften_text) {
    if (!fb || rect.width<=0 || rect.height<=0 || rect.x<0 || rect.y<0 ||
        rect.width>UI_LOCK_WIDTH || rect.height>UI_LOCK_HEIGHT ||
        rect.x>UI_LOCK_WIDTH-rect.width || rect.y>UI_LOCK_HEIGHT-rect.height) return false;
    const int w=rect.width,h=rect.height;
    const uint8_t *kernel=soften_text?s_reader_gauss:s_gauss;
    const size_t need=(size_t)w*h;
    if(need>UI_GLASS_MAX_PIXELS)return false;
    if(radius<0)radius=0;
    if(radius>w/2)radius=w/2;
    if(radius>h/2)radius=h/2;
    if(white<0)white=0;
    if(white>100)white=100;
    if(refract>200)refract=200;
    // 模糊只在进入时生成；缓冲不随控件或阅读正文常驻。
    // Build blur on entry only; no workspace survives into input or reading.
    uint8_t *s_panel_blur=heap_caps_malloc(need+(size_t)GAUSS_TAPS*w,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!s_panel_blur)return false;
    uint8_t (*s_panel_ring)[w]=(void *)(s_panel_blur+need);
    refract_prepare();
    // 原图只解包一次；输出复用这块存储。每行写回前，后续所需的原图行已进入横向环。
    // Unpack the source once and reuse its storage for output. Before overwriting a row, the
    // horizontal ring already owns every original row still needed by subsequent output.
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            s_panel_blur[(size_t)y * w + x] = ui_read_pixel(fb, rect.x + x, rect.y + y);
    int base = 0;
    for (int k = 0; k < GAUSS_TAPS; ++k) {
        int row = k - GAUSS_RADIUS;
        if (row < 0) row = 0; else if (row >= h) row = h - 1;
        panel_row_h(s_panel_blur, w, row, s_panel_ring[k],kernel);
    }
    for (int y = 0; y < h; ++y) {
        const uint8_t *rows[GAUSS_TAPS];
        for (int k = 0; k < GAUSS_TAPS; ++k) rows[k] = s_panel_ring[(base + k) % GAUSS_TAPS];
        for (int x = 0; x < w; ++x) {
            const int sum = kernel[0] * (rows[0][x] + rows[6][x]) + kernel[1] * (rows[1][x] + rows[5][x]) +
                            kernel[2] * (rows[2][x] + rows[4][x]) + kernel[3] * rows[3][x];
            s_panel_blur[(size_t)y * w + x] = (uint8_t)(sum >> 8);
        }
        if (y == h - 1) break;
        int next = y + GAUSS_RADIUS + 1;
        if (next >= h) next = h - 1;
        panel_row_h(s_panel_blur, w, next, s_panel_ring[base],kernel);
        base = (base + 1) % GAUSS_TAPS;
    }
    // 第二步：中央取模糊值，只有边框带折射，和底栏同一套模型。
    // 折射强度：refract 调大时同时加宽折射带并放大位移，玻璃边缘的形变范围更大。
    // Step two: the centre keeps the blurred value and only the bezel band refracts, same model as
    // the dock. A larger refract widens the band and grows the displacement.
    const float fw = (float)w, fh = (float)h, fr = (float)radius;
    const int refract_pct = refract < 10 ? 10 : refract;
    const float refract_scale = (float)refract_pct / 100.0f;
    const float bezel = (float)REFRACT_BEZEL * refract_scale;
    uint8_t tint[16];
    for (int value=0;value<16;++value) {
        int out=(value*(100-UI_GLASS_INK_ALPHA)+50)/100;
        if (white>0) {
            out+=(15-out)*white/100;
            if (out<UI_GLASS_FLOOR) out=UI_GLASS_FLOOR;
        }
        tint[value]=(uint8_t)out;
    }
    // 圆角距离和折射向量关于中心对称：每四个像素仅求一次几何，仍分别读取原模糊值。
    // Rounded distances/refraction are centre-symmetric: compute geometry once per four pixels, retaining each original blurred sample.
    for (int y = 0; y < (h+1)/2; ++y) {
        for (int x = 0; x < (w+1)/2; ++x) {
            const float dist = panel_sdf((float)x + 0.5f, (float)y + 0.5f, fw, fh, fr);
            if (dist > 0.0f) continue;
            const float t = -dist / bezel;
            int sx=x,sy=y,spec=0;
            if (t > 0.0f && t < 1.0f) {
                const float disp = refract_sample(s_disp, t) * refract_scale;
                spec = (int)(refract_sample(s_spec, t) + 0.5f);
                if (disp > 0.5f) {
                    const float e = 0.75f;
                    float gx = panel_sdf((float)x + e + 0.5f, (float)y + 0.5f, fw, fh, fr) -
                               panel_sdf((float)x - e + 0.5f, (float)y + 0.5f, fw, fh, fr);
                    float gy = panel_sdf((float)x + 0.5f, (float)y + e + 0.5f, fw, fh, fr) -
                               panel_sdf((float)x + 0.5f, (float)y - e + 0.5f, fw, fh, fr);
                    const float g = sqrtf(gx * gx + gy * gy);
                    if (g > 0.0001f) {
                        gx /= g; gy /= g;
                        sx = x - (int)lroundf(gx * disp);
                        sy = y - (int)lroundf(gy * disp);
                        if (sx < 0) sx = 0; else if (sx >= w) sx = w - 1;
                        if (sy < 0) sy = 0; else if (sy >= h) sy = h - 1;
                    }
                }
            }
            const int highlight=spec/17;
            for(int mirror_y=0;mirror_y<2;++mirror_y) {
                const int dy=mirror_y?h-1-y:y,source_y=mirror_y?h-1-sy:sy;
                if(mirror_y&&dy==y)continue;
                for(int mirror_x=0;mirror_x<2;++mirror_x) {
                    const int dx=mirror_x?w-1-x:x,source_x=mirror_x?w-1-sx:sx;
                    if(mirror_x&&dx==x)continue;
                    int out=tint[s_panel_blur[(size_t)source_y*w+source_x]]+highlight;
                    if(out>15)out=15;
                    epd_draw_pixel(rect.x+dx,rect.y+dy,
                        ui_image_dither_gray((uint8_t)(out*17),rect.x+dx,rect.y+dy),fb);
                }
            }
        }
    }
    heap_caps_free(s_panel_blur);
    return true;
}

bool ui_glass_panel(uint8_t *fb,EpdRect rect,int radius,int white,int refract) {
    return ui_glass_panel_frost(fb,rect,radius,white,refract,false);
}
