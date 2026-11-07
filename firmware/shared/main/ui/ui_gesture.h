/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * Vòng lặp chính cấp dữ liệu cảm ứng cho bộ nhận diện. Từng màn hình quyết định hành động.
 * English: Pure state recognizer fed by loop touch snapshots; no page actions.
 *
 * Quy tắc cố định: lật trang không có phản hồi khi nhấn. Dùng ngưỡng trong ui_kit.h và hủy thao tác kéo ngắn.
 * Frozen: No press decoration for page turns; thresholds live in ui_kit.h; short drags cancel to avoid accidental turns.
 * User revision 2026-11-03: all four swipe directions use a shorter 60px threshold.
 */
#pragma once
#include "app.h"

typedef enum {
    UI_GESTURE_PRESS, ///< Nhấn / Press
    UI_GESTURE_MOVE, ///< Kéo khi đang giữ để điều khiển thanh trượt / Move while held for continuous sliders
    UI_GESTURE_LONG_PRESS, ///< Nhấn giữ một lần / Single long press
    UI_GESTURE_TAP, ///< Chạm tại điểm bắt đầu / Tap at origin
    UI_GESTURE_SWIPE_L, ///< Vuốt trái / Swipe left
    UI_GESTURE_SWIPE_R, ///< Vuốt phải / Swipe right
    UI_GESTURE_SWIPE_U, ///< Vuốt lên / Swipe up
    UI_GESTURE_SWIPE_D, ///< Vuốt xuống / Swipe down
    UI_GESTURE_CANCEL, ///< Hủy / Cancel
} ui_gesture_type_t;

typedef struct ui_gesture_event_s {
    ui_gesture_type_t type; ///< Loại sự kiện / Event type
    uint16_t x0, y0; ///< Tọa độ bắt đầu, cũng là tọa độ của TAP / Press origin, also the logical TAP position
    uint16_t x, y; ///< Tọa độ hiện tại hoặc khi nhấc tay để hủy khi kéo ra ngoài / Current or release position for outside-control cancellation
    bool swipe_leftward; ///< Released after at least UI_SWIPE_MIN_PX leftward travel.
    int64_t hold_ms; ///< Thời lượng không âm / Nonnegative duration
} ui_gesture_event_t;

typedef struct {
    bool active; ///< Đang theo dõi / Tracking a contact
    uint16_t x0, y0; ///< Điểm bắt đầu / Origin
    uint16_t x, y; ///< Tọa độ hợp lệ gần nhất / Last valid coordinates
    int64_t t0_ms; ///< Thời điểm bắt đầu / Start time
    int64_t last_ms; ///< Thời điểm gần nhất, hủy nếu đồng hồ lùi / Last time, cancel on clock regression
    bool moved; ///< Đã vượt dung sai / Has exceeded movement slop
    bool long_fired; ///< Đã phát sự kiện nhấn giữ / Long press emitted
} ui_gesture_t;

/// Xóa trạng thái khi vào hoặc rời màn hình./ Clear state when entering or leaving a page.
void ui_gesture_reset(ui_gesture_t *gesture);
/// Mỗi lần phát tối đa một sự kiện. Hủy khi đầu vào đã được xử lý, chạm nhiều ngón hoặc thiếu dữ liệu.
/// Emit at most one event; consumed, multitouch or a missing snapshot cancels an active sequence.
bool ui_gesture_feed(ui_gesture_t *gesture, const app_ctx_t *ctx, ui_gesture_event_t *out);

/* Opt-in held drag for text selection; reset/release cancels the opt-in. */
void ui_gesture_enable_held_drag(bool enabled);
