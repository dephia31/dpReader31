/* SPDX-License-Identifier: Apache-2.0
 * 中文：无需字体或帧副本的锁屏反馈。/ English: Lock feedback without fonts or frame copies.
 * 用户修订：调用者先推白屏，再局部画圆锁；不增加占位拼贴或帧缓存。
 * User revision: the caller presents white and then a local round lock, without placeholders or frame caches.
 */
#pragma once
#include <stdint.h>
#include "epdiy.h"

/// 画中央黑白圆形锁图标，返回局部范围；空画布或尺寸不足时不画。
/// Paint a centered monochrome lock badge and return its bounds; skip missing or undersized frames.
EpdRect ui_lock_feedback_draw(uint8_t *framebuffer);
/// 锁屏图不可用时，纯白底上绘制居中圆锁；不依赖资源文件、字体或TF卡。
/// Paint a centered round lock on white when art is unavailable, without resource files, fonts or SD.
void ui_lock_fallback_draw(uint8_t *framebuffer);
