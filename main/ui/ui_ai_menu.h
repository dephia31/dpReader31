/* SPDX-License-Identifier: Apache-2.0
 * 中文：S31 AI键短按功能选择层，只合成有界画面，不写设置、不调用显示。
 * English: Bounded S31 AI-tap chooser; compose only, without persistence or display calls.
 */
#pragma once
#include "app.h"
#include "settings.h"
typedef struct {
    uint8_t *underlay,*frame;
    EpdRect rect;
    int rotation,selected,pressed,x0,y0;
    bool open,draining,armed;
} ui_ai_menu_t;
enum {UI_AI_CLOSE=-1,UI_AI_NO_ACTION=-2};
/// 仅成功捕获底图后打开，失败保持画面不变。/ Open only after saving the underlay; failure leaves the frame intact.
bool ui_ai_menu_begin(ui_ai_menu_t *menu,uint8_t *fb,app_ai_action_t selected,bool reader);
bool ui_ai_menu_restore(ui_ai_menu_t *menu,uint8_t *fb);
void ui_ai_menu_end(ui_ai_menu_t *menu);
/// 五个横排选项的统一绘制/命中矩形。/ Shared paint/hit bounds for five horizontal choices.
EpdRect ui_ai_menu_option(const ui_ai_menu_t *menu,int option);
/// 单指抬手提交，跨选项/多指/读错取消；返回动作编号或NO_ACTION/CLOSE。
/// Commit on a single release; cancel cross-option/multitouch/read errors. Return action index or NO_ACTION/CLOSE.
int ui_ai_menu_feed(ui_ai_menu_t *menu,const app_ctx_t *ctx,bool valid);
EpdRect ui_ai_menu_save_error(ui_ai_menu_t *menu,uint8_t *fb);
