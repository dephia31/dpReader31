/* 中文：调度测试替身。/ English: Scheduler test shim. */
#pragma once
#include "common.h"

bool app_settings_ble_turner(void);
void app_settings_set_ble_turner(bool enabled);

uint8_t app_settings_auto_lock_minutes(void);

uint8_t app_settings_system_contrast(void);
