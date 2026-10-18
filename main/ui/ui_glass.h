/* SPDX-License-Identifier: Apache-2.0
 * 中文：关机、密码锁屏及顶部快捷层共用的有界液态玻璃绘制。/ English: Bounded liquid glass for power, PIN and top quick controls.
 */
#pragma once
#include <stdbool.h>
#include "epdiy.h"
/// 从已有画面取样；white为白层百分比、refract为折射百分比。失败不改画面，调用方画实心回退。
/// Sample the existing frame; white/refract are percentages. Failure leaves pixels intact for a solid fallback.
bool ui_glass_panel(uint8_t *fb,EpdRect rect,int radius,int white,int refract);
