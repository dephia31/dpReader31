/* SPDX-License-Identifier: Apache-2.0
 * 中文：列表像素跟随与有界减速惯性；不分配、不绘图、不访问硬件。
 * English: Pixel-follow lists with bounded decelerating inertia; no allocation, drawing or hardware access.
 */
#pragma once
#include <stdlib.h>
#include <string.h>
#include "app.h"

#define UI_FOLLOW_SCROLL_SLOP 12
#define UI_FOLLOW_SCROLL_SETTLE_MS 320
#define UI_FOLLOW_COAST_MAX_SPEED 2400
#define UI_FOLLOW_COAST_MIN_SPEED 180
#define UI_FOLLOW_COAST_DECEL 3200

typedef struct {
    bool tracking, scrolling, swallow, settle_pending, coasting, stop_tap;
    int x0, y0, origin, sample_offset, velocity, last_velocity, remainder, decel_remainder;
    int64_t last_ms, settle_ms, sample_ms, last_change_ms, coast_ms, coast_start_ms;
} ui_follow_scroll_t;

/// 页面边界重置，旧惯性不带到下一页。/ Reset at page boundaries without carrying old momentum.
static inline void ui_follow_scroll_reset(ui_follow_scroll_t *state) { memset(state, 0, sizeof(*state)); }

/// 取消保留当前偏移，错误/多指不启动惯性。/ Cancel at the visible offset; faults/multiple fingers never launch inertia.
static inline void ui_follow_scroll_cancel(ui_follow_scroll_t *state, int64_t now) {
    bool changed = state->scrolling || state->coasting;
    state->tracking = state->scrolling = state->coasting = state->stop_tap = false;
    state->velocity = state->last_velocity = state->remainder = state->decel_remainder = 0;
    state->swallow = true;
    if (changed) { state->settle_pending = true; state->settle_ms = now + UI_FOLLOW_SCROLL_SETTLE_MS; }
}

static inline bool ui_follow_scroll_coast(ui_follow_scroll_t *state, const app_ctx_t *ctx, int *offset, int limit) {
    int64_t elapsed = ctx->now_ms - state->coast_ms;
    if (elapsed <= 0) return false;
    if (elapsed > 120 || ctx->now_ms - state->coast_start_ms > 900) {
        state->coasting = false; state->velocity = state->remainder = 0;
        return false;
    }
    state->coast_ms = state->last_ms = ctx->now_ms;
    int speed = abs(state->velocity);
    int64_t force=UI_FOLLOW_COAST_DECEL*elapsed+state->decel_remainder;
    int decel=(int)(force/1000);state->decel_remainder=(int)(force%1000);
    int next_speed = speed > decel ? speed - decel : 0;
    int next_velocity = state->velocity < 0 ? -next_speed : next_speed;
    int64_t travel = (int64_t)state->remainder + (state->velocity + next_velocity) * elapsed;
    int next = *offset + (int)(travel / 2000);
    state->remainder = (int)(travel % 2000); state->velocity = next_velocity;
    if (next <= 0 || next >= limit || next_speed < 35) {
        if (next < 0) next = 0;
        if (next > limit) next = limit;
        state->coasting = false; state->velocity = state->remainder = 0;
    }
    // 惯性全程保持黑白，真正停下后才补灰阶。/ Stay binary throughout inertia; restore grays only after it stops.
    state->settle_ms = ctx->now_ms + UI_FOLLOW_SCROLL_SETTLE_MS;
    if (next == *offset) return false;
    *offset = next; return true;
}

/// handled吞掉已捕获滚动和停止惯性的轻触，保留正常点击与导航。
/// Handled suppresses captured drags and stop-momentum taps, retaining ordinary taps and navigation.
static inline bool ui_follow_scroll_feed(ui_follow_scroll_t *state, const app_ctx_t *ctx,
    int top, int bottom, int *offset, int limit, bool *handled) {
    *handled = state->scrolling || state->swallow || state->coasting || state->stop_tap;
    if (!ctx || !ctx->touch || ctx->consumed || ctx->now_ms < 0 ||
        ((state->tracking || state->coasting) && ctx->now_ms < state->last_ms) ||
        (ctx->touch->touched && ctx->touch->count != 1)) {
        ui_follow_scroll_cancel(state, ctx ? ctx->now_ms : 0); *handled = true; return false;
    }
    if (limit < 0) limit = 0;
    const cst836u_touch_t *touch = ctx->touch;
    bool inside = touch->y >= top && touch->y < bottom;
    if (state->swallow) { if (!touch->touched) state->swallow = false; return false; }
    if (state->coasting) {
        if (!touch->touched) return ui_follow_scroll_coast(state, ctx, offset, limit);
        // 新触摸立即停止；列表内可接着拖，轻点只停止，不误开行。题头/底栏仍可导航。
        // New contact stops immediately; resume dragging inside, consume a stop tap, and retain chrome navigation.
        state->coasting = false; state->velocity = state->last_velocity = state->remainder = state->decel_remainder = 0;
        state->settle_ms = ctx->now_ms + UI_FOLLOW_SCROLL_SETTLE_MS;
        if (!ctx->pressed || !inside) { *handled = false; return false; }
        state->stop_tap = true;
    }
    if (!state->tracking) {
        if (!ctx->pressed || !touch->touched || !inside) { *handled = state->stop_tap; return false; }
        state->tracking = true; state->x0 = touch->x; state->y0 = touch->y; state->origin = *offset;
        state->sample_offset = *offset; state->sample_ms = state->last_change_ms = ctx->now_ms;
        state->velocity = state->last_velocity = state->remainder = state->decel_remainder = 0;
        if (state->settle_pending) state->settle_ms = ctx->now_ms + UI_FOLLOW_SCROLL_SETTLE_MS;
    }
    state->last_ms = ctx->now_ms;
    int dx = (int)touch->x - state->x0, dy = (int)touch->y - state->y0;
    if (!state->scrolling && touch->touched && abs(dy) >= UI_FOLLOW_SCROLL_SLOP && abs(dy) > abs(dx))
        state->scrolling = true;
    *handled = state->scrolling || state->stop_tap;
    if (!touch->touched) {
        bool scrolled = state->scrolling;
        state->tracking = state->scrolling = state->stop_tap = false;
        if (scrolled) {
            state->settle_pending = true; state->settle_ms = ctx->now_ms + UI_FOLLOW_SCROLL_SETTLE_MS;
            bool recent = ctx->now_ms - state->last_change_ms < 80;
            state->coasting = recent && abs(state->velocity) >= UI_FOLLOW_COAST_MIN_SPEED &&
                abs(state->last_velocity) >= UI_FOLLOW_COAST_MIN_SPEED &&
                ((*offset > 0 && state->velocity < 0) || (*offset < limit && state->velocity > 0));
            state->coast_ms = state->coast_start_ms = ctx->now_ms;
        }
        return false;
    }
    if (!state->scrolling) {state->sample_offset=*offset;state->sample_ms=ctx->now_ms;return false;}
    int next = state->origin - dy;
    if (next < 0) next = 0;
    if (next > limit) next = limit;
    if (next == *offset) {
        if (next == 0 || next == limit) state->velocity = state->last_velocity = 0;
        return false;
    }
    int64_t duration = ctx->now_ms - state->sample_ms;
    if (duration >= 8) {
        int64_t v = (int64_t)(next - state->sample_offset) * 1000 / duration;
        if (v > UI_FOLLOW_COAST_MAX_SPEED) v = UI_FOLLOW_COAST_MAX_SPEED;
        if (v < -UI_FOLLOW_COAST_MAX_SPEED) v = -UI_FOLLOW_COAST_MAX_SPEED;
        int instant = (int)v;
        state->velocity = state->last_velocity * instant < 0 ? instant : (state->velocity + 3 * instant) / 4;
        state->last_velocity = instant; state->sample_offset = next; state->sample_ms = ctx->now_ms;
    }
    state->last_change_ms = ctx->now_ms; *offset = next; return true;
}

/// 惯性停止并排完驱动相位后只定稿一次。/ Settle once after inertia stops and driving phases complete.
static inline bool ui_follow_scroll_take_settle(ui_follow_scroll_t *state, const app_ctx_t *ctx) {
    if (!state->settle_pending || state->tracking || state->swallow || state->coasting || !ctx || ctx->consumed ||
        (ctx->touch && ctx->touch->touched) || ctx->now_ms < state->settle_ms) return false;
    state->settle_pending = false; return true;
}
