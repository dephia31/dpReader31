/* SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0 */
#pragma once
#include "esp_err.h"
/// 仅在用户二次确认后调用；保留书籍、阅读记录、硬件校准和分区。/ Confirmed reset of preferences/credentials, retaining books, history and calibration.
esp_err_t app_factory_reset(void);
