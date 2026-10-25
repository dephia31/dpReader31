/* SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
 * 中文：系统和阅读共用的紧凑在线字体页；选择启用仍由原字体页负责。
 * English: Shared compact online-font page; activation stays in the original font pickers.
 */
#pragma once
#include "app.h"
#include "ui_gesture.h"
#include "online_font.h"
typedef struct {
    online_font_status_t status;
    bool installed[ONLINE_FONT_COUNT],reading,closed,confirm_cancel,leave_after_cancel;
    int64_t updated_ms;
    unsigned painted_percent;
} ui_online_fonts_t;

EpdRect ui_online_fonts_entry_rect(int top);
void ui_online_fonts_draw_entry(uint8_t *fb,int top);
void ui_online_fonts_open(ui_online_fonts_t *ui,bool reading);
void ui_online_fonts_close(ui_online_fonts_t *ui);
/// 纯绘制；无文件、网络、设置或字体加载。/ Pure paint without file, network, setting or font-load work.
void ui_online_fonts_draw(const ui_online_fonts_t *ui,uint8_t *fb);
app_redraw_t ui_online_fonts_gesture(ui_online_fonts_t *ui,const ui_gesture_event_t *event);
app_redraw_t ui_online_fonts_tick(ui_online_fonts_t *ui,int64_t now_ms);
/// 页面自有推屏，进度只更新下方状态带；调用方的present委托此函数。
/// Page-owned presentation; progress updates only the lower status band. Call from the page's present callback.
bool ui_online_fonts_present(ui_online_fonts_t *ui,app_ctx_t *ctx,app_redraw_t redraw);
