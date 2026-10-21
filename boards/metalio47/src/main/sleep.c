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
 */

#include "sleep.h"
#include "ui_avatar.h"
#include "book_cover_jobs.h"
#include "metalio_hw.h"
#include "ble_page_turner.h"
#include "driver/usb_serial_jtag.h"
#include "ttf_font.h"
#include "output_lcd/lcd_driver.h"
#include "boot_state.h"
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
#include "book_source.h"
#include "book_lock_collage.h"
#include "book_lock_bookmark.h"
#include "book_ticket.h"
#include "display.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_system.h"
#include "esp_app_desc.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pmu_selftest.h"
#include "read_pico_board.h"
#include "read_pico_pmu.h"
#include "read_pico_transfer.h"
#include "sc7a20h_lab.h"
#include "settings.h"
#include "ui_kit.h"
#include "ui_image_dither.h"
#include "ui_wallpaper.h"
#include "ui_lock_feedback.h"
#include "e0470_epaper_waveform.h"
#include "ota_online.h"

static const char* TAG = "read_pico";
static bool s_lock_feedback_started;
static bool s_lock_deep_ready;
static bool s_lock_frame_cached;
static cst836u_handle_t s_lock_sleep_touch;
static esp_err_t lock_light_sleep_once(sc7a20h_handle_t acc, cst836u_handle_t touch, bool *key_wake);
void app_lock_set_resume_ready(bool ready) { s_lock_deep_ready = ready; }



extern const uint8_t lock_4bpp_bin_start[] asm("_binary_lock_4bpp_bin_start");

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

static bool draw_wallpaper(uint8_t *framebuffer) {
    return app_settings_lock_style() == 1 &&
        ui_wallpaper_prepare_lock(framebuffer, app_settings_wallpaper_path());
}

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

app_wake_source_t app_last_wake_source(void) {
    return (app_wake_source_t)app_settings_last_wake();
}

// 中文：默认保留CPU/PSRAM/TF；可选硬件浅睡使用下方完整外设事务。
// USB/拿起唤醒或事务失败时保留待机；任何定时唤醒都不解锁、不进入深睡。
// English: Default retains CPU/PSRAM/TF; optional light sleep uses the complete transaction below.
// USB/pickup/failure retain standby; timer maintenance never unlocks or escalates to deep sleep.
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

app_wake_source_t app_light_sleep_wait(sc7a20h_handle_t acc) {
    return app_light_sleep_wait_timed(acc, 0);
}

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
    bool key_irq=false;
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
        key_irq=err==ESP_OK && (esp_sleep_get_wakeup_causes() & (1u<<ESP_SLEEP_WAKEUP_EXT1));
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
    // GPIO仅唤醒CPU，双击才解锁；未完成时回到保留待机收齐按键。/ GPIO wakes only the CPU; retain standby until the sampled double click completes.
    if(key_irq){
        if(read_pico_pmu_take_key_wakeup())*key_wake=true;
        else if(err==ESP_OK)err=ESP_ERR_INVALID_STATE;
    }
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

void app_restart_host(void) {
    (void)app_sync_time_to_pmu();
    ttf_font_open_builtin(); (void)read_pico_sd_sync();
    metalio_haptic_stop(); metalio_frontlight(0,50); epd_poweroff(); esp_restart();
}

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

static void lock_wait_for_unlock(sc7a20h_handle_t acc) {
    (void)acc;
    for(;;) {
        app_wake_source_t wake=app_light_sleep_wait_timed(NULL,APP_LOCK_IDLE_TIMEOUT_MS);
        if(wake==APP_WAKE_KEY || wake==APP_WAKE_PICKUP) return;
        // 超时不能触发重启或解锁；稳定性版继续保留当前锁屏内存。
        // A timeout cannot reboot or unlock; retain the current lock context in this stability build.
    }
}

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

void app_lock_boot_gate(EpdiyHighlevelState *hl, cst836u_handle_t touch, sc7a20h_handle_t acc) {
    if (!lock_pin_enabled()) {
        if(pico_boot_has_resume()) {
            pico_boot_ready();pico_boot_hold_resume();
            lock_wait_for_unlock(NULL);
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
        if (!drawn) ui_lock_fallback_draw(fb);
    }
    lock_screen_authenticate(hl, touch, acc, true);
    app_font_end_lock(&saved);
}
