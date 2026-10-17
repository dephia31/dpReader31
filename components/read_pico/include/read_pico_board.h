/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：独立Metalio S31板级适配；read_pico前缀仅为RC90兼容接口。
 * English: Independent Metalio S31 hardware; read_pico names are RC90 API compatibility.
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "driver/i2c_master.h"
#include "epd_board.h"
#include "esp_err.h"
#include "fca9555.h"
#include "sy7636a.h"
#include "sdkconfig.h"

#ifdef __cplusplus
extern "C" {
#endif

#define READ_PICO_I2C_PORT ((i2c_port_t)CONFIG_READ_PICO_I2C_PORT)
#define READ_PICO_I2C_FREQ_HZ CONFIG_READ_PICO_I2C_FREQ_HZ
/// TCA9555与SC7A20H共享S31 GPIO2，低有效。/ Shared active-low S31 GPIO2.
#define READ_PICO_IOE_INT_GPIO CONFIG_READ_PICO_IOE_INT_GPIO
/// FT6336U独立GPIO5中断；复位经TCA P12。/ FT6336U IRQ on GPIO5, reset on TCA P12.
#define READ_PICO_TP_INT_GPIO CONFIG_READ_PICO_TP_INT_GPIO

#define READ_PICO_PRODUCT_NAME "kiikoread Metalio S31"
#define READ_PICO_DEVICE_NAME "Metalio S31"
/// RC90兼容轴映射常数，尚未对本S31实测校准；拿起检测使用相邻原始样本差。
/// Compatibility axis constants are not calibrated for this S31; pickup uses raw sample deltas.
#define READ_PICO_ACCEL_ZERO_X_MG 119
#define READ_PICO_ACCEL_ZERO_Y_MG 5
#define READ_PICO_ACCEL_ZERO_Z_MG 1005

extern const EpdBoardDefinition epd_board_read_pico;

i2c_master_bus_handle_t read_pico_i2c_bus(void);
sy7636a_handle_t read_pico_sy7636a(void);
fca9555_handle_t read_pico_fca9555(void);

typedef struct {
    uint16_t ioe_input;
    uint16_t ioe_output;
    uint16_t ioe_invert;
    uint16_t ioe_config;
    int ioe_int_level;
    sy7636a_status_t sy;
    bool sy_live;
    bool rails_on;
} read_pico_status_t;

#define READ_PICO_I2C_DEV_N 5

typedef struct {
    const char* name;
    uint8_t addr;
    bool online;
} read_pico_i2c_dev_t;

typedef struct {
    read_pico_i2c_dev_t dev[READ_PICO_I2C_DEV_N];
    int offline_n;
    bool all_online;
} read_pico_i2c_census_t;

/// Metalio常供电I2C器件探测；不访问不存在的SY7636A。/ Probe always-powered Metalio I2C devices.
void read_pico_i2c_census_take(void);
const read_pico_i2c_census_t* read_pico_i2c_census(void);

esp_err_t read_pico_get_status(read_pico_status_t* status);
/// 只读 IN/OUT + INT#，CFG/INV 走缓存。不唤醒 SY7636A。
/// / Read IN/OUT + INT# only; CFG/INV come from cache. Does not wake SY7636A.
esp_err_t read_pico_get_ioe_status(read_pico_status_t* status);
/// 连读 8 寄存器，刷新 CFG/INV 缓存。底栏「读取」和进页用。
/// / Sequential 8-register read; refreshes CFG/INV cache. Footer Read and page enter.
esp_err_t read_pico_get_ioe_status_full(read_pico_status_t* status);
int read_pico_ioe_int_level(void);
/// 读取低有效卡座 CD；通信失败时不改输出，调用方不得将失败解释为拔卡。
/// / Read active-low slot CD; leave output untouched on failure, which is not card removal.
esp_err_t read_pico_sd_detect(bool* present);
/// S31没有卡座CD；此兼容接口不能证明卡在位，挂载用卡通信确认。
/// No S31 card-detect pin; this compatibility hint cannot prove media presence.
bool read_pico_sd_present(void);
bool read_pico_rails_on(void);

/// 原Pico操作在S31返回NOT_SUPPORTED。/ Pico VCOMCTL operation is unsupported on S31.
esp_err_t read_pico_toggle_vcomctl(void);

/// 拉低再放开触摸芯片的 RST（挂在 FCA9555 上）。触摸进了深度睡眠后不再应答 I2C，
/// 只有这条路能把它叫回来。返回后调用方还要等芯片启动完成。
/// / Pulse the touch RST on the FCA9555. After deep sleep the chip ignores I2C;
/// this is the only wake. Caller must wait for boot after return.
esp_err_t read_pico_touch_reset(void);
/// high=false 拉低复位，high=true 放开。扩展口页要分两帧画脉冲时用。
/// / high=false asserts RST, high=true releases. IOE page draws the pulse in two frames.
esp_err_t read_pico_tp_rst(bool high);

/// 读一遍 9555 输入。INT# 不锁存，输入稳定后会自行松开；浅睡前/醒后都要调。
/// / Read 9555 inputs. INT# is not latched and releases once inputs are stable;
/// call before light sleep and after wake.
void read_pico_clear_ioe_int(void);

/// Restore the S31 LCD after a light-sleep bus hold.
void read_pico_epd_restore_lcd(void);
#ifdef __cplusplus
}
#endif

// 每轮扫描后释放S31直连XOE/MODE，不改变TPS电压或USB。/ Release direct S31 XOE/MODE after output; no TPS-voltage or USB change.
bool read_pico_epd_release_drive(void);
