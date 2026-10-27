/* SPDX-License-Identifier: Apache-2.0
 * 中文：顶部边缘识别器只产出快捷动作，不访问页面、硬件或刷新。
 * English: The top-edge recognizer emits quick actions without accessing pages, hardware or presentation.
 */
#include "ui_quick_menu.h"
#include <stdlib.h>
#include <string.h>
#include "ui_kit.h"
#include "ui_menu.h"

#if KIIKO_BOARD_METALIO47
#else
static int center_x(int button) { return (button * 2 + 1) * UI_LOCK_WIDTH / 8; }

#endif
EpdRect ui_quick_menu_rect(void) { return (EpdRect){0, 0, UI_LOCK_WIDTH, UI_QUICK_HEIGHT}; }

void ui_quick_menu_reset(ui_quick_menu_t *menu) { memset(menu, 0, sizeof(*menu)); }

void ui_quick_menu_cancel_input(ui_quick_menu_t *menu) {
    menu->tracking = false;
    menu->swallow = true;
}

#if KIIKO_BOARD_METALIO47
#else
int ui_quick_menu_pressed(const ui_quick_menu_t *menu) {
    return menu->open && menu->tracking && !menu->moved ? menu->hit : -1;
}

#endif
static void dismiss(ui_quick_menu_t *menu, ui_quick_action_t *action) {
    menu->open = menu->tracking = false;
    menu->swallow = true;
    *action = UI_QUICK_CLOSE;
}

static int hit_test(int x, int y) {
#if KIIKO_BOARD_METALIO47
    for (int i=0;i<2;i++) {
        int sx=UI_QUICK_SLIDER_X(i!=0);
        if(x>=sx && x<sx+UI_QUICK_SLIDER_WIDTH && y>=UI_QUICK_SLIDER_Y0 && y<=UI_QUICK_SLIDER_Y1)
            return UI_QUICK_BUTTON_COUNT+i;
    }
#else
#endif
    for (int i = 0; i < UI_QUICK_BUTTON_COUNT; ++i) {
#if KIIKO_BOARD_METALIO47
        const int dx = x - UI_QUICK_CENTER_X(i), dy = y - UI_QUICK_CENTER_Y(i);
#else
        const int dx = x - center_x(i), dy = y - UI_QUICK_CIRCLE_Y;
#endif
        if (dx * dx + dy * dy <= UI_QUICK_HIT_RADIUS * UI_QUICK_HIT_RADIUS) return i;
    }
    return -1;
#if KIIKO_BOARD_METALIO47
}
static int slider_position(int y) {
    if(y<UI_QUICK_SLIDER_Y0) y=UI_QUICK_SLIDER_Y0;
    if(y>UI_QUICK_SLIDER_Y1) y=UI_QUICK_SLIDER_Y1;
    return UI_QUICK_SLIDER_Y1-y;
#else
#endif
}

bool ui_quick_menu_feed(ui_quick_menu_t *menu, const app_ctx_t *ctx, bool allowed,
                       ui_quick_action_t *action) {
    *action = UI_QUICK_NONE;
    if (!allowed) {
        bool owned = menu->tracking || menu->swallow || menu->open;
        if (menu->open) dismiss(menu, action);
        else ui_quick_menu_reset(menu);
        return owned;
    }
    if (!ctx->touch || (ctx->touch->touched && ctx->touch->count != 1)) {
        bool owned = menu->open || menu->tracking || menu->swallow;
        if (owned) ui_quick_menu_cancel_input(menu);
        return owned;
    }
    if (menu->swallow) {
        if (!ctx->touch->touched) menu->swallow = false;
        return true;
    }
    int x = ctx->touch->x, y = ctx->touch->y;
    if (ctx->pressed && ctx->touch->touched) {
        if (!menu->open && y > UI_QUICK_EDGE_PX) return false;
        if (menu->open && ui_key_hit_test(x, y) >= 0) {
            dismiss(menu, action);
            return true;
        }
        menu->tracking = true;
        menu->moved = false;
        menu->x0 = x; menu->y0 = y;
        menu->started_ms = ctx->now_ms;
        menu->hit = hit_test(x, y);
#if KIIKO_BOARD_METALIO47
        if(menu->open && menu->hit>=UI_QUICK_BUTTON_COUNT) {
            menu->slider_position=slider_position(y);
            menu->slider_value=((menu->hit==UI_QUICK_BUTTON_COUNT?ui_quick_brightness_value(menu->slider_position):(unsigned)menu->slider_position)*100+UI_QUICK_SLIDER_RANGE/2)/UI_QUICK_SLIDER_RANGE;
            *action=menu->hit==UI_QUICK_BUTTON_COUNT?UI_QUICK_BRIGHTNESS:UI_QUICK_WARMTH;
            return true;
        }
#else
#endif
    }
    if (!menu->tracking) return menu->open;
#if KIIKO_BOARD_METALIO47
    // 滑块一旦捕获，整段拖动归它所有，越界或上滑不能切页。
    // Captured sliders own the complete drag, including movement outside the track.
    if(menu->open && menu->hit>=UI_QUICK_BUTTON_COUNT) {
        if(ctx->released) {
            menu->tracking=false;
            *action=UI_QUICK_LIGHT_COMMIT;
        } else if(ctx->touch->touched) {
            int position=slider_position(y);
            if(position!=menu->slider_position) {
                menu->slider_position=position;
                menu->slider_value=((menu->hit==UI_QUICK_BUTTON_COUNT?ui_quick_brightness_value(position):(unsigned)position)*100+UI_QUICK_SLIDER_RANGE/2)/UI_QUICK_SLIDER_RANGE;
                *action=menu->hit==UI_QUICK_BUTTON_COUNT?UI_QUICK_BRIGHTNESS:UI_QUICK_WARMTH;
            }
        }
        return true;
    }
#else
#endif
    int dx = x - (int)menu->x0, dy = y - (int)menu->y0;
    if (ctx->now_ms < menu->started_ms) {
        ui_quick_menu_cancel_input(menu);
        return true;
    }
    if (abs(dx) > UI_TOUCH_SLOP_PX || abs(dy) > UI_TOUCH_SLOP_PX) menu->moved = true;
    if (!menu->open && dy >= UI_QUICK_SWIPE_PX && dy >= abs(dx) * 2 &&
        ctx->now_ms - menu->started_ms <= 1500) {
        menu->open = true;
        menu->tracking = false;
        menu->swallow = true;
        *action = UI_QUICK_OPEN;
    } else if (menu->open && dy <= -UI_QUICK_SWIPE_PX && -dy >= abs(dx) * 2) {
        dismiss(menu, action);
    } else if (ctx->released) {
        menu->tracking = false;
        if (menu->open && !menu->moved) {
            if (menu->hit >= 0 && hit_test(x, y) == menu->hit) {
                static const ui_quick_action_t actions[] = {
#if KIIKO_BOARD_METALIO47
                    UI_QUICK_WIFI, UI_QUICK_BLUETOOTH, UI_QUICK_KOSYNC, UI_QUICK_FULL, UI_QUICK_FRONTLIGHT,
#else
                    UI_QUICK_WIFI, UI_QUICK_BLUETOOTH, UI_QUICK_FULL, UI_QUICK_LOCK,
#endif
                };
                *action = actions[menu->hit];
            } else if (menu->y0 >= UI_QUICK_HEIGHT && y >= UI_QUICK_HEIGHT) {
                dismiss(menu, action);
                menu->swallow = false;
            }
        }
    }
    return true;
}
#if KIIKO_BOARD_METALIO47

// Only a stationary held circle can show press feedback; sliders own their entire drag.
int ui_quick_menu_pressed(const ui_quick_menu_t *menu) {
    return menu&&menu->open&&menu->tracking&&!menu->swallow&&!menu->moved&&menu->hit>=0&&menu->hit<UI_QUICK_BUTTON_COUNT ? menu->hit : -1;
}
#else
#endif
