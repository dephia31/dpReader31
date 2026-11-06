/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include <stdbool.h>

/// 空闲时启动有界后台对时；主循环独占 PMU 与 NVS 的交接。
/// Start bounded background sync while idle; the main loop alone hands time to PMU/NVS.
void app_time_service_poll(bool idle);
