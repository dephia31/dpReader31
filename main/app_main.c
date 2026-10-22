/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 用户修订：硬件前建立持久化启动保护；深睡恢复入口先消费再开书，中断图书进入可操作恢复页。
 * User revision: persist startup protection before hardware; consume deep-sleep checkpoints before opening books, and show an operable recovery page after interrupted opening.
 * 只做开机装配：拉起板级硬件、读设置、定 VCOM、放开机图、开字体；
 * 未标定则先拦住进设定页。然后把控制权交给 app_loop。
 * 板级差异只经 kiiko_board 的宏与两板 BSP：VCOM 来源（PMU / 本机 NVS）、USB 下载请求清理、
 * 音量键进调试页、深睡恢复判定。
 *
 * Boot wiring only: board init, settings, PMU RTC restore, VCOM, splash, fonts. If VCOM
 * is unset, the factory page blocks first. Then control goes to app_loop. Board differences
 * enter only through kiiko_board macros and the two BSPs: where VCOM comes from (PMU or local
 * NVS), download-request cleanup, the volume-key debug page, and the deep-sleep resume test.
 */

#include <stdint.h>
#include <string.h>
#include <sys/time.h>

#include "app_loop.h"
#include "app_content_open.h"
#include "boot_state.h"
#include "lock_screen.h"
#include "ui_lock_feedback.h"
#include "lock_pin.h"
#include "ttf_font.h"
#include "book_store.h"
#include "esp_system.h"
#include "read_pico_sd.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <sys/stat.h>
#include "app_registry.h"
#include "display.h"
#include "epd_highlevel.h"
#include "epdiy.h"
#include "esp_log.h"
#include "kiiko_board.h"
#include "ota_update.h"
#include "pmu_selftest.h"
#if KIIKO_BOARD_METALIO47
/* S31 没有 force-download 的 RTC 选项，而是 GPIO36 上的 USJ/UTMI 通路。 */
/* The S31 has no RTC force-download option; it switches the USJ/UTMI route on GPIO36. */
#include "debug_panel.h"
#include "metalio_hw.h"
#else
#include "soc/rtc_cntl_reg.h"
#endif
#include "read_pico_board.h"
#include "read_pico_crashlog.h"
#include "read_pico_init.h"
#include "read_pico_pmu.h"
#include "read_pico_pmu_protocol.h"
#include "settings.h"
#include "sleep.h"
#include "ttf_font.h"
#include "usb_storage.h"
#include "app_font_context.h"
#include "ui_kit.h"
#include "vcom_setup.h"

static const char* TAG = "read_pico";
extern const app_desc_t app_boot_recovery;

// 软复位保留有效的 RTC；断电才用常供电时基恢复。Pico 的 PMU 与 Metalio 的 PCF8563
// 都经 read_pico_pmu 的同一套协议暴露，所以这里只有日志措辞按板不同。
// Retain a valid RTC across soft reset; restore from the always-on clock after power loss.
// Both the Pico PMU and Metalio's PCF8563 come through the same read_pico_pmu protocol, so only
// the log wording differs per board.
static void restore_time_from_pmu(void) {
    struct timeval current;
    if (gettimeofday(&current, NULL) == 0 && current.tv_sec >= 1704067200 &&
        current.tv_sec <= 4102444799U) return;
    if (read_pico_pmu_cmd(PMU_CMD_TIME_GET, NULL, 0) != ESP_OK) return;
    const pmu_snapshot_t *pmu = read_pico_pmu_get();
    if (!pmu || !pmu->time_ok || !pmu->time_synced || pmu->unix_millis >= 1000 ||
        pmu->unix_sec < 1704067200 || pmu->unix_sec > 4102444799U) return;
    struct timeval tv = {.tv_sec = (time_t)pmu->unix_sec, .tv_usec = pmu->unix_millis * 1000};
    if (settimeofday(&tv, NULL) == 0) {
#if KIIKO_BOARD_METALIO47
        ESP_LOGI(TAG, "system clock restored from PCF8563");
#else
        ESP_LOGI(TAG, "system clock restored from PMU");
#endif
    }
}

extern const uint8_t lock_4bpp_bin_start[] asm("_binary_lock_4bpp_bin_start");
extern const uint8_t lock_4bpp_bin_end[] asm("_binary_lock_4bpp_bin_end");
extern const uint8_t loading_4bpp_bin_start[] asm("_binary_loading_4bpp_bin_start");
extern const uint8_t loading_4bpp_bin_end[] asm("_binary_loading_4bpp_bin_end");

#if KIIKO_BOARD_METALIO47
// VCOM 存本机 NVS（默认 3000mV，Moku 对比测试的取值）。V1.5 没有 Pico PMU，标定值只在本机。
// VCOM lives in local NVS (3000 mV default from Moku's comparison). V1.5 has no Pico PMU, so the
// calibrated value stays on this unit.
static bool resolve_vcom_at_boot(void) {
    const int mv = vcom_setup_load_mv();
    epd_set_vcom((uint16_t)mv);
    ESP_LOGI(TAG, "Metalio V1.5 runtime VCOM=%dmV (NVS, default 3000)", mv);
    return true;
}

// 开机按住音量键进调试/校准页；V1.5 音量键在 FCA9555 P1.0/P1.1，低有效，取两次防抖。
// Held volume keys (active low on FCA9555 P1.0/P1.1, sampled twice) open the debug/VCOM page.
static bool volume_key_held(void) {
    uint16_t first = 0xffff, again = 0xffff;
    if (metalio_io_read(&first) != ESP_OK) return false;
    vTaskDelay(pdMS_TO_TICKS(30));
    if (metalio_io_read(&again) != ESP_OK) return false;
    const bool pressed_first = ((first >> 8) & 0x03) != 0x03;
    const bool pressed_again = ((again >> 8) & 0x03) != 0x03;
    return pressed_first && pressed_again;
}
#else
// 已标定则开机读一次喂给 epdiy。未标定先用板级默认出开机图，随后拦住进标定页。
// Load VCOM once when set. Otherwise keep the board default for the splash,
// then gate into the setup page.
static bool resolve_vcom_at_boot(void) {
    int mv = 0;
    for (int i = 0; i < 3; i++) {
        if (read_pico_pmu_vcom_get(&mv) == ESP_OK) {
            epd_set_vcom((uint16_t)mv);
            ESP_LOGI(TAG, "panel VCOM loaded from PMU");
            return true;
        }
    }
    ESP_LOGW(TAG, "panel VCOM unset, keeping board default until setup");
    return false;
}
#endif

// 开机图和锁屏图是预先转好的 4bpp 全屏位图，尺寸对不上说明资源和面板不匹配。
// Splash and lock images are pre-baked 4bpp full-frames. A size mismatch
// means the assets do not match the panel.
static bool images_match_panel(void) {
    size_t lock_size = (size_t)(lock_4bpp_bin_end - lock_4bpp_bin_start);
    size_t loading_size = (size_t)(loading_4bpp_bin_end - loading_4bpp_bin_start);
    size_t expected = (size_t)epd_width() * epd_height() / 2;
    if (lock_size == expected && loading_size == expected) return true;
    ESP_LOGE(
        TAG, "Image size mismatch: lock %u loading %u expected %u",
        (unsigned)lock_size, (unsigned)loading_size, (unsigned)expected
    );
    return false;
}

// 缓存验证失败时继续原开机路径，不假设未知屏幕已经全白。
// A rejected cache keeps the original startup path; never assume an unknown panel is white.
static bool restore_retained_lock(EpdiyHighlevelState *hl,uint8_t *framebuffer) {
    read_pico_sd_info_t info = {0};
    for(unsigned wait=0;wait<150 && read_pico_sd_get_info(&info)==ESP_ERR_NOT_FINISHED;++wait)
        vTaskDelay(pdMS_TO_TICKS(20));
    // 超时未挂载不能继续进入卡上的缓存读取。/ Never enter cached-art I/O after a mount timeout.
    if(!info.mounted)return false;
    if(!lock_screen_restore(framebuffer))return false;
    memcpy(hl->back_fb,framebuffer,(size_t)epd_width()*epd_height()/2);
    return true;
}

// 深睡恢复会继续等双击解锁；缓存失效时也必须显示锁屏，不能停留在「准备资源」。
// Deep resumes still await double-press unlock; invalid art must show a lock face, never a loading splash.
static bool prepare_startup_frame(EpdiyHighlevelState *hl,uint8_t *framebuffer,bool resuming,bool vcom_ok) {
    bool restored=false;
    if(resuming && vcom_ok && !lock_pin_enabled() && pico_boot_resume_frame_cached())
        restored=restore_retained_lock(hl,framebuffer);
    if(restored)return true;
    epd_poweron();
    if(!resuming)read_pico_i2c_census_take();
    epd_clear();
    epd_hl_set_all_white(hl);
    if(resuming)ui_lock_fallback_draw(framebuffer);
    else ui_draw_full_image(framebuffer,loading_4bpp_bin_start);
    update_display_from_white(hl);
    return false;
}

void app_main(void) {
    // 共享 TF 卡期间复位后，先把内部 PHY 交回串口，再清理已用完的 BOOT 请求。
    // Reclaim Serial/JTAG after a reset during card sharing, then clear the served BOOT request.
    usb_storage_phy_init();
#if KIIKO_BOARD_METALIO47
    metalio_clear_download_request();
#else
    REG_CLR_BIT(RTC_CNTL_OPTION1_REG, RTC_CNTL_FORCE_DOWNLOAD_BOOT);
#endif

    const esp_reset_reason_t reset = esp_reset_reason();
    ESP_LOGI(TAG, "startup reset=%d stack=%u", (int)reset, (unsigned)uxTaskGetStackHighWaterMark(NULL));
    app_settings_init();
    // 在大缓冲和字体之前读取小型锁屏设置；读取失败不阻塞，之后的检查仍可重试。
    // Read the tiny lock setting before large buffers/fonts; keep failures nonblocking and let subsequent checks retry.
    (void)lock_pin_enabled();
    pico_boot_init(reset == ESP_RST_PANIC || reset == ESP_RST_INT_WDT ||
                   reset == ESP_RST_TASK_WDT || reset == ESP_RST_WDT);

    read_pico_handle_t hw;
    if (read_pico_init(&hw) != ESP_OK) return;
    rp_boot_mark("hw");
    if (hw.pmu_ready) restore_time_from_pmu();
    const bool vcom_ok = resolve_vcom_at_boot();
    rp_boot_mark("vcom");
#if KIIKO_BOARD_METALIO47
    ESP_LOGI(TAG, "TPS65185 last VCOM register readback=%dmV", metalio_tps_vcom_mv());
#endif
    pmu_selftest_bind(hw.sensor);
    // 两板都登记常开供电的触摸与加速度计；S31 侧是空实现，接口保持一致。
    // Both boards register the always-on touch/accelerometer; the S31 side is an empty stub so
    // the call site stays shared.
    app_sleep_bind(hw.touch, hw.sensor);

    if (!images_match_panel()) return;

    EpdiyHighlevelState hl = hw.hl;
    uint8_t* framebuffer = hw.framebuffer;

    // 深睡保留屏幕已有画面：验证缓存后同时恢复前后帧，省去清白和开机图两次推屏。
    // Deep sleep retains the panel image: validate its cache and restore both references, skipping white erase and splash output.
#if KIIKO_BOARD_METALIO47
    // S31 的软睡/浅睡也会留下恢复点，只有真深睡才复用面板画面。
    // The S31 leaves checkpoints after soft and light sleep too; only a real deep sleep keeps the frame.
    const bool resuming = pico_boot_has_resume() && reset == ESP_RST_DEEPSLEEP;
#else
    const bool resuming = pico_boot_has_resume();
#endif
    bool restored_lock=prepare_startup_frame(&hl,framebuffer,resuming,vcom_ok);
    rp_boot_mark("splash");
    // 直接续读不先加载整份系统字体；密码验证仍先使用所选系统字体。
    // Direct reader resume skips loading the whole system face first; PIN authentication still uses the selected UI face.
    if(!resuming || lock_pin_enabled())app_font_activate_system();
    else (void)ttf_font_open_builtin();
    rp_boot_mark("font");

    if (!hw.touch_ready) {
        ESP_LOGE(TAG, "No touch controller, UI cannot run");
#if KIIKO_BOARD_METALIO47
        // S31 上不关屏轨会一直耗电，触摸没了也没有别的出口。
        // Leaving the rails up on the S31 drains the pack, and with no touch there is no other exit.
        epd_poweroff();
#endif
        return;
    }

#if KIIKO_BOARD_METALIO47
    // V1.5 没有 Pico PMU：未标定时拦住进标定页；按住音量键开机进调试/校准页。
    // No Pico PMU on V1.5: gate into calibration when unset, and open the debug/calibration page
    // when a volume key is held while powering on.
    if (!vcom_ok) {
        vcom_setup_run(&hl, framebuffer, hw.touch);
        display_request_full_refresh();
    } else if (volume_key_held()) {
        debug_panel_run(&hl, framebuffer, hw.touch);
        display_request_full_refresh();
    }
#else
    if (hw.pmu_ready && !vcom_ok) {
        vcom_setup_run(&hl, framebuffer, hw.touch);
    }
#endif

    // 硬件、显示、触摸与必要的出厂设置均成功后确认新槽。
    // Confirm a new slot only after hardware, display, touch, and required setup succeed.
    pico_ota_confirm_running();

    // 单击开机：开机那一次短按就算开机，直接续读；双击开机仍停在锁屏等第二次。
    // One-press boot treats the powering short press as the boot; double-click boot still waits on the lock face.
    // S31 由 PMU 适配层从扩展口按键给出同一次短按。
    // The S31's PMU adapter reports the same short press from the expander keys.
    const bool boot_press = read_pico_pmu_take_boot_short_press();
    const bool boot_with_one_press = resuming && !lock_pin_enabled() && boot_press &&
        app_settings_wake_presses() == 1;
    if (boot_with_one_press) {
        ESP_LOGI(TAG, "resume lock cache=%d; one-press boot", restored_lock);
        pico_boot_ready();
        pico_boot_hold_resume();
    } else {
        if(resuming && !lock_pin_enabled())
            ESP_LOGI(TAG,"resume lock cache=%d; power presses=%u",restored_lock,
                (unsigned)app_settings_wake_presses());
        app_lock_boot_gate(&hl, hw.touch, hw.sensor);
    }
    rp_boot_mark("lock");
    const app_desc_t *first = app_home_page();
    pico_resume_t resume = {0};
    if (pico_boot_take_resume(&resume)) {
        if (!resume.reader) first = app_at(resume.tab);
        else {
            // 挂载等待有界；恢复入口已消费，无卡或资源失败不会反复自动开书。
            // Bound mount waiting after consuming the checkpoint; absent media or failed resources cannot auto-open in a loop.
            if (!strncmp(resume.path, "/sdcard/", 8)) {
                read_pico_sd_info_t info;
                for (unsigned wait = 0; wait < 150 && read_pico_sd_get_info(&info) == ESP_ERR_NOT_FINISHED; ++wait)
                    vTaskDelay(pdMS_TO_TICKS(20));
            }
            if (!strncmp(resume.path, "/flash/books/", 13) && !book_store_flash_ready()) {
                book_store_root_t roots[BOOK_STORE_ROOT_MAX]; int count;
                (void)book_store_roots(roots, &count);
            }
            struct stat st;
            if (!stat(resume.path, &st) && S_ISREG(st.st_mode) &&
                app_book_request_resume(resume.path, resume.fullscreen)) first = app_at(1);
        }
    }
    char interrupted[PICO_BOOT_PATH_MAX];
    if (pico_boot_interrupted_book(interrupted, sizeof(interrupted))) first = &app_boot_recovery;
    if(resuming && !lock_pin_enabled()) {
        if(first==app_at(1) && resume.reader)app_font_activate_reading();
        else app_font_activate_system();
    }
    rp_boot_mark("resume");
    app_loop_run(&(app_loop_config_t){
        .hl = &hl,
        .fb = framebuffer,
        .acc = hw.sensor,
        .tp = hw.touch,
        .sensor_ready = hw.sensor_ready,
        // 用户修订：深睡一次性恢复上次阅读或主页面；异常启动安全回首页。
        // User revision: resume the last reader/main tab once after deep sleep; abnormal starts safely enter Home.
        .first_app = first,
    });
}
