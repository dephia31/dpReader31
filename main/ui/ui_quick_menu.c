/* SPDX-License-Identifier: Apache-2.0
 * 中文：全宽浅顶部层，下圆角与四个无文字圆钮；三个主页模式统一灰阶和清晰边框。
 * English: A shallow full-width top layer with rounded bottom corners and four unlabeled circles; all main modes share grayscale and clear borders.
 * 冻结：不访问硬件、设置或刷新，不申请帧缓存；只在展开/状态变化后由主循环调用绘图。
 * Frozen: No hardware, settings, presentation or frame allocations; the loop draws only on opening or state changes.
 */
#include "ui_quick_menu.h"
#include <stdlib.h>
#include <string.h>
#include "ui_kit.h"
#include "ui_nav.h"


// 根据当前旋转读取逻辑像素；和图形接口相同，画点保持半字节邻居。
// Read logical pixels using the current rotation; drawing preserves the neighboring nibble like the graphics API.
static uint8_t pixel(uint8_t *fb, int x, int y) {
    int px, py;
    switch (epd_get_rotation()) {
        case EPD_ROT_PORTRAIT: px = epd_width() - y - 1; py = x; break;
        case EPD_ROT_INVERTED_LANDSCAPE: px = epd_width() - x - 1; py = epd_height() - y - 1; break;
        case EPD_ROT_INVERTED_PORTRAIT: px = y; py = epd_height() - x - 1; break;
        default: px = x; py = y; break;
    }
    return (epd_get_pixel(px, py, epd_width(), epd_height(), fb) >> 4) * 17u;
}

static bool layer_pixel(int x, int y, int width) {
    const int r = 56;
    if (y < UI_QUICK_HEIGHT - r || (x >= r && x < width - r)) return true;
    int dx = x < r ? x - r : x - (width - r - 1);
    int dy = y - (UI_QUICK_HEIGHT - r - 1);
    return dx * dx + dy * dy < r * r;
}

EpdRect ui_quick_button_rect(unsigned index) {
    const int r=UI_QUICK_RADIUS-2;
    return (EpdRect){UI_QUICK_CENTER_X(index)-r,UI_QUICK_CENTER_Y(index)-r,r*2+1,r*2+1};
}
void ui_quick_menu_draw_button(uint8_t *fb,unsigned index,bool enabled) {
    if(!fb || index>=4) return;
    static const ui_icon_t icons[]={UI_ICON_WIFI,UI_ICON_BLUETOOTH,UI_ICON_REFRESH_CW,UI_ICON_LIGHTBULB};
    const int cx=UI_QUICK_CENTER_X(index),cy=UI_QUICK_CENTER_Y(index),r=UI_QUICK_RADIUS;
    for(int y=-r;y<=r;y++) for(int x=-r;x<=r;x++) {
        const int d=x*x+y*y;
        if(d<=r*r) epd_draw_pixel(cx+x,cy+y,d<=(r-2)*(r-2)?(enabled?UI_QUICK_FILL_DARK:UI_GRAY_WHITE):0x50,fb);
    }
    ui_draw_icon(fb,cx,cy,44,icons[index],enabled?UI_GRAY_WHITE:UI_QUICK_FILL_DARK);
    // 两种纯色覆盖，不用点阵；图标抗锯齿量化到同一深灰/白色，静态轮廓不变。
    // Two solid tones, no dithering; quantize icon coverage to dark gray/white and hold the outline.
    for(int y=cy-22;y<cy+22;y++) for(int x=cx-22;x<cx+22;x++)
        epd_draw_pixel(x,y,pixel(fb,x,y)>=((UI_QUICK_FILL_DARK+UI_GRAY_WHITE)/2)?UI_GRAY_WHITE:UI_QUICK_FILL_DARK,fb);
    if(enabled) epd_fill_circle(cx+24,cy-24,5,UI_GRAY_WHITE,fb);
}

// 两个胶囊共享尺寸和触点范围，向上增加；仅图标，无标签和百分比。
// Both vertical capsules share drawing/input geometry; upward increases, with icons only.
EpdRect ui_quick_slider_rect(bool warmth) {
    return (EpdRect){UI_QUICK_SLIDER_X(warmth)-2,UI_QUICK_SLIDER_Y0-2,
        UI_QUICK_SLIDER_WIDTH+4,UI_QUICK_SLIDER_Y1-UI_QUICK_SLIDER_Y0+4};
}
// 只包含两次填充边界之间的条带，不触碰整块胶囊的边框。
// Restrict refresh to the band between fill boundaries; unchanged outlines stay still.
EpdRect ui_quick_slider_change_rect(bool warmth,unsigned before,unsigned after) {
    const int height=UI_QUICK_SLIDER_Y1-UI_QUICK_SLIDER_Y0;
    if(before>(unsigned)height) before=height;
    if(after>(unsigned)height) after=height;
    const int a=height-(int)before,b=height-(int)after;
    int top=a<b?a:b,bottom=a>b?a:b;
    if(top<2) top=2;
    if(top>height-2) top=height-2;
    if(bottom<2) bottom=2;
    if(bottom>height-2) bottom=height-2;
    return (EpdRect){UI_QUICK_SLIDER_X(warmth)+2,UI_QUICK_SLIDER_Y0+top,
        UI_QUICK_SLIDER_WIDTH-4,bottom>top?bottom-top:0};
}
static bool capsule_pixel(int x,int y) {
    const int r=UI_QUICK_SLIDER_WIDTH/2;
    const int height=UI_QUICK_SLIDER_Y1-UI_QUICK_SLIDER_Y0;
    if (y>=r && y<height-r) return true;
    int dx=x-r,dy=y<(height/2)?y-r:y-(height-r-1);
    return dx*dx+dy*dy<r*r;
}
void ui_quick_menu_draw_slider(uint8_t *fb,bool warmth,unsigned value) {
    if(value>UI_QUICK_SLIDER_RANGE) value=UI_QUICK_SLIDER_RANGE;
    const EpdRect area=ui_quick_slider_rect(warmth);
    epd_fill_rect(area,0xee,fb);
    const int x0=UI_QUICK_SLIDER_X(warmth),height=UI_QUICK_SLIDER_Y1-UI_QUICK_SLIDER_Y0;
    const int split=height-(int)value;
    for(int y=0;y<height;y++) for(int x=0;x<UI_QUICK_SLIDER_WIDTH;x++) {
        if(!capsule_pixel(x,y)) continue;
        const bool outline=x<2 || x>=UI_QUICK_SLIDER_WIDTH-2 || y<2 || y>=height-2 ||
            !capsule_pixel(x-2,y) || !capsule_pixel(x+2,y) ||
            !capsule_pixel(x,y-2) || !capsule_pixel(x,y+2);
        epd_draw_pixel(x0+x,UI_QUICK_SLIDER_Y0+y,outline?0x50:(y>=split?0xff:UI_QUICK_FILL_DARK),fb);
    }
    // 图标固定白底，填充只在深灰和白色间变化；原厂DU只到黑白，灰阶仍用已有FOLLOW差分。
    // The icon well stays white; gray/white changes use existing FOLLOW diff, not binary-only native DU.
    epd_fill_circle(x0+UI_QUICK_SLIDER_WIDTH/2,UI_QUICK_SLIDER_Y1-54,32,UI_GRAY_WHITE,fb);
    ui_draw_icon(fb,x0+UI_QUICK_SLIDER_WIDTH/2,UI_QUICK_SLIDER_Y1-54,44,
        warmth?UI_ICON_CONTRAST:UI_ICON_SUN,UI_GRAY_BLACK);
}
void ui_quick_menu_draw(uint8_t *fb, bool wifi, bool bluetooth, bool frontlight, unsigned brightness, unsigned warmth) {
    if (!fb) return;
    const int width = epd_rotated_display_width();
    const int height = epd_rotated_display_height();
    // 每个像素只采一次原图，直接合成灰阶，不让主页快刷模式把菜单变成点阵。
    // Sample each underlay pixel once and compose grays directly, independently of fast main-page rendering.
    for (int y = 0; y < UI_QUICK_HEIGHT && y < height; ++y) for (int x = 0; x < width; ++x) {
        if (!layer_pixel(x, y, width)) continue;
        const unsigned tint = 238;
        uint8_t tone = (uint8_t)((pixel(fb, x, y) * 40u + tint * 216u + 128u) / 256u);
        epd_draw_pixel(x, y, tone, fb);
    }
    // 状态栏留清晰白底，沿用系统时间/签名/电池；四图标与底栏共用Lucide细线44px。
    // Keep the system time/signature/battery legible on white; share navigation's 44px Lucide line icons.
    ui_fill_rect_fast((EpdRect){0, 0, width, 76}, UI_GRAY_WHITE, fb);
    ui_nav_status(fb);
    for (int i = 0; i < 4; ++i) {
        ui_quick_menu_draw_button(fb,i,(i==0 && wifi)||(i==1 && bluetooth)||(i==3 && frontlight));
    }
    ui_quick_menu_draw_slider(fb,false,brightness);
    ui_quick_menu_draw_slider(fb,true,warmth);
    ui_fill_round_rect(fb, (EpdRect){width / 2 - 26, UI_QUICK_HEIGHT-24, 52, 6}, 3, 0x70);
    // 只画遮罩内的双像素轮廓，顶部直角、底部圆角，不越过下层边界。
    // Draw a two-pixel outline within the mask: square top, rounded bottom, no underlay leakage.
    for (int y = 0; y < UI_QUICK_HEIGHT && y < height; ++y) for (int x = 0; x < width; ++x) {
        if (layer_pixel(x, y, width) && (x < 2 || x >= width - 2 || y < 2 ||
            y >= UI_QUICK_HEIGHT - 2 || !layer_pixel(x - 2, y, width) ||
            !layer_pixel(x + 2, y, width) || !layer_pixel(x, y + 2, width)))
            epd_draw_pixel(x, y, 0x38, fb);
    }
}
