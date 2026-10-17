/* SPDX-License-Identifier: Apache-2.0
 * 中文：Metalio V1.5 硬件；总线、扩展口与电源轨由此独占。
 * English: Metalio V1.5 hardware owns the shared bus, expander and rails.
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "driver/i2c_master.h"
#include "fca9555.h"
#include "esp_err.h"

esp_err_t metalio_hw_init(void);
i2c_master_bus_handle_t metalio_bus(void);
fca9555_handle_t metalio_ioe(void);
esp_err_t metalio_read(uint8_t address, uint8_t reg, void *data, size_t size);
esp_err_t metalio_write(uint8_t address, uint8_t reg, const void *data, size_t size);
esp_err_t metalio_write8(uint8_t address, uint8_t reg, uint8_t data);
esp_err_t metalio_io_set(unsigned pin, bool high);
esp_err_t metalio_io_read(uint16_t *inputs);
/// 原厂休眠掩码：非按键中断隔离，仅 P17 电源键保留输入；失败后调用恢复。
/// Reference sleep mask: isolate non-key IRQs, keeping only P17 as an input; restore after a failed admission.
esp_err_t metalio_prepare_key_wake(void);
esp_err_t metalio_restore_key_io(void);
bool metalio_key_down(void);
int metalio_take_volume(void);
void metalio_volume_clear(void);
void metalio_volume_sample(uint16_t inputs,int64_t now_ms);
esp_err_t metalio_tps_on(int vcom_mv);
void metalio_tps_off(void);
bool metalio_tps_ready(void);
void metalio_frontlight(unsigned brightness, unsigned warmth);
unsigned metalio_brightness(void);
unsigned metalio_warmth(void);
bool metalio_frontlight_enabled(void);
#define METALIO_FRONTLIGHT_RANGE 240
unsigned metalio_frontlight_position(bool warmth);
void metalio_frontlight_preview_position(unsigned brightness, unsigned warmth);
void metalio_frontlight_restore(void);
void metalio_frontlight_save(unsigned brightness, unsigned warmth);
void metalio_frontlight_preview(unsigned brightness, unsigned warmth);
void metalio_frontlight_toggle(void);
void metalio_frontlight_commit(void);
bool metalio_haptic_enabled(void);
void metalio_haptic_set_enabled(bool enabled);
esp_err_t metalio_haptic_click(void);
void metalio_haptic_stop(void);
/// 失败关机恢复按键反馈。/ Resume feedback after a failed power-off.
void metalio_haptic_resume(void);
void metalio_hold_for_deep_sleep(void);
void metalio_sd_release_hold(void);
/// 官方浅睡引脚与电源轨事务；不触碰 GPIO36 下载通路。
/// Vendor light-sleep pin/rail transaction; retain the GPIO36 download route.
esp_err_t metalio_hold_for_light_sleep(void);
esp_err_t metalio_prepare_light_sleep_pins(void);
void metalio_release_light_sleep_pins(void);
esp_err_t metalio_sleep_cut_rails(uint16_t *saved);
esp_err_t metalio_sleep_restore_rails(uint16_t saved);
void metalio_ana_sw(bool high);
/// S31独立USJ与UTMI，以下仅恢复USJ及板上GPIO36通路。
/// S31 has separate USJ/UTMI PHYs; these recover only USJ and the board GPIO36 route.
/// 正常串口高保持；仅读卡显式释放到UTMI，失败须停止交接。
/// Retain the normal serial route; only explicit MSC may release it to UTMI.
esp_err_t metalio_usb_route_serial(bool serial);
void metalio_usb_serial_startup(void);
void metalio_usb_serial_reconnect(void);
/// 串口连接/外部VBUS/枚举宽限/充电器读取失败时阻止手动睡眠。
/// Block manual sleep for USJ, external VBUS, enumeration grace or an unknown charger state.
bool metalio_usb_blocks_sleep(void);
void metalio_clear_download_request(void);
/// 下载交接：确认USJ通路并单独保持SEL；准备失败不得重启。
/// Download handoff: verify USJ and retain SEL alone; never restart after a failed preparation.
esp_err_t metalio_download_prepare(void);
void metalio_download_cancel(void);
/// 断开板上USB通路500ms，按S31 HAL复位USJ并恢复驱动中断，再等待电脑进入ROM。
/// Disconnect the board USB route for 500ms, reset USJ with the S31 HAL, restore driver interrupts, then await host ROM entry.
esp_err_t metalio_request_download(void);
/// 只表示检测到USB SOF，不能据此宣称已进入ROM。/ USB SOF presence, not proof of ROM download.
bool metalio_download_connected(void);
/// 只读USB状态快照；供串口失效时写入TF诊断，不表示ROM握手成功。
/// Read-only USB snapshot for TF diagnostics when CDC is unavailable; not proof of ROM handshake.
void metalio_usb_describe(char *out,size_t size);
void metalio_poweroff(void);
