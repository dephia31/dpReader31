// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <stdbool.h>

// 产品只读；本地诊断可做同机开关对照。/ Read-only in products; local diagnostics allow same-device comparisons.
bool epd_pipeline_enabled(void);
#ifdef KIIKO_LOCAL_LATENCY
void epd_pipeline_enable(bool enabled);
#endif
