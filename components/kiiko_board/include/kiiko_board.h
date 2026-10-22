/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：板级选择层的公共头。共享代码只通过这里的宏与函数区分板型，不再直接写板级条件。
 * 选中的板级目录由 -DKIIKO_BOARD=<名字> 决定（boards/<名字>/），其 kiiko_board_config.h
 * 提供身份常量与能力宏，board_hw.c 提供两台板实现不同的板级服务。
 *
 * English: public header of the board selection layer. Shared code branches on the macros and
 * functions declared here instead of hard-coding a board. `-DKIIKO_BOARD=<name>` picks
 * boards/<name>/; its kiiko_board_config.h carries identity constants and capability macros, and
 * its board_hw.c implements the services the two boards do differently.
 *
 * 冻结：这里不放引脚号或时序常量——那些属于板级目录。也不要在这里 include 具体 BSP。
 * Frozen: no pin numbers or timing constants here; those belong to the board directory. Do not
 * include a concrete BSP from this header either.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"
#include "../../../main/kiiko_board_config.h"

/* ---- 能力宏兜底 / Capability fallbacks ---- */
/* 板级头没写的按「本板没有」处理，共享代码因此可以无条件 #if。 */
/* Anything a board header leaves out counts as absent, so shared code can always #if on it. */
#ifndef KIIKO_BOARD_HAS_FRONTLIGHT
#define KIIKO_BOARD_HAS_FRONTLIGHT 0
#endif
#ifndef KIIKO_BOARD_HAS_HAPTIC
#define KIIKO_BOARD_HAS_HAPTIC 0
#endif
#ifndef KIIKO_BOARD_HAS_VOLUME_KEYS
#define KIIKO_BOARD_HAS_VOLUME_KEYS 0
#endif
#ifndef KIIKO_BOARD_HAS_TPS65185
#define KIIKO_BOARD_HAS_TPS65185 0
#endif
#ifndef KIIKO_BOARD_HAS_USB_ROUTE
#define KIIKO_BOARD_HAS_USB_ROUTE 0
#endif
#ifndef KIIKO_BOARD_HAS_ANTI_FADE
#define KIIKO_BOARD_HAS_ANTI_FADE 0
#endif
#ifndef KIIKO_BOARD_HAS_COVER_CACHE
#define KIIKO_BOARD_HAS_COVER_CACHE 0
#endif
#ifndef KIIKO_BOARD_HAS_PMU
#define KIIKO_BOARD_HAS_PMU 0
#endif
#ifndef KIIKO_BOARD_VCOM_IN_NVS
#define KIIKO_BOARD_VCOM_IN_NVS 0
#endif
#ifndef KIIKO_BOARD_CACHE_DIR
#define KIIKO_BOARD_CACHE_DIR "/sdcard/.kiikoread"
#endif
/// 下载交接就是 USB 路由的对外说法，两个名字指向同一能力。
/// "USB download" is the outward name of the same capability as the USB route.
#define KIIKO_BOARD_HAS_USB_DOWNLOAD KIIKO_BOARD_HAS_USB_ROUTE


/* 板级宏必须是 0 或 1，写错了在编译期就断，而不是在真机上跑出奇怪行为。 */
/* Board macros are 0 or 1; a typo breaks the build instead of the device. */
#if (KIIKO_BOARD_HAS_FRONTLIGHT != 0 && KIIKO_BOARD_HAS_FRONTLIGHT != 1) || \
    (KIIKO_BOARD_HAS_HAPTIC != 0 && KIIKO_BOARD_HAS_HAPTIC != 1) ||         \
    (KIIKO_BOARD_HAS_VOLUME_KEYS != 0 && KIIKO_BOARD_HAS_VOLUME_KEYS != 1) || \
    (KIIKO_BOARD_HAS_TPS65185 != 0 && KIIKO_BOARD_HAS_TPS65185 != 1) ||     \
    (KIIKO_BOARD_HAS_USB_ROUTE != 0 && KIIKO_BOARD_HAS_USB_ROUTE != 1) ||   \
    (KIIKO_BOARD_HAS_ANTI_FADE != 0 && KIIKO_BOARD_HAS_ANTI_FADE != 1) ||   \
    (KIIKO_BOARD_HAS_COVER_CACHE != 0 && KIIKO_BOARD_HAS_COVER_CACHE != 1) || \
    (KIIKO_BOARD_HAS_PMU != 0 && KIIKO_BOARD_HAS_PMU != 1) ||               \
    (KIIKO_BOARD_VCOM_IN_NVS != 0 && KIIKO_BOARD_VCOM_IN_NVS != 1)
#error "KIIKO_BOARD_HAS_* / KIIKO_BOARD_VCOM_IN_NVS must be 0 or 1"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/// 板级编译身份，用于日志与诊断页；OTA 认板型靠 app_desc 的 project_name。
/// / Build-time board identity for logs and diagnostics; OTA identifies the board through the
/// / app description's project_name.
const char *kiiko_board_id(void);

/// 板级胶水初始化的最后一步：在通用 BSP（read_pico_init）之后补齐本板独有的外设。
/// 失败只让对应能力不可用，不应阻止面板与触摸起来。
/// / Last step of board bring-up: add the peripherals only this board has, after the shared
/// / read_pico_init(). A failure only disables that capability; it must not keep the panel or
/// / touch from coming up.
esp_err_t kiiko_board_extras_init(void);

#ifdef __cplusplus
}
#endif
