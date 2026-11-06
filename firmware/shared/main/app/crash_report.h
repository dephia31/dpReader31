/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include <stdbool.h>
/* Capture previous-boot evidence before new boot breadcrumbs replace it. */
void crash_report_boot(void);
/* UI/boot owner only. false updates retained uptime without accessing SD. */
void crash_report_poll(bool allow_sd);
