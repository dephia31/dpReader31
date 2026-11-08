/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：记录实际阅读时长和翻页，并绘制票根锁屏。
 * English: Record actual reading time and turns, and paint the ticket lock face.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

esp_err_t book_ticket_record(uint32_t seconds, uint32_t turns);
/// 近 30 天（最早到今天）的实测阅读秒数；时钟无效时返回 false 且清零。
/// Measured seconds for the last 30 days, oldest to today; false and zeroed without a valid clock.
bool book_ticket_recent_days(uint32_t seconds[30]);
/// 当前月每日真实阅读秒数，兼容旧30日记录；无有效日期返回 false。/ Current-month measured seconds with legacy 30-day migration; false without a valid date.
bool book_ticket_month_days(int year, int month, uint32_t seconds[31]);
bool book_ticket_draw(uint8_t *fb, bool allow_cover_decode);

/// Draw the ticket layers using a borrowed, already loaded cover.
void book_ticket_draw_background(uint8_t *fb, const uint8_t *pixels, unsigned width, unsigned height, bool packed);
void book_ticket_draw_paper(uint8_t *fb);

/// 复用锁屏纸张阴影。/ Reuse the lock-screen paper shadow.
void book_ticket_draw_shadow(uint8_t *fb);
