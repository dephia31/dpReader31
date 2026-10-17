/* SPDX-License-Identifier: Apache-2.0
 * 中文：按Metalio power_hw.cc恢复S31 USJ；不使用S3共享PHY切换。
 * English: Recover S31 USJ following Metalio power_hw.cc, without S3 shared-PHY switching.
 */
#include "metalio_hw.h"
#include "driver/gpio.h"
#include "driver/usb_serial_jtag.h"
#include "hal/usb_serial_jtag_ll.h"
#include "esp_timer.h"
#include "esp_log.h"
#include <stdio.h>
#include "esp_efuse.h"
#include "esp_efuse_table.h"
#include "soc/usb_serial_jtag_struct.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static portMUX_TYPE usb_guard_lock=portMUX_INITIALIZER_UNLOCKED;
static int64_t usb_guard_until;
static bool download_prepared;
static unsigned download_recoveries;
static void serial_commit(void) {
    // S31寄存器契约：APB侧配置须显式提交到48MHz USB时钟域。
    // S31 register contract: explicitly latch APB configuration into the 48MHz USB domain.
    USB_SERIAL_JTAG.config_update.config_update = 1;
}
static void serial_enable(void) {
    usb_serial_jtag_ll_enable_bus_clock(true);
    // S31寄存器的存储供电、时钟和主机复位使能；仅开pad不足以恢复这些状态。
    // S31 memory power/clock and host-reset enable: restoring pads alone does not restore these.
    USB_SERIAL_JTAG.mem_conf.usb_mem_pd = 0;
    USB_SERIAL_JTAG.mem_conf.usb_mem_clk_en = 1;
    USB_SERIAL_JTAG.chip_rst.usb_uart_chip_rst_dis = 0;
    serial_commit();
}
static void enumeration_grace(void) {
    const int64_t until=esp_timer_get_time()+2000000;
    portENTER_CRITICAL(&usb_guard_lock);
    usb_guard_until=until;
    portEXIT_CRITICAL(&usb_guard_lock);
}
esp_err_t metalio_usb_route_serial(bool serial) {
    // 首次配置须先建立SEL锁存/方向，再解除复位前保持；正常高电平跨主机复位保留。
    // Configure SEL before releasing a reset-retained pad; retain high across host resets.
    if (serial) metalio_ana_sw(true);
    esp_err_t err=gpio_hold_dis(GPIO_NUM_36);
    if (err!=ESP_OK) return err;
    metalio_ana_sw(serial);
    if ((gpio_get_level(GPIO_NUM_36)!=0)!=serial) return ESP_ERR_INVALID_STATE;
    return serial?gpio_hold_en(GPIO_NUM_36):ESP_OK;
}
static esp_err_t serial_recover(void) {
    esp_err_t err=metalio_usb_route_serial(false);
    if (err!=ESP_OK) return err;
    // SD/CNNT恢复也可能清空USJ状态；统一使用本板500ms断开和S31原生控制器恢复。
    // SD/CNNT restoration can invalidate USJ too; use the same bounded board disconnect and native recovery.
    vTaskDelay(pdMS_TO_TICKS(500));
    const bool driver_installed=usb_serial_jtag_is_driver_installed();
    portENTER_CRITICAL(&usb_guard_lock);
    usb_serial_jtag_ll_enable_bus_clock(true);
    uint32_t interrupts=usb_serial_jtag_ll_get_intr_ena_status();
    usb_serial_jtag_ll_disable_intr_mask(USB_SERIAL_JTAG_LL_INTR_MASK);
    usb_serial_jtag_ll_reset_register();
    serial_enable();
    usb_serial_jtag_ll_phy_set_defaults();
    usb_serial_jtag_ll_phy_enable_pad(true);
    serial_commit();
    if (driver_installed) {
        interrupts|=USB_SERIAL_JTAG_INTR_SERIAL_OUT_RECV_PKT|USB_SERIAL_JTAG_INTR_SERIAL_IN_EMPTY;
    } else {
        interrupts&=~(USB_SERIAL_JTAG_INTR_SERIAL_OUT_RECV_PKT|USB_SERIAL_JTAG_INTR_SERIAL_IN_EMPTY);
    }
    usb_serial_jtag_ll_ena_intr_mask(interrupts&USB_SERIAL_JTAG_LL_INTR_MASK);
    ++download_recoveries;
    portEXIT_CRITICAL(&usb_guard_lock);
    err=metalio_usb_route_serial(true);
    if (err==ESP_OK) enumeration_grace();
    return err;
}
void metalio_usb_serial_startup(void) {
    esp_err_t err=metalio_usb_route_serial(true);
    serial_enable();
    usb_serial_jtag_ll_phy_set_defaults();
    usb_serial_jtag_ll_phy_enable_pad(true);
    serial_commit();
    enumeration_grace();
    if (err!=ESP_OK) ESP_LOGW("metalio_usb","Cannot retain normal USB download route: %d",(int)err);
}
void metalio_usb_serial_reconnect(void) {
    // 仅在SD恢复/真实唤醒边界恢复一次；常规阅读不轮询重置，也不抢走MSC通路。
    // Recover once at SD/wake boundaries; never periodically reset a live connection or steal MSC.
    if (!gpio_get_level(GPIO_NUM_36)) return;
    esp_err_t err=serial_recover();
    if (err!=ESP_OK) {
        (void)metalio_usb_route_serial(true);
        ESP_LOGW("metalio_usb","USJ restore failed: %d",(int)err);
    } else {
        ESP_LOGI("metalio_usb","Normal USJ download route retained after SD/wake");
    }
}
esp_err_t metalio_download_prepare(void) {
    // 只读安全限制，不写任何eFuse。/ Read download restrictions; never program eFuses.
    if (download_prepared) return ESP_OK;
    if (esp_efuse_read_field_bit(ESP_EFUSE_DIS_DOWNLOAD_MODE) ||
        esp_efuse_read_field_bit(ESP_EFUSE_DIS_USB_SERIAL_JTAG_DOWNLOAD_MODE)) return ESP_ERR_NOT_SUPPORTED;
    // 已在OTG/MSC通路时拒绝，不得抢走电脑正在使用的TF。
    // Refuse an OTG/MSC route instead of stealing a card currently exposed to the host.
    if (!gpio_get_level(GPIO_NUM_36)) return ESP_ERR_INVALID_STATE;
    esp_err_t err=metalio_usb_route_serial(true);
    download_prepared=err==ESP_OK;
    return err;
}
void metalio_download_cancel(void) {
    if (download_prepared) (void)metalio_usb_route_serial(true);
    // 退出手动恢复页只结束会话，正常串口及SEL高保持继续有效。
    // Leaving the manual recovery page ends the session; normal USB and retained SEL remain available.
    download_prepared=false;
}
esp_err_t metalio_request_download(void) {
    if (!download_prepared) return ESP_ERR_INVALID_STATE;
    return serial_recover();
}
bool metalio_download_connected(void) {
    return download_prepared && usb_serial_jtag_is_connected();
}
void metalio_usb_describe(char *out,size_t size) {
    if (!out || !size) return;
    snprintf(out,size,"SEL=%d clock=%d conf=%08lx mem=%08lx reset=%08lx fifo=%08lx sof=%d prepared=%d dl_disabled=%d usj_dl_disabled=%d driver=%d irq=%08lx recoveries=%u",
        gpio_get_level(GPIO_NUM_36),usb_serial_jtag_ll_module_is_enabled(),
        (unsigned long)USB_SERIAL_JTAG.conf0.val,(unsigned long)USB_SERIAL_JTAG.mem_conf.val,
        (unsigned long)USB_SERIAL_JTAG.chip_rst.val,(unsigned long)USB_SERIAL_JTAG.ser_afifo_config.val,
        usb_serial_jtag_is_connected(),download_prepared,
        esp_efuse_read_field_bit(ESP_EFUSE_DIS_DOWNLOAD_MODE),
        esp_efuse_read_field_bit(ESP_EFUSE_DIS_USB_SERIAL_JTAG_DOWNLOAD_MODE),
        usb_serial_jtag_is_driver_installed(),(unsigned long)usb_serial_jtag_ll_get_intr_ena_status(),download_recoveries);
}

bool metalio_usb_blocks_sleep(void) {
    if (usb_serial_jtag_is_connected()) {
        enumeration_grace();
        return true;
    }
    // CX25601N参考代码：0x1E低3位为VBUS来源，0无输入、7为本机OTG输出。
    // Reference CX25601N: status 0x1E bits 0..2 identify VBUS; 0 absent, 7 local OTG output.
    uint8_t status;
    if (metalio_read(0x6b,0x1e,&status,1)!=ESP_OK) return true;
    const unsigned source=status&7;
    if (source!=0 && source!=7) {
        enumeration_grace();
        return true;
    }
    portENTER_CRITICAL(&usb_guard_lock);
    const int64_t until=usb_guard_until;
    portEXIT_CRITICAL(&usb_guard_lock);
    return esp_timer_get_time()<until;
}
