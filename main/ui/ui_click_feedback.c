/* SPDX-License-Identifier: Apache-2.0
 * 中文：底栏图标放大和圆形返回键轻微下沉，只暂存一个控件真实像素。
 * English: Enlarge navigation icons and gently depress round backs, retaining actual pixels for one control only.
 * 冻结：无整帧缓存或旧页回放；返回键释放即恢复，换帧丢弃旧像素。
 * Frozen: No full-frame storage or stale replay; backs restore on release, and new paints discard old pixels.
 * 用户修订：底栏恢复按下放大、随后复位；每次触控先给局部反馈，和是否成功切页无关，连点可重触发。
 * User revision: restore the pressed enlargement followed by settling; each contact gets local feedback independently of navigation, and rapid taps retrigger.
 * 用户修订：普通、快刷、水波纹共用底栏反馈，保留灰底及文字；仅重绘图标盒并推真实变化边界，避免矩形白框。
 * User revision: ordinary, fast and water share navigation feedback, retaining gray backgrounds and labels; repaint only the icon box and present actual change bounds to avoid white rectangles.
 */
#include "ui_click_feedback.h"
#include <string.h>
#include "esp_heap_caps.h"
#include "settings.h"
#include "ui_nav_layout.h"
typedef struct { EpdRect area; ui_click_kind_t kind; ui_icon_t icon; } click_control_t;
static click_control_t s_controls[12], s_active;
static unsigned s_count;
static uint8_t *s_pixels;
static uint8_t *s_frame;
static int s_x, s_y;
static EpdRect changed_area(uint8_t *fb);
static struct { int tab, phase; int64_t due; EpdRect dirty; bool accent; } s_nav = {.tab=-1};
static void restore_pixels(uint8_t *fb) {
    if (!s_pixels || fb != s_frame) return;
    EpdRect r=s_active.area;
    for(int y=0;y<r.height;++y)for(int x=0;x<r.width;++x)
        epd_draw_pixel(r.x+x,r.y+y,s_pixels[y*r.width+x],fb);
}
static EpdRect nav_union(EpdRect a,EpdRect b) {
    if(a.width<=0)return b;
    if(b.width<=0)return a;
    int x=a.x<b.x?a.x:b.x,y=a.y<b.y?a.y:b.y;
    int right=a.x+a.width>b.x+b.width?a.x+a.width:b.x+b.width;
    int bottom=a.y+a.height>b.y+b.height?a.y+a.height:b.y+b.height;
    return (EpdRect){x,y,right-x,bottom-y};
}
bool ui_click_feedback_nav_pending(void) { return s_nav.tab>=0; }
bool ui_click_feedback_active(void) { return s_pixels != NULL; }
void ui_click_feedback_reset(void) {
    heap_caps_free(s_pixels); s_pixels = NULL; s_count = 0; s_frame = NULL;
    s_nav.tab=-1;s_nav.phase=0;s_nav.due=0;s_nav.dirty=(EpdRect){0};
}
void ui_click_feedback_begin(uint8_t *fb) {
    // 四主页共用完全相同的图标底色，保留这一个小控件原像素，切页后仍能准确复位。
    // Main roots share identical icon backgrounds; retain this one control baseline so it settles correctly after navigation.
    if(s_pixels&&s_active.kind==UI_CLICK_NAV&&ui_click_feedback_nav_pending()&&fb==s_frame) {
        restore_pixels(fb);s_count=0;return;
    }
    heap_caps_free(s_pixels);s_pixels=NULL;s_count=0;s_frame=fb;
    if(ui_click_feedback_nav_pending()){s_nav.phase=0;s_nav.due=0;}
}
void ui_click_feedback_register(uint8_t *fb, EpdRect rect, ui_click_kind_t kind, ui_icon_t icon) {
    if (fb != s_frame || !fb) return;
    if (rect.x < 0 || rect.y < 0 || rect.width <= 0 || rect.height <= 0 ||
        rect.width > 64 || rect.height > 64 || rect.x + rect.width > epd_rotated_display_width() ||
        rect.y + rect.height > epd_rotated_display_height()) return;
    for (unsigned i = 0; i < s_count; ++i) if (!memcmp(&s_controls[i].area, &rect, sizeof(rect))) {
        s_controls[i] = (click_control_t){rect, kind, icon}; return;
    }
    if (s_count < 12) s_controls[s_count++] = (click_control_t){rect, kind, icon};
}
static uint8_t read_pixel(uint8_t *fb, int x, int y) {
    int px = x, py = y;
    switch (epd_get_rotation()) {
        case EPD_ROT_PORTRAIT: px = epd_width() - y - 1; py = x; break;
        case EPD_ROT_INVERTED_LANDSCAPE: px = epd_width() - x - 1; py = epd_height() - y - 1; break;
        case EPD_ROT_INVERTED_PORTRAIT: px = y; py = epd_height() - x - 1; break;
        default: break;
    }
    return epd_get_pixel(px, py, epd_width(), epd_height(), fb);
}
// 缓存边界不等于推屏边界；未变的灰底交给等灰保持，文字和横条不参与动效。
// Cache bounds are not presentation bounds; hold unchanged grays and exclude labels and the marker from feedback.
static EpdRect changed_area(uint8_t *fb) {
    EpdRect r = s_active.area;
    int left = r.width, top = r.height, right = -1, bottom = -1;
    for (int y = 0; y < r.height; ++y) for (int x = 0; x < r.width; ++x) {
        if (read_pixel(fb, r.x + x, r.y + y) == s_pixels[y * r.width + x]) continue;
        if (x < left) left = x;
        if (y < top) top = y;
        if (x > right) right = x;
        if (y > bottom) bottom = y;
    }
    return right < 0 ? r : (EpdRect){r.x + left, r.y + top, right - left + 1, bottom - top + 1};
}
bool ui_click_feedback_release(uint8_t *fb, EpdRect *area) {
    if (!s_pixels) return false;
    *area = changed_area(fb);
    restore_pixels(fb);
    *area=nav_union(*area,s_nav.dirty);
    heap_caps_free(s_pixels); s_pixels = NULL;
    s_nav.tab=-1;s_nav.dirty=(EpdRect){0};
    return true;
}
bool ui_click_feedback_cancel_at(int x, int y) {
    bool outside = s_active.kind == UI_CLICK_NAV
        ? x < 0 || x >= UI_LOCK_WIDTH || y < UI_NAV_TOUCH_TOP || y >= UI_LOCK_HEIGHT ||
          x / (UI_LOCK_WIDTH / 4) != s_x / (UI_LOCK_WIDTH / 4)
        : x < s_active.area.x || x >= s_active.area.x + s_active.area.width ||
          y < s_active.area.y || y >= s_active.area.y + s_active.area.height;
    return s_pixels && (outside ||
        x - s_x > UI_TOUCH_SLOP_PX || s_x - x > UI_TOUCH_SLOP_PX ||
        y - s_y > UI_TOUCH_SLOP_PX || s_y - y > UI_TOUCH_SLOP_PX);
}
bool ui_click_feedback_press(uint8_t *fb, int x, int y, EpdRect *area) {
    if (s_pixels || ui_click_feedback_nav_pending()) return false;
    for (unsigned i = 0; i < s_count; ++i) {
        EpdRect r = s_controls[i].area;
        bool hit = s_controls[i].kind == UI_CLICK_NAV
            ? y >= UI_NAV_TOUCH_TOP && y < UI_LOCK_HEIGHT && x >= 0 && x < UI_LOCK_WIDTH &&
              x / (UI_LOCK_WIDTH / 4) == (r.x + r.width / 2) / (UI_LOCK_WIDTH / 4)
            : x >= r.x && x < r.x + r.width && y >= r.y && y < r.y + r.height;
        if (!hit) continue;
        s_pixels = heap_caps_malloc((size_t)r.width * r.height, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!s_pixels) return false;
        s_active = s_controls[i]; s_x = x; s_y = y; *area = r;
        for (int yy = 0; yy < r.height; ++yy) for (int xx = 0; xx < r.width; ++xx)
            s_pixels[yy * r.width + xx] = read_pixel(fb, r.x + xx, r.y + yy);
        int cx = r.x + r.width / 2, cy = r.y + r.height / 2;
        if (s_active.kind == UI_CLICK_BACK || s_active.kind == UI_CLICK_BACK_SMALL) {
            epd_fill_rect(r, s_pixels[0], fb);
            // 返回图标包含完整圆环；按压只增加圆内底色，不另画圆环或移动题头按钮。
            // The icon includes its ring; press tint stays inside without another outline or shifted header geometry.
            bool small=s_active.kind==UI_CLICK_BACK_SMALL;
            int diameter=small?40:44,offset=small?0:1;
            ui_fill_round_rect(fb,(EpdRect){cx-diameter/2,cy-diameter/2+offset,diameter,diameter},diameter/2,0xd0);
            ui_draw_icon(fb,cx,cy+offset,small?48:52,s_active.icon,UI_GRAY_BLACK);
            if (app_settings_main_fast_refresh()) ui_image_bw_rect(fb, r);
        } else {
            // 底栏始终保留原灰底，不把整个按压缓存转成黑白点阵。
            // Retain the original footer gray in every mode rather than binarizing the entire pressed cache.
            // 放大笔画从32列对齐的横条扫描边界下方开始，避开横条及底部文字。
            // Enlarged ink starts below the marker's aligned 32-column scan boundary, clear of its marker and labels.
            epd_fill_rect((EpdRect){cx - 25, cy - 23, 50, 50}, s_pixels[0], fb);
            ui_draw_icon(fb, cx, cy + 2, 50, s_active.icon, UI_GRAY_BLACK);
        }
        *area = changed_area(fb);
        return true;
    }
    return false;
}

// 还原单次放大/复位反馈；新点击优先，连点同一图标用轻微尺寸差避免被差分吞掉。
// Restore one enlargement/settle; new contacts win and a small repeated-tap variation prevents identical-frame skips.
void ui_click_feedback_nav_request(int tab) {
    if(tab<0||tab>=4)return;
    bool accent=s_nav.tab==tab&&s_pixels ? !s_nav.accent : false;
    if(s_pixels) {
        if(s_active.kind==UI_CLICK_NAV)s_nav.dirty=nav_union(s_nav.dirty,changed_area(s_frame));
        restore_pixels(s_frame);
        heap_caps_free(s_pixels);s_pixels=NULL;
    }
    s_nav.tab=tab;s_nav.phase=0;s_nav.due=0;s_nav.accent=accent;
}
bool ui_click_feedback_nav_finish(uint8_t *fb,EpdRect *area) {
    if(!ui_click_feedback_nav_pending())return false;
    EpdRect dirty=s_nav.dirty;
    if(s_pixels) {
        dirty=nav_union(dirty,changed_area(fb));restore_pixels(fb);
        heap_caps_free(s_pixels);s_pixels=NULL;
    }
    s_nav.tab=-1;s_nav.dirty=(EpdRect){0};*area=dirty;
    return dirty.width>0;
}
static void nav_pose(uint8_t *fb) {
    EpdRect r=s_active.area;
    int cx=r.x+r.width/2,cy=r.y+r.height/2;
    epd_fill_rect((EpdRect){cx-25,cy-23,50,50},s_pixels[0],fb);
    ui_draw_icon(fb,cx,cy+(s_nav.accent?1:2),s_nav.accent?48:50,s_active.icon,UI_GRAY_BLACK);
}
bool ui_click_feedback_nav_tick(uint8_t *fb,int64_t now_ms,EpdRect *area) {
    if(!ui_click_feedback_nav_pending()||fb!=s_frame)return false;
    if(s_nav.phase==0) {
        click_control_t *control=NULL;
        for(unsigned i=0;i<s_count;++i)if(s_controls[i].kind==UI_CLICK_NAV&&
            (s_controls[i].area.x+s_controls[i].area.width/2)/(UI_LOCK_WIDTH/4)==s_nav.tab) {
            control=&s_controls[i];break;
        }
        // 书架正等待封面时新帧尚未注册底栏；按稳定公共布局反馈，不能等待目标页出现。
        // While shelf covers load, the new frame may lack registrations; use the stable common footer immediately.
        static const ui_icon_t tabs[]={UI_ICON_HOUSE,UI_ICON_LIBRARY_BIG,UI_ICON_FOLDER_OPEN,UI_ICON_SLIDERS_HORIZONTAL};
        int center=(s_nav.tab*2+1)*UI_LOCK_WIDTH/8;
        s_active=control?*control:(click_control_t){(EpdRect){center-30,UI_NAV_TOP+15,60,60},UI_CLICK_NAV,tabs[s_nav.tab]};
        EpdRect r=s_active.area;
        s_pixels=heap_caps_malloc((size_t)r.width*r.height,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
        if(!s_pixels) {
            *area=s_nav.dirty;s_nav.tab=-1;s_nav.dirty=(EpdRect){0};
            return area->width>0;
        }
        for(int y=0;y<r.height;++y)for(int x=0;x<r.width;++x)
            s_pixels[y*r.width+x]=read_pixel(fb,r.x+x,r.y+y);
        nav_pose(fb);
        *area=nav_union(changed_area(fb),s_nav.dirty);s_nav.dirty=(EpdRect){0};
        s_nav.phase=1;s_nav.due=now_ms+65;return true;
    }
    if(now_ms<s_nav.due)return false;
    return ui_click_feedback_nav_finish(fb,area);
}
