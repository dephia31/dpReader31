/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 锁屏进睡、浅睡等待按键或拿起、软睡/关机拉掉 EN。
 *
 * Enter lock and sleep, light-sleep wait for key or pickup, and drop EN
 * for soft sleep / power-off.
 * 用户修订：票根从任意页面进入均用系统字体；浅睡唤醒恢复此前阅读字体，深睡由启动检查点恢复。
 * User revision: tickets use the system face from every page; light wake restores the previous reader face, while deep wake uses the startup checkpoint.
 * 用户修订：其他样式/自动锁屏先准备完整前缓冲，再清屏并做原 GC16 输出，缩短白屏等待。
 * User revision: other styles and idle locks prepare the complete front buffer before the original clear/GC16 output, shortening white-screen waits.
 * 用户修订：拼贴的新整帧缓存移到完整显示之后写入，密码层覆盖前提交。
 * User revision: persist new collage frames after complete presentation and before PIN overlays.
 * 用户修订：主动拼贴锁屏立即清白并显示圆锁，随后只用一次原GC16输出完整拼贴；取消占位和四步显影。
 * User revision: manual collage locks promptly present white and a round lock, then present the complete collage once with original GC16; remove placeholders and four-pass reveal.
 * 用户修订：合入官方 a2a63a6 的外设停用、IOE 收口与 CPU 浅睡保持；保留十分钟浅睡、密码和阅读检查点。
 * User revision: port official a2a63a6 peripheral/IOE parking and CPU light-sleep retention while preserving ten-minute light sleep, PIN and reader checkpoints.
 * 板级：Pico 的锁屏待机交给 CW32 PMU（十分钟 SOFT_SLEEP 后深睡，IOE_INT 唤醒）；S31 的浅睡/深睡/关机
 * 由锁屏待机设置与 TPS65185 电源轨决定，按键来自扩展口，下载交接走板上 USJ 路由。
 * Boards: the Pico hands lock standby to the CW32 PMU (ten-minute SOFT_SLEEP then deep sleep, IOE_INT
 * wake); the S31 takes its light/deep/off policy from the lock standby setting and the TPS65185
 * rails, with expander keys and the on-board USJ download route.
 * 冻结：沿用实际后缓冲，不把圆锁当白色；不增加帧副本，自动休眠/其他样式/失败回退保留原流程。
 * Frozen: Retain true history including the badge, add no frame copy and preserve idle/other-style/failure recovery.
 */
#include "ui_avatar.h"
#include "book_cover_jobs.h"

#include "sleep.h"
#include "boot_state.h"
#include "kiiko_board.h"
#include "lock_pin.h"
#include "lock_screen.h"
#include "read_pico_sd.h"
#include "usb_storage.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <time.h>

#include "app.h"
#include "app_font_context.h"
#include "book_cover.h"
#include "book_lock_collage.h"
#include "book_lock_bookmark.h"
#include "book_ticket.h"
#include "display.h"
#include "e0470_epaper_waveform.h"
#include "ota_online.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pmu_selftest.h"
#include "read_pico_board.h"
#include "read_pico_buzzer.h"
#include "ble_page_turner.h"
#include "read_pico_pmu.h"
#include "read_pico_transfer.h"
#include "sc7a20h_lab.h"
#include "settings.h"
#include "ttf_font.h"
#include "ui_kit.h"
#include "ui_image_dither.h"
#include "ui_wallpaper.h"
#include "ui_lock_feedback.h"

#if KIIKO_BOARD_METALIO47
#include "metalio_hw.h"
#include "book_source.h"
#include "epd_lcd.h"
#include "esp_app_desc.h"
#else
#include "esp_private/sleep_cpu.h"
#endif

static const char* TAG = "read_pico";

#if KIIKO_BOARD_METALIO47
// S31 锁屏状态：待机设置是否需要恢复点、锁屏画面是否已成功缓存、当前锁屏触摸句柄。
// S31 lock state: whether the resume point is saved, whether the lock frame is cached, and the
// lock touch handle.
static bool s_lock_feedback_started;
static bool s_lock_frame_cached;
static cst836u_handle_t s_lock_sleep_touch;
static esp_err_t lock_light_sleep_once(sc7a20h_handle_t acc, cst836u_handle_t touch, bool *key_wake);
#else
static bool s_lock_feedback_visible;
#endif
// 两板共用的深睡准入标志：由主循环在成功保存恢复点后设置。
// Shared deep-sleep admission flag: set by the main loop after a successful resume save.
static bool s_lock_deep_ready;
#if KIIKO_BOARD_METALIO47
void app_lock_set_resume_ready(bool ready) { s_lock_deep_ready = ready; }
#endif

#define APP_LOCK_KEY_POLL_US (2LL * 1000 * 1000)
#define APP_LOCK_STUCK_IRQ_US (3LL * 1000 * 1000)
#define APP_LOCK_SLEEP_ERROR_LIMIT 5
// PMU 确认后仍未掉 EN 时恢复调用方，禁止无限等待。/ Restore the caller if EN stays high after acknowledgement instead of waiting forever.
#define HOST_EN_DROP_WAIT_MS 5000
#if !KIIKO_BOARD_METALIO47
// Pico 锁屏先浅睡十分钟，再交给 PMU 软睡；S31 的同类期限由 APP_LOCK_IDLE_TIMEOUT_MS 表示。
// The Pico light-sleeps ten minutes before the PMU soft sleep; the S31 expresses the same deadline as APP_LOCK_IDLE_TIMEOUT_MS.
#define APP_LOCK_LIGHT_SLEEP_MS (10U * 60U * 1000U)
#endif

extern const uint8_t lock_4bpp_bin_start[] asm("_binary_lock_4bpp_bin_start");

#if KIIKO_BOARD_METALIO47
// 中文：保留S31灭灯/按键边界，反馈期间的新按键留给锁屏唤醒。
// English: Keep S31 lamp/key boundaries; preserve new presses during feedback for waking.
void app_lock_feedback(EpdiyHighlevelState *hl) {
    s_lock_feedback_started = false;
    if (!hl || app_settings_lock_style() != 2 || pico_online_busy()) return;
    read_pico_transfer_status_t transfer = {0};
    read_pico_transfer_get_status(&transfer);
    if (transfer.state == READ_PICO_TRANSFER_UPLOADING) return;
    uint8_t *fb = epd_hl_get_framebuffer(hl);
    if (!fb || epd_rotated_display_width() < 97 || epd_rotated_display_height() < 97) return;
    metalio_frontlight(0, 50);
    read_pico_pmu_drain_events();
    s_lock_feedback_started = true;
    display_main_transition_cancel();
    epd_hl_set_all_white(hl);
    if (update_display_full(hl) != EPD_DRAW_SUCCESS) return;
    EpdRect area = ui_lock_feedback_draw(fb);
    (void)update_display_area_diff_with(hl, &E0470_FOLLOW_WAVEFORM, MODE_DU, area);
    epd_poweroff();
}
#else
void app_lock_feedback(EpdiyHighlevelState *hl) {
    s_lock_feedback_visible = false;
    if (!hl || app_settings_lock_style() != 2 || pico_online_busy()) return;
    read_pico_transfer_status_t transfer = {0};
    read_pico_transfer_get_status(&transfer);
    if (transfer.state == READ_PICO_TRANSFER_UPLOADING) return;
    uint8_t *framebuffer = epd_hl_get_framebuffer(hl);
    if (!framebuffer || epd_rotated_display_width() < 97 || epd_rotated_display_height() < 97) return;
    display_main_transition_cancel();
    // 原GC16清白适用于任意旧灰阶，不用只保证黑白端点的短DU擦整屏，无额外定时停留。
    // Calibrated GC16 whitens any prior gray; endpoint-only short DU must not erase a whole gray page.
    epd_hl_set_all_white(hl);
    if (update_display_full(hl) != EPD_DRAW_SUCCESS) return;
    EpdRect area = ui_lock_feedback_draw(framebuffer);
    s_lock_feedback_visible = update_display_area_diff_with(hl, &E0470_FOLLOW_WAVEFORM,
        MODE_DU, area) == EPD_DRAW_SUCCESS;
}
#endif

static bool draw_wallpaper(uint8_t *framebuffer) {
    const char *path = app_settings_wallpaper_path();
    return app_settings_lock_style() == 1 &&
        ui_wallpaper_prepare_lock(framebuffer,path);
}

#if KIIKO_BOARD_METALIO47
void app_sleep_bind(cst836u_handle_t tp, sc7a20h_handle_t acc) {
    // S31 的触摸与加速度计不共用 CPU 浅睡保持区；接口保留为空实现。
    // The S31 keeps no CPU light-sleep retention for touch/accelerometer; the interface stays an empty stub.
    (void)tp;
    (void)acc;
}

// 中文：S31 的扩展口按键由适配层后台采样，这里只等松开后的稳定窗口。
// English: The S31 expander keys are sampled by the adapter's background task; wait only for a
// steady released window.
void app_lock_wait_key_idle(int timeout_ms) {
    int64_t start = esp_timer_get_time() / 1000;
    int64_t released_from = -1;
    while (esp_timer_get_time() / 1000 - start < timeout_ms) {
        read_pico_pmu_drain_events();
        read_pico_clear_ioe_int();
        bool held = true;
        if (read_pico_pmu_poll() == ESP_OK) {
            held = (read_pico_pmu_get()->key_state & 0x01) != 0;
        }
        // GPIO2还承载触摸/传感器中断，不能用它判断电源键是否松开。
        // GPIO2 also carries touch/sensor IRQs; use the debounced TCA power-key state.
        if (!held) {
            if (released_from < 0) released_from = esp_timer_get_time() / 1000;
            if (esp_timer_get_time() / 1000 - released_from >= 30) return;
        } else {
            released_from = -1;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
#else
static cst836u_handle_t s_tp;
static sc7a20h_handle_t s_acc;
static bool s_acc_was_on;

void app_sleep_bind(cst836u_handle_t tp, sc7a20h_handle_t acc) {
    s_tp = tp;
    s_acc = acc;
    // 没开 PM 时 IDF 不分配 CPU 保存区，浅睡里 CPU 不会掉电；这里手动申请一次。
    // Without PM, IDF never allocates CPU retention, so light sleep keeps the CPU powered; request it once here.
    esp_err_t err = sleep_cpu_configure(true);
    if (err != ESP_OK) ESP_LOGW(TAG, "CPU light-sleep power-down off: %s", esp_err_to_name(err));
}

// 触摸、加速度计都在常开 3V3 上，ESP 浅睡或 EN 拉低都不会断它们的电，必须主动送进最低功耗态。
// Touch and accelerometer sit on the always-on 3V3; neither light sleep nor EN low cuts them, so park them explicitly.
static void periph_suspend(bool host_off) {
    if (read_pico_buzzer_score_busy()) (void)read_pico_buzzer_score_stop();
    esp_err_t ioe = read_pico_ioe_sleep(host_off);
    if (ioe != ESP_OK) ESP_LOGW(TAG, "ioe sleep %s", esp_err_to_name(ioe));
    s_acc_was_on = sc7a20h_powered(s_acc);
    if (s_acc_was_on) (void)sc7a20h_power_down(s_acc);
    if (s_tp != NULL && cst836u_get_mode(s_tp) != CST836U_MODE_DEEPSLEEP) {
        esp_err_t err = cst836u_set_mode(s_tp, CST836U_MODE_DEEPSLEEP);
        if (err != ESP_OK) ESP_LOGW(TAG, "touch deepsleep %s", esp_err_to_name(err));
    }
}

// 深睡的触摸只认 TP_RST 复位；加速度计按睡前的配置重新上电。
// Deep-sleeping touch only wakes via TP_RST; the accelerometer comes back with its pre-sleep config.
static void periph_resume(void) {
    (void)read_pico_ioe_wake();
    if (s_tp != NULL && cst836u_get_mode(s_tp) == CST836U_MODE_DEEPSLEEP) {
        esp_err_t err = cst836u_wake(s_tp);
        if (err != ESP_OK) ESP_LOGW(TAG, "touch wake %s", esp_err_to_name(err));
    }
    if (s_acc_was_on) (void)sc7a20h_power_up(s_acc);
    s_acc_was_on = false;
}

// 单击提示只保留一个小区域；等待双击窗口结束才绘制，触摸仅临时唤醒三秒。
// Retain one small hint region; draw after the double-click window and wake touch for only three seconds.
static EpdiyHighlevelState *s_lock_hint_hl;
static struct {
    uint8_t *pixels;
    EpdRect area;
    uint32_t serial;
    int64_t pending_at, touch_until;
    bool touch_awake;
} s_lock_hint;

static uint8_t lock_hint_pixel(uint8_t *fb,int x,int y) {
    int px=x,py=y;
    switch(epd_get_rotation()) {
        case EPD_ROT_PORTRAIT:px=epd_width()-y-1;py=x;break;
        case EPD_ROT_INVERTED_LANDSCAPE:px=epd_width()-x-1;py=epd_height()-y-1;break;
        case EPD_ROT_INVERTED_PORTRAIT:px=y;py=epd_height()-x-1;break;
        default:break;
    }
    return epd_get_pixel(px,py,epd_width(),epd_height(),fb);
}

static void lock_hint_present(void) {
    epd_poweron();
    enum EpdDrawError result=update_display_area_diff_with(s_lock_hint_hl,&E0470_WAVEFORM,MODE_GL16,s_lock_hint.area);
    guard_draw_result(s_lock_hint_hl,result);
    epd_poweroff();
}

static void lock_hint_finish(bool restore) {
    if(s_lock_hint.pixels) {
        uint8_t *fb=epd_hl_get_framebuffer(s_lock_hint_hl);
        EpdRect r=s_lock_hint.area;
        for(int y=0;y<r.height;++y)for(int x=0;x<r.width;++x) {
            size_t i=(size_t)y*r.width+x;
            uint8_t v=(s_lock_hint.pixels[i/2]>>((i&1)*4))&15;
            epd_draw_pixel(r.x+x,r.y+y,(uint8_t)(v*17),fb);
        }
        if(restore)lock_hint_present();
        heap_caps_free(s_lock_hint.pixels);s_lock_hint.pixels=NULL;
    }
    if(s_lock_hint.touch_awake && s_tp) (void)cst836u_set_mode(s_tp,CST836U_MODE_DEEPSLEEP);
    s_lock_hint.touch_awake=false;s_lock_hint.pending_at=s_lock_hint.touch_until=0;
}

static void lock_hint_service(void) {
    if(!s_lock_hint_hl)return;
    int64_t now=esp_timer_get_time()/1000;
    uint32_t serial=read_pico_pmu_lock_key_serial();
    if(serial!=s_lock_hint.serial) {
        s_lock_hint.serial=serial;s_lock_hint.pending_at=now+650;
    }
    if(s_lock_hint.pending_at && now>=s_lock_hint.pending_at) {
        s_lock_hint.pending_at=0;s_lock_hint.touch_until=now+3000;
        if(!s_lock_hint.pixels) {
            uint8_t *fb=epd_hl_get_framebuffer(s_lock_hint_hl);
            const int w=epd_rotated_display_width(),h=epd_rotated_display_height();
            s_lock_hint.area=(EpdRect){(w-448)/2,h-196,448,72};
            if(fb && w>=448 && h>=268)
                s_lock_hint.pixels=heap_caps_calloc(1,448u*72u/2u,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
            if(s_lock_hint.pixels) {
                EpdRect r=s_lock_hint.area;
                for(int y=0;y<r.height;++y)for(int x=0;x<r.width;++x) {
                    size_t i=(size_t)y*r.width+x;
                    s_lock_hint.pixels[i/2]|=(lock_hint_pixel(fb,r.x+x,r.y+y)>>4)<<((i&1)*4);
                }
                ui_fill_round_rect(fb,r,28,UI_GRAY_WHITE);
                ui_draw_round_rect(fb,r,28,UI_GRAY_BLACK);
                ui_text_fixed_vc(fb,w/2,r.y+r.height/2,28,"双击电源键解锁",EPD_DRAW_ALIGN_CENTER,false);
                lock_hint_present();
            }
        }
        if(!s_lock_hint.touch_awake && s_tp && cst836u_wake(s_tp)==ESP_OK)s_lock_hint.touch_awake=true;
    }
    if(s_lock_hint.touch_until && now>=s_lock_hint.touch_until)lock_hint_finish(true);
    if(s_lock_hint.touch_awake && s_tp) {
        cst836u_touch_t touch={0};
        // 窗口内点屏保留同一提示，不重绘、不累计延时，也绝不触发解锁。
        // Screen taps retain this hint without redraws or extending the window, and can never unlock.
        (void)cst836u_read(s_tp,&touch);
    }
}

static bool lock_wait_key(void) {
    bool wake = app_settings_wake_presses() == 1
        ? read_pico_pmu_take_key_wakeup_single() : read_pico_pmu_take_key_wakeup();
    if(wake)return true;
    lock_hint_service();return false;
}

static void lock_arm_ioe_wakeup(void) {
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << READ_PICO_IOE_INT_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io);
    read_pico_clear_ioe_int();
    gpio_wakeup_enable((gpio_num_t)READ_PICO_IOE_INT_GPIO, GPIO_INTR_LOW_LEVEL);
    esp_sleep_enable_gpio_wakeup();
}

void app_lock_wait_key_idle(int timeout_ms) {
    int64_t start = esp_timer_get_time() / 1000;
    int64_t high_from = 0;
    while (esp_timer_get_time() / 1000 - start < timeout_ms) {
        read_pico_pmu_drain_events();
        read_pico_clear_ioe_int();
        bool held = false;
        if (read_pico_pmu_poll() == ESP_OK) {
            held = (read_pico_pmu_get()->key_state & 0x01) != 0;
        }
        bool int_high = gpio_get_level((gpio_num_t)READ_PICO_IOE_INT_GPIO) != 0;
        if (!held && int_high) {
            if (high_from == 0) high_from = esp_timer_get_time() / 1000;
            if (esp_timer_get_time() / 1000 - high_from >= 150) return;
        } else {
            high_from = 0;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

static void pickup_ack(sc7a20h_handle_t acc) {
    sc7a20h_events_t ev;
    sc7a20h_read_events(acc, &ev);
    sc7a20h_ack_int(acc);
}

static bool pickup_ia(sc7a20h_handle_t acc) {
    sc7a20h_events_t ev;
    sc7a20h_aoi_src_t aoi;
    if (sc7a20h_read_events(acc, &ev) != ESP_OK) return false;
    sc7a20h_aoi_decode(ev.aoi1_src, &aoi);
    return aoi.ia;
}

// 第一次低电平不够：高通余波还会再拉高。要连续安静一段时间才进浅睡。
// A first low is not enough: high-pass ringing can rise again. Wait for a quiet stretch before light sleep.
static bool pickup_wait_quiet(sc7a20h_handle_t acc, int quiet_ms, int timeout_ms) {
    int64_t start = esp_timer_get_time() / 1000;
    int64_t low_from = 0;
    while (esp_timer_get_time() / 1000 - start < timeout_ms) {
        if (sc7a20h_int1_level(acc) == 0) {
            if (low_from == 0) low_from = esp_timer_get_time() / 1000;
            if (esp_timer_get_time() / 1000 - low_from >= quiet_ms) return true;
        } else {
            pickup_ack(acc);
            low_from = 0;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    return false;
}
#endif

app_wake_source_t app_last_wake_source(void) {
    return (app_wake_source_t)app_settings_last_wake();
}

#if KIIKO_BOARD_METALIO47
// 中文：默认保留CPU/PSRAM/TF；可选硬件浅睡使用下方完整外设事务。
// USB/拿起唤醒或事务失败时保留待机；任何定时唤醒都不解锁、不进入深睡。
// English: Default retains CPU/PSRAM/TF; optional light sleep uses the complete transaction below.
// USB/pickup/failure retain standby; a timer maintenance wake never unlocks or escalates to deep sleep.
app_wake_source_t app_light_sleep_wait_timed(sc7a20h_handle_t acc,uint32_t timeout_ms) {
    if (!(book_cover_jobs_quiesce(2000) && ui_avatar_quiesce(2000))) return APP_WAKE_TIMEOUT;
    const int64_t start=esp_timer_get_time();
    const int64_t deadline=timeout_ms?start+(int64_t)timeout_ms*1000:0;
    bool pickup=acc && app_settings_pickup_wake();
    sc7a20h_sample_t last={0}; bool last_valid=false;
    int64_t motion_at=start;
    if(pickup) sc7a20h_power_up(acc);
    else if(acc) (void)sc7a20h_shared_irq_idle(acc);
    metalio_frontlight(0,50);
    app_wake_source_t wake=APP_WAKE_NONE;
    bool usb_was_blocked=metalio_usb_blocks_sleep();
    bool hardware_failed=false;
    ESP_LOGI(TAG,"S31 lock wait: mode=%u, USB/pickup use retained standby",app_settings_lock_sleep());
    for(;;) {
        if(read_pico_pmu_take_key_wakeup()) { wake=APP_WAKE_KEY; break; }
        if (!hardware_failed && !deadline && s_lock_sleep_touch && !pickup && !metalio_usb_blocks_sleep()) {
            unsigned mode=app_settings_lock_sleep();
            if(mode==1) {
                bool key=false;
                esp_err_t err=lock_light_sleep_once(acc,s_lock_sleep_touch,&key);
                if(key) { wake=APP_WAKE_KEY; break; }
                if(err!=ESP_OK) hardware_failed=true;
            } else if(mode==2 && s_lock_deep_ready) {
                esp_err_t err=book_suspend_io();
                if(err==ESP_OK && s_lock_frame_cached && !lock_pin_enabled())
                    (void)pico_boot_mark_frame_cached();
                if(err==ESP_OK) err=app_enter_host_sleep(APP_SLEEP_DEEP);
                (void)book_resume_io();
                ESP_LOGW(TAG,"S31 lock deep refused: %s; retained standby",esp_err_to_name(err));
                hardware_failed=true;
            }
        }
        int64_t now_us=esp_timer_get_time();
        if(pickup && now_us>=motion_at) {
            motion_at=now_us+250000;
            sc7a20h_sample_t now;
            // 仅比较新的完整样本，降低采样频率；短按由原厂TCA后台任务去抖。
            // Compare fresh complete samples at 4Hz; vendor TCA background sampling debounces short keys.
            if(sc7a20h_read(acc,&now)==ESP_OK && (now.status&0x08)) {
                if(last_valid && (abs(now.x_mg-last.x_mg)+abs(now.y_mg-last.y_mg)+abs(now.z_mg-last.z_mg)>550)) { wake=APP_WAKE_PICKUP; break; }
                last=now;last_valid=true;
            }
        }
        bool usb=metalio_usb_blocks_sleep();
        if (usb && !usb_was_blocked) metalio_usb_serial_reconnect();
        usb_was_blocked=usb;
        if(deadline && esp_timer_get_time()>=deadline && !usb) { wake=APP_WAKE_TIMEOUT; break; }
        read_pico_clear_ioe_int();
        // 不重复操作低功耗/唤醒寄存器；每轮阻塞以保证按键、空闲任务和看门狗得到调度。
        // Avoid repeated sleep/wake register transitions; block each loop for key/idle/watchdog scheduling.
        vTaskDelay(pdMS_TO_TICKS(30));
    }
    if(pickup) (void)sc7a20h_shared_irq_idle(acc);
    if (wake != APP_WAKE_TIMEOUT) metalio_frontlight_restore();
    app_settings_set_last_wake((uint8_t)(wake==APP_WAKE_TIMEOUT?APP_WAKE_NONE:wake));
    ESP_LOGI(TAG,"S31 retained lock exit cause=%u elapsed_ms=%lld irq=%d",(unsigned)wake,
        (long long)((esp_timer_get_time()-start)/1000),read_pico_ioe_int_level());
    return wake;
}
#else
app_wake_source_t app_light_sleep_wait_timed(sc7a20h_handle_t acc, uint32_t timeout_ms) {
    bool covers_idle=book_cover_jobs_quiesce(2000);
    bool avatar_idle=ui_avatar_quiesce(2000);
    bool poll_only=!(covers_idle && avatar_idle);
    bool pickup = acc != NULL && app_settings_pickup_wake();
    bool acc_armed = false;
    const int64_t deadline_us = timeout_ms
        ? esp_timer_get_time() + (int64_t)timeout_ms * 1000 : 0;
    periph_suspend(false);
    lock_arm_ioe_wakeup();
    if (pickup) {
        sc7a20h_motion_cfg_t motion = SC7A20H_MOTION_DEFAULT();
        motion.ths_mg = 350;
        motion.duration = 3;
        sc7a20h_arm_pickup_wake(acc, &motion);
        acc_armed = true;
        pickup_ack(acc);
        if (!pickup_wait_quiet(acc, 400, 2000)) {
            pickup = false;
            ESP_LOGW(TAG, "pickup INT1 stuck high, key only");
        } else {
            sc7a20h_config_light_sleep_wakeup(acc);
            if (!pickup_wait_quiet(acc, 400, 1500)) {
                gpio_wakeup_disable(sc7a20h_int1_gpio(acc));
                pickup = false;
                ESP_LOGW(TAG, "pickup INT1 re-asserted, key only");
            } else {
                // 安静窗口里若 INT1 闪过，ESP 会留下高电平唤醒挂起，关掉再打开清掉。
                // If INT1 glitched in the quiet window, ESP keeps a high-level wake pending; disable then re-enable to clear it.
                gpio_wakeup_disable(sc7a20h_int1_gpio(acc));
                sc7a20h_config_light_sleep_wakeup(acc);
                if (sc7a20h_int1_level(acc) > 0) {
                    gpio_wakeup_disable(sc7a20h_int1_gpio(acc));
                    pickup = false;
                    ESP_LOGW(TAG, "pickup INT1 high at sleep, key only");
                } else {
                    ESP_LOGI(
                        TAG, "wait key GPIO%d or pickup GPIO%d",
                        READ_PICO_IOE_INT_GPIO, (int)sc7a20h_int1_gpio(acc)
                    );
                }
            }
        }
    } else {
        ESP_LOGI(TAG, "wait key on IOE_INT GPIO%d", READ_PICO_IOE_INT_GPIO);
    }

    app_wake_source_t wake = APP_WAKE_NONE;
    int64_t ioe_low_since_us = 0;
    unsigned sleep_errors = 0;
    esp_err_t last_sleep_error = ESP_OK;
    int64_t last_sleep_error_log_ms = 0;
    for (;;) {
        int64_t now_us = esp_timer_get_time();
        // Check the deadline before every interrupt recovery path. A stuck-low
        // IO expander must never bypass the ten-minute PMU handoff forever.
        // 每次处理中断前先检查期限，避免 IO 扩展器低电平让票根永远停住。
        if (deadline_us && now_us >= deadline_us) {
            wake = APP_WAKE_TIMEOUT;
            ESP_LOGI(TAG, "lock light sleep timeout after %u ms", (unsigned)timeout_ms);
            break;
        }
        // The PMU event is authoritative even if the IOE edge was missed.
        // Timer wakeups below provide a bounded fallback poll for that case.
        // 即使漏掉 IOE 中断，也由定时唤醒轮询 PMU 按键事件。
        if (lock_wait_key()) {
            wake = APP_WAKE_KEY;
            break;
        }
        if(poll_only) {vTaskDelay(pdMS_TO_TICKS(40));continue;}
        read_pico_clear_ioe_int();
        if (gpio_get_level((gpio_num_t)READ_PICO_IOE_INT_GPIO) == 0) {
            if (!ioe_low_since_us) ioe_low_since_us = now_us;
            if (now_us - ioe_low_since_us >= APP_LOCK_STUCK_IRQ_US) {
                // 中断或浅睡异常只能回退到轮询，不能冒充双击解锁。/ Fall back to polling on IRQ/sleep faults, never synthesize an unlock.
                ESP_LOGW(TAG, "lock IOE_INT stuck low; retain lock and poll key");
                poll_only=true;
            }
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }
        ioe_low_since_us = 0;
        if (pickup && sc7a20h_int1_level(acc) > 0 && pickup_ia(acc)) {
            wake = APP_WAKE_PICKUP;
            ESP_LOGI(TAG, "pickup wake");
            break;
        }
        if (pickup && sc7a20h_int1_level(acc) > 0) {
            pickup_ack(acc);
            pickup_wait_quiet(acc, 400, 1500);
            continue;
        }
        int64_t sleep_us = APP_LOCK_KEY_POLL_US;
        if(s_lock_hint.pending_at || s_lock_hint.touch_until)sleep_us=100000;
        if (deadline_us) {
            int64_t remaining_us = deadline_us - esp_timer_get_time();
            if (remaining_us < sleep_us) sleep_us = remaining_us;
        }
        if (sleep_us <= 0) continue;
        esp_err_t timer_error = esp_sleep_enable_timer_wakeup((uint64_t)sleep_us);
        if (timer_error != ESP_OK) {
            ESP_LOGW(TAG, "lock sleep timer rejected: %s", esp_err_to_name(timer_error));
            if (++sleep_errors >= APP_LOCK_SLEEP_ERROR_LIMIT) {
                poll_only=true;
            }
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        int64_t t0 = esp_timer_get_time();
        esp_err_t sleep_error = esp_light_sleep_start();
        int64_t dt_ms = (esp_timer_get_time() - t0) / 1000;
        read_pico_clear_ioe_int();
        if (lock_wait_key()) {
            wake = APP_WAKE_KEY;
            break;
        }
        if (pickup && sc7a20h_int1_level(acc) > 0 && pickup_ia(acc)) {
            if (dt_ms < 80) {
                ESP_LOGW(TAG, "pickup ignored, sleep %lld ms", (long long)dt_ms);
                pickup_ack(acc);
                pickup_wait_quiet(acc, 400, 1500);
                continue;
            }
            wake = APP_WAKE_PICKUP;
            ESP_LOGI(TAG, "pickup wake");
            break;
        }
        if (pickup && sc7a20h_int1_level(acc) > 0) {
            pickup_ack(acc);
            pickup_wait_quiet(acc, 400, 1500);
        }
        if (sleep_error != ESP_OK) {
            int64_t now_ms = esp_timer_get_time() / 1000;
            if (sleep_error != last_sleep_error || now_ms - last_sleep_error_log_ms >= 30000) {
                ESP_LOGW(TAG, "lock light sleep rejected: %s, IOE_INT=%d",
                         esp_err_to_name(sleep_error),
                         gpio_get_level((gpio_num_t)READ_PICO_IOE_INT_GPIO));
                last_sleep_error_log_ms = now_ms;
            }
            last_sleep_error = sleep_error;
            if (++sleep_errors >= APP_LOCK_SLEEP_ERROR_LIMIT) {
                ESP_LOGE(TAG, "lock light sleep repeatedly rejected; retain lock and poll key");
                poll_only=true;
            }
            // A rejected sleep must not spin at full CPU speed behind the static e-paper image.
            vTaskDelay(pdMS_TO_TICKS(100));
        } else {
            last_sleep_error = ESP_OK;
            sleep_errors = 0;
        }
    }

    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
    gpio_wakeup_disable((gpio_num_t)READ_PICO_IOE_INT_GPIO);
    if (acc_armed) {
        gpio_wakeup_disable(sc7a20h_int1_gpio(acc));
        sc7a20h_events_t ev;
        sc7a20h_read_events(acc, &ev);
        sc7a20h_power_down(acc);
        sc7a20h_int1_begin(acc);
    }
    lock_hint_finish(wake!=APP_WAKE_KEY);
    periph_resume();
    app_settings_set_last_wake((uint8_t)(wake == APP_WAKE_TIMEOUT ? APP_WAKE_NONE : wake));
    return wake;
}
#endif

app_wake_source_t app_light_sleep_wait(sc7a20h_handle_t acc) {
    return app_light_sleep_wait_timed(acc, 0);
}

#if KIIKO_BOARD_METALIO47
// 中文：深睡准入按原厂顺序：暂停采样→只留P17→等共享中断线释放→ext1任意低有效。
// English: Deep-sleep admission follows the vendor order: pause sampling, keep only P17, wait for
// the shared IRQ line, then arm ext1 on any low.
static void lock_restore_key_wake(void) {
    (void)esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
    esp_err_t err=metalio_restore_key_io();
    read_pico_pmu_keys_resume();
    if(err!=ESP_OK) ESP_LOGE(TAG,"S31 sleep input restore: %s",esp_err_to_name(err));
}

static bool lock_prepare_key_wake(void) {
    if(read_pico_pmu_keys_pause()!=ESP_OK) return false;
    esp_err_t err=metalio_prepare_key_wake();
    bool idle=false;
    // 对齐原厂 WaitIntLineHigh 的有界等待；确认电源键和共享低有效线已释放。
    // Follow the vendor bounded WaitIntLineHigh check; confirm key and shared active-low IRQ release.
    for(unsigned tries=0;err==ESP_OK && tries<30;tries++) {
        uint16_t inputs;
        err=metalio_io_read(&inputs);
        if(err==ESP_OK && (inputs&(1u<<15)) && read_pico_ioe_int_level()!=0) { idle=true; break; }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    if(idle) {
        err=esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
        if(err==ESP_OK) err=esp_sleep_enable_ext1_wakeup_io(1ULL<<READ_PICO_IOE_INT_GPIO,ESP_EXT1_WAKEUP_ANY_LOW);
    }
    if(err==ESP_OK && idle) return true;
    ESP_LOGW(TAG,"S31 deep sleep deferred: shared IRQ=%d result=%s",read_pico_ioe_int_level(),esp_err_to_name(err));
    lock_restore_key_wake();
    return false;
}

// 中文：按官方 power_hw 的顺序执行；保留PSRAM/排版/字体缓存，定时唤醒不解锁或点灯。
// English: Follow vendor power_hw ordering; timer wakes retain RAM/layout/font caches and never unlock/light.
static esp_err_t lock_light_sleep_once(sc7a20h_handle_t acc,cst836u_handle_t touch,bool *key_wake) {
    *key_wake=false;
    if (!touch || metalio_usb_blocks_sleep() || usb_storage_active() || pico_online_busy())
        return ESP_ERR_INVALID_STATE;
    read_pico_sd_info_t info;
    if(read_pico_sd_get_info(&info)!=ESP_OK || !info.mounted) return ESP_ERR_INVALID_STATE;
    bool touch_asleep=false,sd_closed=false,lcd_stopped=false,keys_ready=false,rails_cut=false,pins_prepared=false;
    uint16_t saved_rails=0;
    esp_err_t err=book_suspend_io();
    if(err==ESP_OK) err=ttf_font_suspend_io();
    if(err==ESP_OK) {
        err=cst836u_set_mode(touch,CST836U_MODE_DEEPSLEEP);
        touch_asleep=err==ESP_OK;
    }
    if(err==ESP_OK) { err=read_pico_sd_sync(); sd_closed=err==ESP_OK; }
    if(err==ESP_OK) {
        metalio_haptic_stop();
        metalio_frontlight(0,50);
        epd_poweroff();
        vTaskDelay(pdMS_TO_TICKS(120));
        epd_lcd_deinit(); lcd_stopped=true;
        pins_prepared=true;
        err=metalio_prepare_light_sleep_pins();
        if(err==ESP_OK && acc) err=sc7a20h_shared_irq_idle(acc);
    }
    // SD/屏总线准备期间仍由正常按键任务采样，不吞掉这段时间的新短按。
    // Keep clicks sampled during SD/panel preparation instead of masking them away.
    if(err==ESP_OK && read_pico_pmu_take_key_wakeup()) {
        *key_wake=true; err=ESP_ERR_INVALID_STATE;
    }
    if(err==ESP_OK) { keys_ready=lock_prepare_key_wake(); if(!keys_ready) err=ESP_ERR_INVALID_STATE; }
    if(err==ESP_OK) err=esp_sleep_enable_timer_wakeup(UINT64_C(60000000));
    if(err==ESP_OK && metalio_usb_blocks_sleep()) err=ESP_ERR_INVALID_STATE;
    if(err==ESP_OK) {
        rails_cut=true; err=metalio_sleep_cut_rails(&saved_rails);
    }
    if(err==ESP_OK) {
        err=metalio_hold_for_light_sleep();
    }
    if(err==ESP_OK) {
        ESP_LOGI(TAG,"S31 light sleep: slice<=60s; handles paused, RAM/font/index retained");
        err=esp_light_sleep_start();
        *key_wake=err==ESP_OK && (esp_sleep_get_wakeup_causes() & (1u<<ESP_SLEEP_WAKEUP_EXT1));
    }
    if(pins_prepared) metalio_release_light_sleep_pins();
    (void)esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
    if(rails_cut) {
        esp_err_t restore=metalio_sleep_restore_rails(saved_rails);
        if(err==ESP_OK) err=restore;
        metalio_frontlight(0,50);
    }
    if(keys_ready) lock_restore_key_wake();
    if(sd_closed) {
        // remount异步；只在实际挂载完成后恢复文件句柄，不能因任务创建成功就加载字体。
        // Remount is asynchronous: wait for mounted state before reopening handles, never just task creation.
        esp_err_t mounted=read_pico_sd_remount();
        if(mounted==ESP_OK) {
            int64_t deadline=esp_timer_get_time()+INT64_C(15000000);
            do {
                mounted=read_pico_sd_get_info(&info);
                if(mounted!=ESP_ERR_NOT_FINISHED) break;
                vTaskDelay(pdMS_TO_TICKS(20));
            } while(esp_timer_get_time()<deadline);
            if(mounted==ESP_OK && !info.mounted) mounted=ESP_ERR_INVALID_STATE;
        }
        if(err==ESP_OK) err=mounted;
    }
    esp_err_t font=ttf_font_resume_io(),book=book_resume_io();
    if(err==ESP_OK) err=font!=ESP_OK?font:book;
    if(lcd_stopped) read_pico_epd_restore_lcd();
    if(touch_asleep) {
        esp_err_t restore=cst836u_wake(touch);
        if(err==ESP_OK) err=restore;
    }
    metalio_haptic_resume();
    // 恢复失败仍保持锁屏；仅真正的电源键可交给PIN/解锁逻辑。
    // Restore failure retains the lock; only a real power wake reaches PIN/unlock handling.
    ESP_LOGI(TAG,"S31 light restore: result=%s power_wake=%u",esp_err_to_name(err),*key_wake);
    return err;
}

esp_err_t app_enter_host_sleep(app_sleep_mode_t mode) {
    if (!(book_cover_jobs_quiesce(2000) && ui_avatar_quiesce(2000))) return ESP_ERR_NOT_FINISHED;
    if (mode != APP_SLEEP_OFF && mode != APP_SLEEP_DEEP) return ESP_ERR_INVALID_ARG;
    if (mode == APP_SLEEP_DEEP && metalio_usb_blocks_sleep()) return ESP_ERR_INVALID_STATE;
    if (pico_online_busy() || !read_pico_transfer_pause_for_sleep()) return ESP_ERR_NOT_FINISHED;
    // 锁屏调用方已经持有无线暂停；失败时不能擅自恢复其会话。
    // An active lock owns the radio pause; failed deep admission must not release that ownership.
    bool restore_radios=s_lock_sleep_touch==NULL;
    esp_err_t err = ble_pt_suspend_for_sleep();
    if (err != ESP_OK) { if(restore_radios) read_pico_transfer_resume_after_sleep(); return err; }
    if (mode == APP_SLEEP_DEEP && !lock_prepare_key_wake()) {
        if(restore_radios) { ble_pt_resume_after_sleep(); read_pico_transfer_resume_after_sleep(); }
        return ESP_ERR_INVALID_STATE;
    }
    (void)app_sync_time_to_pmu();
    app_lock_font_t saved_font = {0};
    app_font_begin_lock(&saved_font);
    (void)ttf_font_open_builtin();
    err = read_pico_sd_sync();
    if (err != ESP_OK) {
        if (mode == APP_SLEEP_DEEP) lock_restore_key_wake();
        app_font_end_lock(&saved_font);
        if(restore_radios) { ble_pt_resume_after_sleep(); read_pico_transfer_resume_after_sleep(); }
        return err;
    }
    metalio_frontlight(0, 50); epd_poweroff();
    if (mode == APP_SLEEP_DEEP) {
        epd_lcd_deinit();
        metalio_hold_for_deep_sleep();
        esp_deep_sleep_start();
    }
    // 中文：沿用原厂15次100ms关机脉冲；未掉电时有界返回原页。
    // English: Keep the vendor 15 x 100ms power pulses; return to the page if power never drops.
    metalio_poweroff();
    vTaskDelay(pdMS_TO_TICKS(1000));
    (void)read_pico_sd_remount();
    app_font_end_lock(&saved_font);
    metalio_haptic_resume();
    if(restore_radios) { ble_pt_resume_after_sleep(); read_pico_transfer_resume_after_sleep(); }
    return ESP_ERR_TIMEOUT;
}
#else
esp_err_t app_enter_host_sleep(app_sleep_mode_t mode) {
    if (mode != APP_SLEEP_OFF && mode != APP_SLEEP_DEEP) return ESP_ERR_INVALID_ARG;
    // 产品页用 on_before_lock 保存并收齐后台；保留阅读内存以便交接失败仍可回到原页。
    // Product pages checkpoint/join in on_before_lock; retain reader state so a failed handoff can resume the same page.
    if (pico_online_busy() || !(book_cover_jobs_quiesce(2000) && ui_avatar_quiesce(2000)) || !read_pico_transfer_pause_for_sleep()) return ESP_ERR_INVALID_STATE;
    (void)ble_pt_stop(2000);
    (void)app_sync_time_to_pmu();
    // 必须先断面板轨，再拉低 MODE，避免常开逻辑仍耗电。/ Drop panel rails before parking MODE on the always-on supply.
    pmu_selftest_prepare_powerdown();
    periph_suspend(true);
    esp_err_t err = ESP_FAIL;
    const unsigned attempts = mode == APP_SLEEP_OFF ? 1 : 3;
    for (unsigned attempt = 0; attempt < attempts; ++attempt) {
        err = mode == APP_SLEEP_OFF ? read_pico_pmu_power_off() : read_pico_pmu_report_sleep();
        ESP_LOGI(TAG, "%s attempt %u: %s", mode == APP_SLEEP_OFF ? "power off" : "SOFT_SLEEP",
                 attempt + 1, esp_err_to_name(err));
        if (err == ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(HOST_EN_DROP_WAIT_MS));
            err = ESP_ERR_TIMEOUT;
            ESP_LOGE(TAG, "EN still high after PMU ack");
            break;
        }
        if (attempt + 1 < attempts) vTaskDelay(pdMS_TO_TICKS(200));
    }
    // 失败不复位重试；恢复外设，交还锁屏/密码层，阅读恢复检查点仍由原入口处理。
    // Do not reset/retry on failure; restore peripherals to the lock/PIN caller, which retains checkpoint ownership.
    periph_resume();
    read_pico_transfer_resume_after_sleep();
    return err;
}
#endif

esp_err_t app_sync_time_to_pmu(void) {
    time_t now = time(NULL);
    if (now < 946684800 || now > 4102444799U) {
        ESP_LOGW(TAG, "skip PMU time sync: system clock invalid");
        return ESP_ERR_INVALID_STATE;
    }
    uint32_t seconds = (uint32_t)now;
    uint8_t payload[4] = {
        (uint8_t)(seconds & 0xff),
        (uint8_t)((seconds >> 8) & 0xff),
        (uint8_t)((seconds >> 16) & 0xff),
        (uint8_t)((seconds >> 24) & 0xff),
    };
    esp_err_t err = read_pico_pmu_cmd(PMU_CMD_TIME_SYNC, payload, sizeof(payload));
    ESP_LOGI(TAG, "PMU time sync before power action: %s", esp_err_to_name(err));
    return err;
}

#if KIIKO_BOARD_METALIO47
void app_restart_host(void) {
    // 中文：S31 直接软复位主控；PMU 动作不参与重启，避免多一次断电窗口。
    // English: The S31 soft-resets the host itself; the PMU action is not part of a restart.
    (void)app_sync_time_to_pmu();
    ttf_font_open_builtin(); (void)read_pico_sd_sync();
    metalio_haptic_stop(); metalio_frontlight(0,50); epd_poweroff(); esp_restart();
}
#else
void app_restart_host(void) {
    (void)app_sync_time_to_pmu();
    pmu_selftest_prepare_powerdown();
    esp_err_t err = read_pico_pmu_action(PMU_ACTION_HOST_NORMAL_RESTART, 250, 0x5052);
    ESP_LOGI(TAG, "normal restart: %s", esp_err_to_name(err));
    if (err != ESP_OK) esp_restart();
    while (true) vTaskDelay(pdMS_TO_TICKS(1000));
}
#endif

#if KIIKO_BOARD_METALIO47
static bool s_download_active;
static void download_trace(const char *stage,bool first) {
    char state[256];metalio_usb_describe(state,sizeof(state));
    ESP_LOGI(TAG,"USB download %s: %s",stage,state);
    // 只记录本次准备过程，覆盖旧记录，不在滑动/常规轮询中写卡。
    // Keep one bounded preparation trace; no writes during normal polling or dragging.
    (void)mkdir("/sdcard/.metalio_s31",0777);
    FILE *log=fopen("/sdcard/.metalio_s31/usb-download.log",first?"w":"a");
    if (log) {
        fprintf(log,"%s %s %s\n",esp_app_get_description()->version,stage,state);
        (void)fclose(log);
    }
}
bool app_download_active(void) { return s_download_active; }
void app_cancel_download(void) {
    if (!s_download_active) return;
    metalio_download_cancel();
    s_download_active=false;
    ble_pt_resume_after_sleep();
    read_pico_transfer_resume_after_sleep();
}
esp_err_t app_enter_download(void) {
    if (usb_storage_active()) return ESP_ERR_INVALID_STATE;
    if (s_download_active) {
        download_trace("before_recover",true);
        esp_err_t retry=metalio_request_download();
        download_trace(retry==ESP_OK?"waiting_host":"recovery_failed",false);
        if (retry!=ESP_OK) app_cancel_download();
        return retry;
    }
    if (!(book_cover_jobs_quiesce(2000) && ui_avatar_quiesce(2000))) return ESP_ERR_NOT_FINISHED;
    if (!read_pico_transfer_pause_for_sleep()) return ESP_ERR_NOT_FINISHED;
    esp_err_t err=ble_pt_suspend_for_sleep();
    if (err!=ESP_OK) { read_pico_transfer_resume_after_sleep(); return err; }
    download_trace("before_prepare",true);
    err=metalio_download_prepare();
    if (err!=ESP_OK) {
        ble_pt_resume_after_sleep(); read_pico_transfer_resume_after_sleep(); return err;
    }
    (void)app_sync_time_to_pmu();
    // 保持TF/字体/LCD和调度器，由电脑使用官方USJ控制线进入ROM；等待时仍可返回退出。
    // Retain TF/fonts/LCD and scheduling; the host enters ROM via official USJ control lines. Cancel stays usable.
    err=metalio_request_download();
    if (err!=ESP_OK) {
        download_trace("recovery_failed",false);
        metalio_download_cancel();
        ble_pt_resume_after_sleep(); read_pico_transfer_resume_after_sleep();
        return err;
    }
    s_download_active=true;
    download_trace("waiting_host",false);
    return ESP_OK;
}

// 超时不重启也不解锁；只把真正的按键/拿起交回锁屏层。
// A timeout neither reboots nor unlocks; only a real key or pickup returns to the lock layer.
static void lock_wait_for_unlock(sc7a20h_handle_t acc) {
    for(;;) {
        app_wake_source_t wake=app_light_sleep_wait_timed(acc,APP_LOCK_IDLE_TIMEOUT_MS);
        if(wake==APP_WAKE_KEY || wake==APP_WAKE_PICKUP) return;
        // 超时不能触发重启或解锁；稳定性版继续保留当前锁屏内存。
        // A timeout cannot reboot or unlock; retain the current lock context in this stability build.
    }
}
#else
// 中文：Pico 没有板上 USB 路由，下载口交接保持固定的不可用结果。
// English: The Pico has no on-board USB route, so the download handoff stays fixed-unavailable.
/* Pico 没有板载 USB 路由，这三个下载交接入口只在 S31 存在（见 sleep.h 的板级守卫）。 */
/* The Pico has no on-board USB route, so the download handover entries exist on the S31 only;
 * see the board guard in sleep.h. */
#endif

#if KIIKO_BOARD_METALIO47
void enter_lock_and_sleep(
    EpdiyHighlevelState* hl, int64_t* ignore_until_ms, sc7a20h_handle_t acc, cst836u_handle_t touch,
    bool reader_background
) {
    // 先灭灯并消费本次锁屏动作；后续生成/推屏期间的新短按必须保留给唤醒。
    // Turn the lamp off and consume the initiating action before composition; retain later wake presses.
    metalio_frontlight(0, 50);
    if (!s_lock_feedback_started) read_pico_pmu_drain_events();
    s_lock_feedback_started = false;
    // Refuse to interrupt an active upload. The caller repaints its page when
    // this function returns, and the user can lock again after the transfer.
    bool radio_paused = read_pico_transfer_pause_for_sleep();
    if (!radio_paused) {
        metalio_frontlight_restore();
        pico_boot_clear_resume();
        ESP_LOGW(TAG, "lock postponed while transfer is busy");
        return;
    }
    if (ble_pt_suspend_for_sleep()!=ESP_OK) {
        read_pico_transfer_resume_after_sleep();
        metalio_frontlight_restore();
        pico_boot_clear_resume();
        ESP_LOGW(TAG,"S31 lock postponed: BLE shutdown failed"); return;
    }
    uint8_t* framebuffer = epd_hl_get_framebuffer(hl);
    // 仅清目标内存，旧页面在拼贴准备期间仍留在屏幕；不提前清白物理面板。
    // Clear only target memory; retain the old physical page until the complete collage is ready.
    epd_poweroff();
    uint8_t lock_style = app_settings_lock_style();
    // 锁屏待机（不关机）也先整屏刷白一次：把上一屏差分刷新留下的浅态拉回饱和白，
    // 锁屏画面再从干净白底叠上去，退出/唤醒后的黑度才稳定。
    // 透明 PNG 书封样式以最后显示的一页做底图，刷白会清掉底图，因此该样式保持原流程。
    // Lock standby (no power-off) also starts with a full white refresh: the previous
    // differential frame is pushed back to saturated white and the lock art is then
    // composed from that clean base. The transparent-PNG cover style draws over the
    // last presented page, so it keeps the original path without the white pass.
    if (lock_style != 3) guard_draw_result(hl, update_display_white(hl));
    s_lock_frame_cached=false;
    // 透明书封以最后成功推屏的真实参考作为底图，不取未显示的目标。
    // Overlay the last successfully presented reference, never an uncommitted target.
    if(lock_style==3)memcpy(framebuffer,hl->back_fb,(size_t)epd_width()*epd_height()/2);
    else epd_hl_set_all_white(hl);
    bool ticket = lock_style == 0;
    bool system_lock = ticket || lock_style == 2 || lock_pin_enabled();
    app_lock_font_t lock_font = {0};
    if (system_lock) app_font_begin_lock(&lock_font);
    // 阅读票根独占锁屏画布：始终先画最近书籍封面，再叠票根；自定义壁纸只属于壁纸模式。
    // Ticket mode owns the lock canvas: book cover first, ticket on top. Custom wallpaper is wallpaper-only.
    bool lock_drawn = ticket
        ? book_ticket_draw(framebuffer, reader_background)
        : lock_style == 2 ? book_lock_collage_prepare(framebuffer) :
          lock_style == 3 ? book_lock_bookmark_draw(framebuffer,app_settings_bookmark_path()) : draw_wallpaper(framebuffer);
    if (!lock_drawn) {
        // 中文：回退画面用系统圆锁，不再把锁屏图当底图清屏。
        // English: Fall back to the system lock badge instead of clearing with the lock image.
        ui_lock_fallback_draw(framebuffer);
    }
    const int64_t draw_at = esp_timer_get_time();
    enum EpdDrawError result = update_display_full(hl);
    if (result != EPD_DRAW_SUCCESS) {
        guard_draw_result(hl, result);
        result = update_display_full(hl);
    }
    ESP_LOGI(TAG, "S31 lock style=%u GC48 result=%d push_ms=%lld", lock_style,
        (int)result, (long long)((esp_timer_get_time() - draw_at) / 1000));
    if (result != EPD_DRAW_SUCCESS && !lock_pin_enabled()) {
        // 推屏失败不宣布锁屏完成；恢复本页与灯光，允许再次尝试。
        // A failed presentation does not enter sleep; restore the page and lamp for another attempt.
        epd_poweroff();
        if (system_lock) app_font_end_lock(&lock_font);
        read_pico_transfer_resume_after_sleep();
        ble_pt_resume_after_sleep();
        metalio_frontlight_restore();
        pico_boot_clear_resume();
        return;
    }
    // 中文：只缓存成功显示的完整锁屏，必须在密码层覆盖前提交。
    // English: Commit only successfully presented lock art, before the PIN overlay.
    if (lock_drawn && result == EPD_DRAW_SUCCESS) {
        if (lock_style == 2) book_lock_collage_cache_commit(framebuffer);
        if (lock_style == 1) ui_wallpaper_cache_commit(framebuffer);
        if(!lock_pin_enabled() && s_lock_deep_ready)s_lock_frame_cached=lock_screen_save(framebuffer);
    }
    // 密码启用时即使显示失败也保持验证边界，绝不回到未验证页面。
    // A display failure must never return an unauthenticated page when the PIN is enabled.
    // 中文：按所选方式等待；默认保留待机，没有定时深睡/自动解锁。
    // English: Use the selected mode; default retained standby has no timed deep sleep/unlock.
    ESP_LOGI(TAG, "lock LIGHT");
    epd_poweroff();
    s_lock_sleep_touch=touch;
    if (lock_pin_enabled()) lock_screen_authenticate(hl, touch, acc, false);
    else lock_wait_for_unlock(acc);
    s_lock_sleep_touch=NULL;
    s_lock_deep_ready=false;s_lock_frame_cached=false;
    pico_boot_clear_resume();
    if (system_lock) app_font_end_lock(&lock_font);
    read_pico_transfer_resume_after_sleep();
    ble_pt_resume_after_sleep();

    // 仅清目标画布，保留面板和真实参考帧；当前页接着单次重绘，无额外白屏。
    // Clear only the target canvas, retaining panel/reference history for the next page presentation.
    read_pico_pmu_drain_events();
    epd_hl_set_all_white(hl);
    read_pico_pmu_drain_events();
    if (ignore_until_ms) {
        *ignore_until_ms = esp_timer_get_time() / 1000 + APP_LOCK_IGNORE_BOOT_MS;
    }
    ESP_LOGI(TAG, "unlocked");
}
#else
void enter_lock_and_sleep(
    EpdiyHighlevelState* hl, int64_t* ignore_until_ms, sc7a20h_handle_t acc, cst836u_handle_t touch,
    bool reader_background
) {
    bool feedback_visible = s_lock_feedback_visible;
    s_lock_feedback_visible = false;
    // Refuse to interrupt an active upload. The caller repaints its page when
    // this function returns, and the user can lock again after the transfer.
    bool radio_paused = read_pico_transfer_pause_for_sleep();
    if (!radio_paused) {
        pico_boot_clear_resume();
        ESP_LOGW(TAG, "lock postponed while transfer is busy");
        return;
    }
    uint8_t* framebuffer = epd_hl_get_framebuffer(hl);
    // 主动拼贴在白底圆锁上准备最终画面；其他入口仍保留旧页面，只改RAM。
    // Prepare manual collages behind the white lock badge; other entries keep their prior page while drawing RAM.
    uint8_t lock_style = app_settings_lock_style();
    // 透明书封保留当前页面作为底图，不清白、不重排阅读页，也不读取旧阅读快照。
    // Keep the current page beneath the PNG; no white reset, reader reflow or old reader snapshot.
    if (lock_style != 3) epd_hl_set_all_white(hl);
    else memcpy(framebuffer,hl->back_fb,(size_t)epd_width()*epd_height()/2);
    bool ticket = lock_style == 0;
    bool system_lock = ticket || lock_style == 2 || lock_pin_enabled();
    app_lock_font_t lock_font = {0};
    if (system_lock) app_font_begin_lock(&lock_font);
    // 阅读票根独占锁屏画布：始终先画最近书籍封面，再叠票根；自定义壁纸只属于壁纸模式。
    // Ticket mode owns the lock canvas: book cover first, ticket on top. Custom wallpaper is wallpaper-only.
    bool lock_drawn = ticket
        ? book_ticket_draw(framebuffer, reader_background)
        : lock_style == 2 ? book_lock_collage_prepare(framebuffer) :
          lock_style == 3 ? book_lock_bookmark_draw(framebuffer,app_settings_bookmark_path()) : draw_wallpaper(framebuffer);
    if (!lock_drawn) {
        ui_lock_fallback_draw(framebuffer);
    }
    enum EpdDrawError result = EPD_DRAW_SUCCESS;
    if (feedback_visible) result = update_display_full(hl);
    if (!feedback_visible || result != EPD_DRAW_SUCCESS) {
        // 反馈成功时不再重复清白；GC16从真实圆锁历史输出。自动/其他样式/失败保留原清屏。
        // A successful badge needs no second white clear; GC16 uses its true history. Idle/other/fault paths retain original clearing.
        epd_poweron();
        epd_clear();
        result = epd_hl_update_screen_from_white(hl, MODE_GC16, 25);
    }
    if (lock_style == 2 && lock_drawn && result == EPD_DRAW_SUCCESS) book_lock_collage_cache_commit(framebuffer);
    if (lock_style == 1 && lock_drawn && result == EPD_DRAW_SUCCESS) ui_wallpaper_cache_commit(framebuffer);

    app_lock_wait_key_idle(800);
    // 锁屏先浅睡，十分钟后进入深睡；按键唤醒使用已保存的页面检查点。
    // Locks light-sleep first, enter deep sleep after ten minutes and wake through the saved page checkpoint.
    ESP_LOGI(TAG, "lock LIGHT");
    epd_poweroff();
    s_lock_hint_hl=hl;s_lock_hint.serial=read_pico_pmu_lock_key_serial();
    app_wake_source_t wake = APP_WAKE_KEY;
    if (lock_pin_enabled()) lock_screen_authenticate(hl, touch, acc, false);
    else wake = app_light_sleep_wait_timed(NULL, APP_LOCK_LIGHT_SLEEP_MS);
    while (wake == APP_WAKE_TIMEOUT) {
        // PMU SOFT_SLEEP drops the ESP rail while keeping its RTC alive. A short
        // power-key press boots the host; the e-paper keeps this lock image.
        if(!lock_pin_enabled() && lock_screen_save(framebuffer)) (void)pico_boot_mark_frame_cached();
        app_enter_host_sleep(APP_SLEEP_DEEP);
        wake=app_light_sleep_wait_timed(NULL,APP_LOCK_LIGHT_SLEEP_MS);
    }
    pico_boot_clear_resume();
    s_lock_hint_hl=NULL;
    if (system_lock) app_font_end_lock(&lock_font);
    read_pico_transfer_resume_after_sleep();

    // 参考帧和屏幕都归零，回到主循环后由当前页自己画一遍，不必知道是哪一页。
    // Zero the reference frame and the panel; the current page redraws after the loop resumes, without knowing which page it is.
    read_pico_pmu_drain_events();
    epd_poweron();
    epd_clear();
    epd_hl_set_all_white(hl);
    read_pico_pmu_drain_events();
    if (ignore_until_ms) {
        *ignore_until_ms = esp_timer_get_time() / 1000 + APP_LOCK_IGNORE_BOOT_MS;
    }
    ESP_LOGI(TAG, "unlocked");
}
#endif

#if KIIKO_BOARD_METALIO47
void app_lock_boot_gate(EpdiyHighlevelState *hl, cst836u_handle_t touch, sc7a20h_handle_t acc) {
    // 中文：无密码时沿用 rc99 的待机分支——有恢复点就先留在锁屏等真实按键。
    // English: Without a PIN keep rc99's standby branch: with a resume point, stay on the lock face until a real key.
    if (!lock_pin_enabled()) {
        if(pico_boot_has_resume()) {
            pico_boot_ready();pico_boot_hold_resume();
            (void)app_light_sleep_wait_timed(NULL,0);
        }
        return;
    }
    // 卡挂载有界等待仅为了字体/锁屏图；没有卡仍必须验证。
    // Bound mount waiting for the face/art only; absent media still requires authentication.
    read_pico_sd_info_t info;
    for (unsigned i = 0; i < 150 && read_pico_sd_get_info(&info) == ESP_ERR_NOT_FINISHED; ++i)
        vTaskDelay(pdMS_TO_TICKS(20));
    app_lock_font_t saved; app_font_begin_lock(&saved);
    uint8_t *fb = epd_hl_get_framebuffer(hl);
    if (!lock_screen_restore(fb)) {
        if(app_settings_lock_style()==3)memset(fb,255,(size_t)epd_width()*epd_height()/2);
        bool drawn = app_settings_lock_style() == 0 ? book_ticket_draw(fb, false) :
            app_settings_lock_style() == 2 ? book_lock_collage_prepare(fb) :
            app_settings_lock_style() == 3 ? book_lock_bookmark_draw(fb,app_settings_bookmark_path()) : draw_wallpaper(fb);
        // 回退画面用系统圆锁，不再把锁屏图当底图清屏。
        // Fall back to the system lock badge instead of clearing with the lock image.
        if (!drawn) ui_lock_fallback_draw(fb);
    }
    lock_screen_authenticate(hl, touch, acc, true);
    app_font_end_lock(&saved);
}
#else
void app_lock_boot_gate(EpdiyHighlevelState *hl, cst836u_handle_t touch, sc7a20h_handle_t acc) {
    s_lock_hint_hl=hl;s_lock_hint.serial=0;
    if (!lock_pin_enabled()) {
        if(pico_boot_has_resume()) {
            pico_boot_ready();pico_boot_hold_resume();
            (void)app_light_sleep_wait_timed(NULL,0);
        }
        s_lock_hint_hl=NULL;return;
    }
    // 卡挂载有界等待仅为了字体/锁屏图；没有卡仍必须验证。
    // Bound mount waiting for the face/art only; absent media still requires authentication.
    read_pico_sd_info_t info;
    for (unsigned i = 0; i < 150 && read_pico_sd_get_info(&info) == ESP_ERR_NOT_FINISHED; ++i)
        vTaskDelay(pdMS_TO_TICKS(20));
    app_lock_font_t saved; app_font_begin_lock(&saved);
    uint8_t *fb = epd_hl_get_framebuffer(hl);
    if (!lock_screen_restore(fb)) {
        // 冷启动丢失锁屏缓存时没有当前页面，用干净底图并继续密码校验。
        // A cold start with no retained art has no current page; use a clean base and keep authentication.
        if (app_settings_lock_style() == 3) memset(fb,255,(size_t)epd_width()*epd_height()/2);
        bool drawn = app_settings_lock_style() == 0 ? book_ticket_draw(fb, false) :
            app_settings_lock_style() == 2 ? book_lock_collage_draw(fb) :
            app_settings_lock_style() == 3 ? book_lock_bookmark_draw(fb,app_settings_bookmark_path()) : draw_wallpaper(fb);
        if (!drawn) ui_lock_fallback_draw(fb);
    }
    lock_screen_authenticate(hl, touch, acc, true);
    s_lock_hint_hl=NULL;
    app_font_end_lock(&saved);
}
#endif
