/* SPDX-License-Identifier: Apache-2.0
 * 中文：白屏锁屏反馈上的圆锁徽标；锁形取自图标库，无字库、文件 I/O、动画等待或像素缓存。
 * English: A round lock badge on the white feedback frame; the lock mark comes from the icon set, with no fonts, file I/O, animation waits or pixel caches.
 * 用户修订：取消占位拼贴和分步显影，调用者先清白后局部画锁；最终拼贴绘制器保持。
 * User revision: remove placeholders and staged reveal; the caller whitens before the local lock, retaining final collage artwork.
 */
#include "ui_lock_feedback.h"
#include <string.h>
#include "ui_kit.h"

// 徽标边长、圆环半径和锁的显示尺寸；圆环四像素厚，锁用图标库的 Lucide 图形。
// Badge side, ring radius and lock size; the ring is four pixels and the lock comes from the icon set.
enum { BADGE_SIZE = 97, BADGE_RADIUS = 48, BADGE_ICON_PX = 56 };

EpdRect ui_lock_feedback_draw(uint8_t *fb) {
    int width = epd_rotated_display_width(), height = epd_rotated_display_height();
    if (!fb || width < BADGE_SIZE || height < BADGE_SIZE) return (EpdRect){0};
    EpdRect area = {(width - BADGE_SIZE) / 2, (height - BADGE_SIZE) / 2, BADGE_SIZE, BADGE_SIZE};
    int cx = area.x + BADGE_RADIUS, cy = area.y + BADGE_RADIUS;
    // 白盘先盖住旧画面，再画圆环和锁；徽标以外的像素不动。
    // The white disc covers the old frame before the ring and lock; nothing outside the badge is touched.
    epd_fill_circle(cx, cy, BADGE_RADIUS, UI_GRAY_WHITE, fb);
    for (int r = BADGE_RADIUS - 3; r <= BADGE_RADIUS; ++r)
        epd_draw_circle(cx, cy, r, UI_GRAY_BLACK, fb);
    ui_draw_icon(fb, cx, cy, BADGE_ICON_PX, UI_ICON_LOCK, UI_GRAY_BLACK);
    return area;
}

void ui_lock_fallback_draw(uint8_t *fb) {
    if(!fb)return;
    memset(fb,255,(size_t)epd_width()*epd_height()/2);
    (void)ui_lock_feedback_draw(fb);
}
