/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * Header công khai của lớp chọn bo. Mã dùng chung phân biệt bo bằng các macro và hàm ở đây.
 * Tham số -DDPREADER_BOARD=<tên> chọn thư mục boards/<tên>. Header dpreader_board_config.h
 * định nghĩa nhận diện và khả năng của bo. board_hw.c triển khai dịch vụ phần cứng riêng cho từng bo.
 *
 * English: public header of the board selection layer. Shared code branches on the macros and
 * functions declared here instead of hard-coding a board. `-DDPREADER_BOARD=<name>` picks
 * boards/<name>/; its dpreader_board_config.h carries identity constants and capability macros, and
 * its board_hw.c implements the services the two boards do differently.
 *
 * Quy tắc cố định: đặt sơ đồ chân và thời gian trong thư mục bo. Header này không include BSP cụ thể.
 * Frozen: no pin numbers or timing constants here; those belong to the board directory. Do not
 * include a concrete BSP from this header either.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "../../../main/dpreader_board_config.h"

/* ---- Giá trị mặc định cho macro khả năng ---- */
/* Khả năng không khai báo trong header bo được xem là không có, nên mã dùng chung có thể kiểm tra trực tiếp bằng #if. */
/* Anything a board header leaves out counts as absent, so shared code can always #if on it. */
#ifndef DPREADER_BOARD_HAS_FRONTLIGHT
#define DPREADER_BOARD_HAS_FRONTLIGHT 0
#endif
#ifndef DPREADER_BOARD_HAS_HAPTIC
#define DPREADER_BOARD_HAS_HAPTIC 0
#endif
#ifndef DPREADER_BOARD_HAS_VOLUME_KEYS
#define DPREADER_BOARD_HAS_VOLUME_KEYS 0
#endif
#ifndef DPREADER_BOARD_HAS_TPS65185
#define DPREADER_BOARD_HAS_TPS65185 0
#endif
#ifndef DPREADER_BOARD_HAS_USB_ROUTE
#define DPREADER_BOARD_HAS_USB_ROUTE 0
#endif
#ifndef DPREADER_BOARD_HAS_ANTI_FADE
#define DPREADER_BOARD_HAS_ANTI_FADE 0
#endif
#ifndef DPREADER_BOARD_HAS_COVER_CACHE
#define DPREADER_BOARD_HAS_COVER_CACHE 0
#endif
#ifndef DPREADER_BOARD_HAS_PMU
#define DPREADER_BOARD_HAS_PMU 0
#endif
#ifndef DPREADER_BOARD_VCOM_IN_NVS
#define DPREADER_BOARD_VCOM_IN_NVS 0
#endif
#ifndef DPREADER_BOARD_CACHE_DIR
#define DPREADER_BOARD_CACHE_DIR "/sdcard/.kiikoread"
#endif
/// Bàn giao chế độ tải là tên gọi bên ngoài của chức năng chuyển tuyến USB.
/// "USB download" is the outward name of the same capability as the USB route.
#define DPREADER_BOARD_HAS_USB_DOWNLOAD DPREADER_BOARD_HAS_USB_ROUTE


/* Macro bo chỉ nhận 0 hoặc 1. Giá trị sai phải làm build thất bại để tránh lỗi trên thiết bị. */
/* Board macros are 0 or 1; a typo breaks the build instead of the device. */
#if (DPREADER_BOARD_HAS_FRONTLIGHT != 0 && DPREADER_BOARD_HAS_FRONTLIGHT != 1) || \
    (DPREADER_BOARD_HAS_HAPTIC != 0 && DPREADER_BOARD_HAS_HAPTIC != 1) ||         \
    (DPREADER_BOARD_HAS_VOLUME_KEYS != 0 && DPREADER_BOARD_HAS_VOLUME_KEYS != 1) || \
    (DPREADER_BOARD_HAS_TPS65185 != 0 && DPREADER_BOARD_HAS_TPS65185 != 1) ||     \
    (DPREADER_BOARD_HAS_USB_ROUTE != 0 && DPREADER_BOARD_HAS_USB_ROUTE != 1) ||   \
    (DPREADER_BOARD_HAS_ANTI_FADE != 0 && DPREADER_BOARD_HAS_ANTI_FADE != 1) ||   \
    (DPREADER_BOARD_HAS_COVER_CACHE != 0 && DPREADER_BOARD_HAS_COVER_CACHE != 1) || \
    (DPREADER_BOARD_HAS_PMU != 0 && DPREADER_BOARD_HAS_PMU != 1) ||               \
    (DPREADER_BOARD_VCOM_IN_NVS != 0 && DPREADER_BOARD_VCOM_IN_NVS != 1)
#error "DPREADER_BOARD_HAS_* / DPREADER_BOARD_VCOM_IN_NVS must be 0 or 1"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/// Định danh bo phục vụ nhật ký và chẩn đoán. OTA dùng project_name của app_desc để nhận diện.
/// / Build-time board identity for logs and diagnostics; OTA identifies the board through the
/// / app description's project_name.
const char *dpreader_board_id(void);

/// Khởi tạo các thiết bị ngoại vi riêng của bo sau BSP chung read_pico_init.
/// Khi khởi tạo thất bại, vô hiệu hóa khả năng tương ứng và vẫn khởi động màn hình cùng cảm ứng.
/// / Last step of board bring-up: add the peripherals only this board has, after the shared
/// / read_pico_init(). A failure only disables that capability; it must not keep the panel or
/// / touch from coming up.
esp_err_t dpreader_board_extras_init(void);

#ifdef __cplusplus
}
#endif
