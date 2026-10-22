/* SPDX-License-Identifier: Apache-2.0
 * 中文：从已解析正文缓存完整短句，不开书或改进度。/ English: Cache a complete sentence from already parsed text without opening books or changing progress.
 */
#pragma once
#include "html_text.h"
#define BOOK_EXCERPT_CAP 240
/// 仅读取匹配已保存进度章节的书摘；旧版及其他章节不回放。/ Read only the saved chapter’s excerpt, never a legacy or different-chapter quote.
bool book_excerpt_load(const char *path, uint32_t chapter, char out[BOOK_EXCERPT_CAP]);
/// 仅接受已成功切入的章节正文；后台预取不调用。/ Offer only a successfully entered chapter, never background prefetch.
void book_excerpt_offer(const char *path, uint32_t chapter, const html_text_t *text);
