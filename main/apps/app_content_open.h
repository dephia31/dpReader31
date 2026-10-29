/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：文件管理与首页请求内容页打开指定文件。
 * English: File manager and home request a specific file in the content pages.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>

bool app_book_request_open(const char *path);
bool app_book_request_open_from_home(const char *path);
/// 按保存位置恢复；保留参数以兼容旧睡眠记录，忽略旧全屏值。/ Resume at the saved position; retain but ignore the legacy full-screen argument.
bool app_book_request_resume(const char *path, bool fullscreen);
/// 仅导出已打开阅读上下文。/ Export only an already open reader context.
bool app_book_resume_context(char *path, size_t capacity, bool *fullscreen);
bool app_image_request_open(const char *path);
void app_files_request_folder(int folder);
void app_book_request_manage(void);
void app_book_request_opds(void);
bool app_book_reader_body_visible(void);
#if KIIKO_BOARD_METALIO47
#include "app/app.h"
#include "settings.h"
/// 共用看图入口，path可空以显示列表；透明书封只收PNG。/ Shared viewer entry; an empty path opens its list, while transparent covers accept PNG only.
bool app_image_request_lock_picker(const char *path, bool transparent);
/// 从专用看图列表返回锁屏样式，跳转主页时不保留此请求。/ Return from the scoped viewer list to lock styles; home navigation does not queue this.
void app_device_settings_resume_lock_style(void);
/// 阅读音量键固定翻页；非阅读返回false，由原页面处理。/ Fixed volume turns in the reader; false delegates non-reader views to their existing keys.
bool app_book_volume_key(app_ctx_t *ctx, int direction, app_redraw_t *redraw);
app_redraw_t app_book_handle_key_action(app_ctx_t *ctx, app_reader_key_action_t action);
void app_book_request_kosync(app_ctx_t *ctx);
/// 仅当前正文允许AI书签，复用原书签保存/提示。/ AI bookmark only on the visible reader body, using existing persistence/notices.
bool app_book_ai_bookmark(void);
#endif
/// 模式/对比度变化时释放旧格式封面；绘制前由主循环一次调用。/ Drop obsolete cover formats once on mode/contrast changes, before drawing.
void app_book_cover_mode_changed(void);
void app_home_cover_mode_changed(void);
/// 开书前释放可重建的表盘底图，让解析器使用这部分 PSRAM。/ Release rebuildable clock bases before opening a book so parsing can use that PSRAM.
void app_home_release_clock_cache(void);
