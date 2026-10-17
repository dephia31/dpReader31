/* SPDX-License-Identifier: Apache-2.0
 * 中文：共享控件按压反馈及不阻塞导航的底栏短动效。
 * English: Shared pressed controls and navigation-independent footer tap feedback.
 */
#pragma once
#include "ui_kit.h"
typedef enum { UI_CLICK_NAV, UI_CLICK_BACK, UI_CLICK_BACK_SMALL } ui_click_kind_t;
void ui_click_feedback_reset(void);
/// 仅主循环当前帧可注册；后台阅读预绘制不能改控件表。/ Only the loop's current frame may register; reader prefetch cannot alter controls.
void ui_click_feedback_begin(uint8_t *fb);
/// 最多12控件，一个按压缓存上限4096B PSRAM；OOM仅略过装饰。/ Up to 12 controls, one press cache bounded to 4096 PSRAM bytes; OOM skips decoration only.
void ui_click_feedback_register(uint8_t *fb, EpdRect rect, ui_click_kind_t kind, ui_icon_t icon);
/// 三种主页模式保留底栏灰底，返回实际变化边界，不按缓存方框推屏。/ All three main modes retain footer grays and return actual change bounds instead of the cache rectangle.
bool ui_click_feedback_press(uint8_t *fb, int x, int y, EpdRect *area);
/// 先记录变化边界，再精确恢复完整小控件缓存；不分配额外内存。/ Record change bounds before restoring the complete small cache exactly, without another allocation.
bool ui_click_feedback_release(uint8_t *fb, EpdRect *area);
bool ui_click_feedback_cancel_at(int x, int y);
bool ui_click_feedback_active(void);

/// 只登记动作，不推屏、不等待；连续点击替换旧动作。/ Queue only, without presentation or waits; a newer click replaces it.
void ui_click_feedback_nav_request(int tab);
/// 按下同轮显示放大，随后定时复位；不依赖切页成功，保留底色、文字和横条。/ Enlarge in the contact loop, then settle independently of navigation, retaining background, labels and marker.
bool ui_click_feedback_nav_tick(uint8_t *fb,int64_t now_ms,EpdRect *area);
bool ui_click_feedback_nav_pending(void);
/// 覆盖层出现前恢复底栏；只返回必要差分，避免遗留放大姿态。/ Restore before an overlay and return only necessary damage bounds.
bool ui_click_feedback_nav_finish(uint8_t *fb,EpdRect *area);
