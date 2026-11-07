/* SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
 * Điểm tương thích lúc biên dịch cho giao diện cũ. Bo mạch được chọn định nghĩa các khả năng và ngân sách bộ nhớ.
 * English: Compile-time compatibility entry; the selected board owns capabilities and budgets.
 */
#pragma once
#ifndef DPREADER_BOARD_METALIO47
#define DPREADER_BOARD_METALIO47 0
#endif
#if DPREADER_BOARD_METALIO47 != 0 && DPREADER_BOARD_METALIO47 != 1
#error "DPREADER_BOARD_METALIO47 must be 0 or 1"
#endif
#if DPREADER_BOARD_METALIO47
#if __has_include("../boards/metalio47/dpreader_board_config.h")
#include "../boards/metalio47/dpreader_board_config.h"
#else
#include "../../../boards/metalio47/dpreader_board_config.h"
#endif
#else
#include "../boards/pico/dpreader_board_config.h"
#endif
