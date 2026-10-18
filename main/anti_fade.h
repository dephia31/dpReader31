/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 强光下像素电荷缓慢泄漏，画面会逐渐变淡；静置时按固定间隔整屏重刷把画面推回去。
 * Under strong light the pixel charge leaks slowly and the image fades; re-drive the
 * whole frame at a fixed idle interval.
 */

#pragma once
#include <stdbool.h>
#include <stdint.h>

/// 防褪色重刷间隔（分钟），0 = 关闭；默认 5。值存 NVS（命名空间 metalio_hw）。
/// Anti-fade refresh interval in minutes (0 = off); default 5; stored in NVS.
unsigned anti_fade_minutes(void);

/// 设置间隔；只接受 0 / 1 / 3 / 5 / 10 / 30，其它值忽略。
/// Set the interval; only 0 / 1 / 3 / 5 / 10 / 30 are accepted.
void anti_fade_set_minutes(unsigned minutes);

/// 正常主循环调用：阻塞/唤醒/输入后重新等待完整间隔，不创建定时唤醒。
/// Poll from the normal loop; blocked states, wake and input defer a full interval without scheduling wakeups.
bool anti_fade_due(int64_t now_ms,int64_t last_input_ms,bool blocked);
