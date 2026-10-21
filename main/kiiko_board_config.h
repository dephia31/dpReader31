/* SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
 * 中文：设备编译身份；选择在构建前完成，禁止由运行时猜测硬件或接受另一板型的升级包。
 * English: Device build identity is fixed before compilation; never guess hardware at runtime or accept another board's update.
 */
#pragma once
#ifndef KIIKO_BOARD_METALIO47
#define KIIKO_BOARD_METALIO47 0
#endif
#if KIIKO_BOARD_METALIO47 != 0 && KIIKO_BOARD_METALIO47 != 1
#error "KIIKO_BOARD_METALIO47 must be 0 or 1"
#endif
#if KIIKO_BOARD_METALIO47
#define KIIKO_BOARD_ID "metalio47"
#define KIIKO_BOARD_PROJECT "Kiikoread_Metalio_S31"
#define KIIKO_BOARD_IMAGE_CHIP_ID 0x0020
#define KIIKO_BOARD_SETTINGS_NAMESPACE "metalio_s31"
#define KIIKO_BOARD_LOCK_NAMESPACE "metalio_lock"
#define KIIKO_BOARD_STARTUP_NAMESPACE "metalio_start"
#else
#define KIIKO_BOARD_ID "pico"
#define KIIKO_BOARD_PROJECT "Read_Pico"
#define KIIKO_BOARD_IMAGE_CHIP_ID 0x0009
#define KIIKO_BOARD_SETTINGS_NAMESPACE "read_pico"
#define KIIKO_BOARD_LOCK_NAMESPACE "pico_lock"
#define KIIKO_BOARD_STARTUP_NAMESPACE "pico_startup"
#endif
