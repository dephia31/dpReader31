/* SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
 * 中文：字体文件的有界校验与提交；不接触字体槽或设置。
 * English: Bounded font validation and installation, without font slots or settings.
 */
#pragma once
#include "online_font.h"
typedef bool (*online_font_cancel_t)(void *ctx);
/// 源文件完整校验后提交字体及原许可证，失败只清除本次临时文件。
/// Verify source bytes before installing font/notices; failure removes only this attempt's temporary files.
esp_err_t online_font_store_commit(const online_font_item_t *item, const char *directory,
                                 const char *source, online_font_cancel_t cancelled, void *ctx);
/// 精确文件与许可证均存在；不加载整字库。/ Exact-sized file and notice exist, without loading a face.
bool online_font_store_installed(const online_font_item_t *item, const char *directory);
/// SFNT范围/TrueType表检查；不会运行字体栅格器。/ SFNT bounds/TrueType table checks without invoking a rasterizer.
esp_err_t online_font_store_validate(const char *path, uint32_t size);
