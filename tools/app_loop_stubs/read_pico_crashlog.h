/* SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0 */
#pragma once
/* 宿主测试不写RTC日志。/ Host tests do not write RTC breadcrumbs. */
static inline void rp_boot_mark(const char *phase) {(void)phase;}
