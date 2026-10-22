/* SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
 * 中文：旧接口的编译期兼容入口，能力和预算由所选板型集中定义。
 * English: Compile-time compatibility entry; the selected board owns capabilities and budgets.
 */
#pragma once
#ifndef KIIKO_BOARD_METALIO47
#define KIIKO_BOARD_METALIO47 0
#endif
#if KIIKO_BOARD_METALIO47 != 0 && KIIKO_BOARD_METALIO47 != 1
#error "KIIKO_BOARD_METALIO47 must be 0 or 1"
#endif
#if KIIKO_BOARD_METALIO47
#include "../boards/metalio47/kiiko_board_config.h"
#else
#include "../boards/pico/kiiko_board_config.h"
#endif
