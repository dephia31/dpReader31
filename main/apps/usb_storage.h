/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：在 TF 卡和电脑之间独占切换 USB 磁盘访问。
 * English: Switch exclusive SD access between firmware and USB host.
 */
#pragma once
#include <stdbool.h>
#include "esp_err.h"

esp_err_t usb_storage_start(void);
esp_err_t usb_storage_stop(void);
bool usb_storage_active(void);
bool usb_storage_connected(void);

/// 开机配置S31独立USJ和GPIO36物理通路，早于SD/板级初始化。
/// Configure S31 USJ and GPIO36 before SD/board initialization.
void usb_storage_phy_init(void);
