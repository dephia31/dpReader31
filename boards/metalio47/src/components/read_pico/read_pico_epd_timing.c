/* SPDX-License-Identifier: Apache-2.0
 * 中文：Metalio 8bit 扫描，沿用参考 76 时序；不套用 Pico 16bit 档位。
 * English: Metalio 8-bit scan uses reference 76 timing, independent of Pico's 16-bit profile.
 */
#include "read_pico_epd_timing.h"
#include "epdiy.h"
#include "output_lcd/lcd_driver.h"
static int clock_mhz=20;
int read_pico_epd_pclk_mhz(void) { return clock_mhz; }
int read_pico_epd_frame_target_us(void) { return 12000; }
void read_pico_epd_set_frame_target_us(int us) { (void)us; }
int read_pico_epd_ckv_high_max_01us(void) { return 170; }
void read_pico_epd_set_ckv_high_max_01us(int value) { (void)value; }
void read_pico_epd_scan(int mhz,read_pico_epd_scan_profile_t profile,read_pico_epd_scan_t *s) {
    (void)profile; (void)mhz;
    if (!s) return;
    *s=(read_pico_epd_scan_t){.pclk_mhz=20,.px_per_clk=4,.line_start=9,.line_back_porch=4,
        .line_data=304,.line_end=10,.line_clocks=340,.line_us=17,
        .frame_lines=700,.frame_us=12000,.frame_hz_x100=8333,.ckv_high_01us=159};
}
int read_pico_epd_refresh_ms(int mhz,read_pico_epd_scan_profile_t profile,int phases) {
    (void)mhz; (void)profile; return phases*12;
}
void read_pico_epd_set_pclk(int mhz) { (void)mhz; clock_mhz=20; }
void read_pico_epd_use_scan(read_pico_epd_scan_profile_t profile) { (void)profile; }

// 运行时白边屏蔽（0 = 不屏蔽）。此前面板最外圈 8 像素在运行时被写成保持（00），
// 四周因此永远停在开机刷白的状态：内容画不到那里，也不会随后续刷新更新，看起来
// 就是一圈白框。原厂固件（CloudZao/Metalio-E-INK4-Plus）对整屏像素一视同仁，
// 边角随内容刷新；这里跟原厂一致，让最外圈正常参与驱动。个别屏幕若边缘确有坏点，
// 可改回 4 或 8 局部回退；启动清屏仍可用 epd_lcd_prime_white_border() 单独刷白。
// Runtime white-border suppression (0 = off). The old port forced the outermost
// eight pixels to hold (00) during runtime, so the ring stayed at its boot-time
// white state: content could not reach it and it never updated again. The vendor
// firmware drives every pixel, edges included; match that here. Set 4 or 8 to
// fall back for a panel with a genuinely bad edge column.
#define METALIO_RUNTIME_WHITE_BORDER_PIXELS 0

void read_pico_epd_init_lcd(const lcd_bus_config_t *bus,int width_bits,int w,int h,const read_pico_epd_scan_t *scan) {
    (void)scan;
    LcdEpdConfig_t cfg={.pixel_clock=20000000,.ckv_high_time=60,.line_front_porch=4,
        .le_high_time=9,.bus_width=width_bits,.bus=*bus,.dummy_clocks_after_data=10,
        .white_border_pixels=METALIO_RUNTIME_WHITE_BORDER_PIXELS};
    epd_lcd_init(&cfg,w,h);
}
