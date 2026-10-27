/* SPDX-License-Identifier: Apache-2.0
 * 中文：Metalio E-INK4 Plus V1.5 的编译身份与硬件能力。板型在构建前选定，禁止运行时猜硬件或接受另一板型的升级包。
 * English: Metalio E-INK4 Plus V1.5 build identity and hardware capabilities. The board is fixed
 * before compilation; never guess hardware at runtime or accept another board's update.
 *
 * 冻结：本文件只放常量与能力宏，不放代码；跨板共用逻辑一律按这里的宏或 components/kiiko_board 的接口分叉。
 * Frozen: constants and capability macros only. Shared code branches on these macros or on the
 * components/kiiko_board interface.
 */
#pragma once

#ifndef KIIKO_BOARD_METALIO47
#define KIIKO_BOARD_METALIO47 1
#endif
#if KIIKO_BOARD_METALIO47 != 0 && KIIKO_BOARD_METALIO47 != 1
#error "KIIKO_BOARD_METALIO47 must be 0 or 1"
#endif

/* ---- 身份 / Identity ---- */
#define KIIKO_BOARD_ID "metalio47"
#define KIIKO_BOARD_PROJECT "dpReader"
/// ESP32-S31 的 esp_image 头 chip_id（esp_chip_info.h 里 CHIP_ESP32S31 = 32）。
/// esp_image header chip id for the ESP32-S31 (CHIP_ESP32S31 = 32 in esp_chip_info.h).
#define KIIKO_BOARD_IMAGE_CHIP_ID 0x0020
/// NVS 命名空间沿用独立固件 test31～test36 的取值，升级后本机设置与密码继续有效。
/// The NVS namespaces keep the values used by the standalone test31-test36 firmware so settings
/// and the lock PIN survive the move to the shared tree.
#define KIIKO_BOARD_SETTINGS_NAMESPACE "metalio_s31"
#define KIIKO_BOARD_LOCK_NAMESPACE "metalio_lock"
#define KIIKO_BOARD_STARTUP_NAMESPACE "metalio_start"

/* ---- 面板 / Panel ---- */
/// 两块板同为 1216x684 单色面板，尺寸一致；驱动路径不同。
/// Both boards carry the same 1216x684 mono panel; only the driving path differs.
#define KIIKO_BOARD_PANEL_WIDTH 1216
#define KIIKO_BOARD_PANEL_HEIGHT 684
/// VCOM 标定值存本机 NVS；默认 1200mV 与厂家固件（CloudZao/Metalio-E-INK4-Plus）一致，
/// 也是唯一在出厂硬件上验证过的起点。标定窗口 900..2000mV。
/// The calibrated VCOM lives in local NVS; the 1200 mV default matches the vendor
/// firmware and is the only value verified on shipping hardware. Calibration
/// window 900..2000 mV.
#define KIIKO_BOARD_VCOM_IN_NVS 1
#define KIIKO_BOARD_VCOM_DEFAULT_MV 1700

/* ---- 硬件能力 / Hardware capabilities ---- */
/// Front light (brightness + warmth) driven over I2C; haptics through a GPIO-driven motor.
/// 前光（亮度 + 色温）与振动马达。
#define KIIKO_BOARD_HAS_FRONTLIGHT 1
#define KIIKO_BOARD_HAS_HAPTIC 1
/// FCA9555 P1.0/P1.1 volume keys./ 侧边音量键。
#define KIIKO_BOARD_HAS_VOLUME_KEYS 1
/// TPS65185 rails and VCOM readback./ TPS65185 电源轨与 VCOM 回读。
#define KIIKO_BOARD_HAS_TPS65185 1
/// GPIO36 USJ/UTMI switch and the download handoff./ 板上 USJ/UTMI 切换与下载交接。
#define KIIKO_BOARD_HAS_USB_ROUTE 1
/// Anti fade (timed full refresh) and the PSRAM cover cache./ 防淡墨与 PSRAM 封面缓存。
#define KIIKO_BOARD_HAS_ANTI_FADE 1
#define KIIKO_BOARD_HAS_COVER_CACHE 1
#define KIIKO_BOARD_CACHE_DIR "/sdcard/.metalio_s31"
/// No CW32L010 PMU on this board: time comes from the PCF8563, keys from the expander.
/// 本板没有 CW32L010 PMU：时间来自 PCF8563，按键来自扩展口。
#define KIIKO_BOARD_HAS_PMU 0
/// Lock-face standby policy (light/deep/off) is a product setting here.
/// 锁屏待机方式（浅睡 / 深睡 / 关机）在本板是产品设置项。
#define KIIKO_BOARD_HAS_LOCK_SLEEP_POLICY 1
/// Reader turn effect adds a fast monochrome mode on this panel.
/// 本板面板另提供一档速读黑白翻页效果。
#define KIIKO_BOARD_READER_TURN_EFFECT_MAX 2

/* ---- 备份与设备名 / Backup and device name ---- */
/// Values kept byte-identical to the standalone test31-test36 firmware so existing backups
/// and the default device name keep matching.
/// 与独立固件 test31～test36 保持逐字节一致，已有备份与默认设备名继续可用。
#define KIIKO_BOARD_BACKUP_PREFIX "dpReader-settings"
#define KIIKO_BOARD_PRODUCT_NAME "dpReader"

/* ---- 显示策略 / Display policy ---- */
/// One epd_clear_checked baseline instead of the Pico blind-repair cycles.
/// 用一次 epd_clear_checked 白底基线，不做 Pico 那种盲刷多轮。
#define KIIKO_BOARD_HAS_PANEL_BLIND_REPAIR 0
/// Power the rails down right after every output instead of an idle timeout
/// (the 2e1e4e2 fix: white turns grey under strong light if the rails stay up).
/// 每次输出后立即断电，不做空闲延时（对应修复：屏轨常上电会让白底在强光下发灰）。
#define KIIKO_BOARD_RAILS_IDLE_TIMEOUT_MS 0
/// Vendor 2.0.76 full GL48 waveform, untrimmed.
/// 厂家 2.0.76 完整 GL48 波形，不裁剪。
#define KIIKO_BOARD_WAVEFORM_TRIM_GL48 0
/// No extra phase reordering for reader text; equal-value hold comes from the derived tables.
/// 文字翻页不做额外相序调序，等值保持交给派生表。
#define KIIKO_BOARD_READER_TEXT_PHASE_TUNE 0
/// 不对未变化白底补推，沿用已验证的差分目标。/ Preserve validated differential targets without a white-background tick.
#define KIIKO_BOARD_PANEL_WHITE_TICK 0

/* ---- 缓存与内存预算 / Cache and memory budgets ---- */
#define KIIKO_BOARD_COVER_CACHE_DIR KIIKO_BOARD_CACHE_DIR "/covers"
/// RAM cover cache lives in the S31's PSRAM; the standalone firmware put it in the 4 MiB
/// budget with 96 slots, and trims 512 KiB before opening a book.
/// RAM 封面缓存放在 S31 的 PSRAM：沿用独立固件的 4 MiB 预算与 96 槽，开书前回收 512 KiB。
#define KIIKO_BOARD_RAM_COVER_CACHE 1
#define KIIKO_BOARD_COVER_RAM_SLOTS 96
#define KIIKO_BOARD_COVER_RAM_BUDGET_KB 4096
#define KIIKO_BOARD_CACHE_PSRAM_RESERVE_KB 2560
#define KIIKO_BOARD_CACHE_BLOCK_RESERVE_KB 1024
/// 字体缓存 1 MiB：这些预算按 PSRAM 余量设定，实际可用量在开机日志里记录。
/// 各缓存都按需分配且分配前查剩余量，所以预算之和不等于常驻占用。
/// TTF cache 1 MiB: these budgets follow the PSRAM headroom, and boot logs the
/// actual free size. Every cache allocates lazily after checking free space, so
/// the budgets do not sum to resident usage.
#define KIIKO_BOARD_TTF_CACHE_LIMIT_KB 1024
#define KIIKO_BOARD_TTF_CACHE_MID_KB 768
#define KIIKO_BOARD_TTF_CACHE_PRESSURE_KB 320
/// Four page-cache slots instead of two, so the reader can pre-render both neighbours.
/// 页缓存四槽而不是两槽，阅读页可以同时预渲染前后页。
#define KIIKO_BOARD_PAGE_CACHE_SLOTS 4
#define KIIKO_BOARD_PAGE_CACHE_MAX_KB 1024
/// ZIP prefetch on the S31 only; it needs the PSRAM headroom.
/// 只有 S31 开 ZIP 预取，它有余量。
#define KIIKO_BOARD_ZIP_PREFETCH_KB 512

/* ---- 书库 NVS 命名空间 / Shelf NVS namespaces ---- */
/// Kept byte-identical to the standalone test31-test36 firmware; renaming would clear the
/// user's shelf, favorites and bookmarks.
/// 与独立固件 test31～test36 逐字节一致；改名等于清空用户书架、收藏与书签。
#define KIIKO_BOARD_FAVORITES_NS "mt_favs"
#define KIIKO_BOARD_SHELF_NS "mt_shelf"
#define KIIKO_BOARD_MARKS_NS "mt_marks"
