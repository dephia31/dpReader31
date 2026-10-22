/* SPDX-License-Identifier: Apache-2.0
 * 中文：浮于清晰页面之上的液态玻璃快捷层，四个圆键沿用密码锁屏的折射与亮边。
 * English: A liquid-glass quick layer over the sharp page, sharing PIN-key refraction and highlights.
 * 用户修订：仅展开时生成玻璃；缓存四个小圆键直到关闭，输入只恢复原像素与圆内按压，避免重复模糊和顶部重刷。
 * User revision: generate glass only on open; cache four small keys until close and restore original pixels for local interior feedback without reblurring or refreshing the top panel.
 * S31：2x2 圆键加亮度、色温滑块，滑块整段拖动只提交一次。
 * S31: a 2x2 key grid plus brightness and warmth sliders; one drag commits the light once on release.
 * 冻结：不访问硬件、设置或刷新，不留整帧缓存；输入零分配，关闭释放。
 * Frozen: no hardware, settings, presentation or full-frame cache; input allocates nothing and close frees controls.
 */
#include "ui_quick_menu.h"
#include "ui_kit.h"
#include "ui_nav.h"
#include "ui_glass.h"
#include "esp_heap_caps.h"
#if KIIKO_BOARD_METALIO47
#include <string.h>
#endif

#if KIIKO_BOARD_METALIO47
enum { KEY_SIDE=2*UI_QUICK_RADIUS, KEY_BYTES=KEY_SIDE*KEY_SIDE/2, TOP_BYTES=UI_LOCK_WIDTH*UI_QUICK_HEIGHT/2 };
static uint8_t *s_underlay,*s_key_pixels,*s_frame;
static int s_rotation;
static const ui_icon_t icons[]={UI_ICON_WIFI,UI_ICON_BLUETOOTH,UI_ICON_REFRESH_CW,UI_ICON_LIGHTBULB};
static uint8_t pixel(const uint8_t *fb,int x,int y) { return (uint8_t)(ui_read_pixel(fb,x,y)*17u); }
static EpdRect key_rect(unsigned i) { return (EpdRect){UI_QUICK_CENTER_X(i)-UI_QUICK_RADIUS,UI_QUICK_CENTER_Y(i)-UI_QUICK_RADIUS,KEY_SIDE,KEY_SIDE}; }
EpdRect ui_quick_button_rect(unsigned i) { return key_rect(i); }
void ui_quick_menu_end(void) { heap_caps_free(s_underlay);heap_caps_free(s_key_pixels);s_underlay=s_key_pixels=s_frame=NULL; }
static void save_tile(const uint8_t *fb,EpdRect r,uint8_t *dst) {
    for(int y=0;y<r.height;++y)for(int x=0;x<r.width;++x){size_t at=(size_t)y*r.width+x;uint8_t v=ui_read_pixel(fb,r.x+x,r.y+y);if(at&1)dst[at/2]|=v<<4;else dst[at/2]=v;}
}
static void restore_tile(uint8_t *fb,EpdRect r,const uint8_t *saved) {
    for(int y=0;y<r.height;++y)for(int x=0;x<r.width;++x){size_t at=(size_t)y*r.width+x;epd_draw_pixel(r.x+x,r.y+y,(uint8_t)(((saved[at/2]>>((at&1)*4))&15)<<4),fb);}
}
bool ui_quick_menu_restore(uint8_t *fb) {
    if(!s_underlay||s_frame!=fb||s_rotation!=epd_get_rotation())return false;
    restore_tile(fb,(EpdRect){0,0,UI_LOCK_WIDTH,UI_QUICK_HEIGHT},s_underlay);return true;
}
static void restore_circle(uint8_t *fb,unsigned i) {
    int cx=UI_QUICK_CENTER_X(i),cy=UI_QUICK_CENTER_Y(i),rad=UI_QUICK_RADIUS;
    for(int y=cy-rad;y<cy+rad;++y)for(int x=cx-rad;x<cx+rad;++x){int dx=2*(x-cx)+1,dy=2*(y-cy)+1;if(dx*dx+dy*dy>4*rad*rad)continue;
        size_t at=(size_t)y*UI_LOCK_WIDTH+x;epd_draw_pixel(x,y,(uint8_t)(((s_underlay[at/2]>>((at&1)*4))&15)<<4),fb);}
}
static void solid_key(uint8_t *fb,unsigned i) {
    epd_fill_circle(UI_QUICK_CENTER_X(i),UI_QUICK_CENTER_Y(i),UI_QUICK_RADIUS-1,UI_GRAY_WHITE,fb);
    epd_draw_circle(UI_QUICK_CENTER_X(i),UI_QUICK_CENTER_Y(i),UI_QUICK_RADIUS-1,0x60,fb);
}
bool ui_quick_menu_draw_button(uint8_t *fb,unsigned i,bool enabled,bool pressed,EpdRect *dirty) {
    if(!fb||!dirty||i>=4)return false;
    EpdRect r=key_rect(i);int cx=UI_QUICK_CENTER_X(i),cy=UI_QUICK_CENTER_Y(i),rad=UI_QUICK_RADIUS;
    if(s_key_pixels&&s_frame==fb&&s_rotation==epd_get_rotation())restore_tile(fb,r,s_key_pixels+i*KEY_BYTES);else solid_key(fb,i);
    if(enabled)for(int y=-rad+3;y<rad-3;++y)for(int x=-rad+3;x<rad-3;++x)if(x*x+y*y<(rad-3)*(rad-3))
        epd_draw_pixel(cx+x,cy+y,(uint8_t)((pixel(fb,cx+x,cy+y)+3*UI_QUICK_FILL_DARK)/4),fb);
    ui_draw_icon(fb,cx,cy,44,icons[i],enabled?UI_GRAY_WHITE:UI_GRAY_BLACK);
    if(enabled){epd_fill_circle(cx+24,cy-24,5,UI_GRAY_WHITE,fb);epd_draw_circle(cx+24,cy-24,5,0x50,fb);}
    if(pressed){epd_draw_circle(cx,cy,rad-7,enabled?UI_GRAY_WHITE:UI_GRAY_BLACK,fb);epd_draw_circle(cx,cy,rad-8,enabled?UI_GRAY_WHITE:UI_GRAY_BLACK,fb);}
    *dirty=r;return true;
}
void ui_quick_menu_draw_status(uint8_t *fb) { ui_fill_rect_fast((EpdRect){0,0,UI_LOCK_WIDTH,76},UI_GRAY_WHITE,fb);ui_nav_status(fb); }
EpdRect ui_quick_slider_rect(bool warmth) {
    return (EpdRect){UI_QUICK_SLIDER_X(warmth)-2,UI_QUICK_SLIDER_Y0-2,
        UI_QUICK_SLIDER_WIDTH+4,UI_QUICK_SLIDER_Y1-UI_QUICK_SLIDER_Y0+4};
}
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
    (void)area; // Keep the glass outside the rounded capsule intact.
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
void ui_quick_menu_draw(uint8_t *fb,bool wifi,bool bluetooth,bool frontlight,unsigned brightness,unsigned warmth) {
    ui_quick_menu_end();
    if(!fb||epd_rotated_display_width()!=UI_LOCK_WIDTH||epd_rotated_display_height()<UI_QUICK_HEIGHT)return;
    s_underlay=heap_caps_malloc(TOP_BYTES,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);s_frame=fb;s_rotation=epd_get_rotation();
    if(s_underlay)save_tile(fb,(EpdRect){0,0,UI_LOCK_WIDTH,UI_QUICK_HEIGHT},s_underlay);
    EpdRect panel={18,88,UI_LOCK_WIDTH-36,280};
    if(!s_underlay||!ui_glass_panel(fb,panel,48,25,160)) { ui_fill_round_rect(fb,panel,48,0xe0);ui_draw_control_frame(fb,panel,48,0x60); }
    for(unsigned i=0;i<4;++i){if(s_underlay)restore_circle(fb,i);if(!s_underlay||!ui_glass_panel(fb,key_rect(i),UI_QUICK_RADIUS,25,160))solid_key(fb,i);}
    // Allocate key snapshots after every glass workspace has been released.
    s_key_pixels=heap_caps_malloc(4*KEY_BYTES,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(s_key_pixels)for(unsigned i=0;i<4;++i)save_tile(fb,key_rect(i),s_key_pixels+i*KEY_BYTES);
    for(unsigned i=0;i<4;++i){EpdRect dirty;ui_quick_menu_draw_button(fb,i,(i==0&&wifi)||(i==1&&bluetooth)||(i==3&&frontlight),false,&dirty);}
    ui_quick_menu_draw_slider(fb,false,brightness);ui_quick_menu_draw_slider(fb,true,warmth);
    ui_fill_round_rect(fb,(EpdRect){UI_LOCK_WIDTH/2-26,UI_QUICK_HEIGHT-24,52,6},3,0x70);
    ui_quick_menu_draw_status(fb);
}
#else
static int center_x(int button) { return (button * 2 + 1) * UI_LOCK_WIDTH / 8; }
static EpdRect glass_rect(void) { return (EpdRect){18,88,648,168}; }
enum { KEY_SIDE=2*UI_QUICK_RADIUS, KEY_BYTES=KEY_SIDE*KEY_SIDE/2, TOP_BYTES=UI_LOCK_WIDTH*UI_QUICK_HEIGHT/2 };
static uint8_t *s_underlay, *s_key_pixels, *s_key_frame;
static int s_key_rotation;
static const ui_icon_t s_icons[]={UI_ICON_WIFI,UI_ICON_BLUETOOTH,UI_ICON_REFRESH_CW,UI_ICON_LOCK};
static EpdRect key_rect(int button) {
    return (EpdRect){center_x(button)-UI_QUICK_RADIUS,UI_QUICK_CIRCLE_Y-UI_QUICK_RADIUS,KEY_SIDE,KEY_SIDE};
}
void ui_quick_menu_end(void) {
    heap_caps_free(s_underlay);heap_caps_free(s_key_pixels);s_underlay=s_key_pixels=s_key_frame=NULL;
}
static void draw_solid_key(uint8_t *fb,int button) {
    int cx=center_x(button);
    epd_fill_circle(cx,UI_QUICK_CIRCLE_Y,UI_QUICK_RADIUS-1,UI_GRAY_WHITE,fb);
    epd_draw_circle(cx,UI_QUICK_CIRCLE_Y,UI_QUICK_RADIUS-1,0x60,fb);
    epd_draw_circle(cx,UI_QUICK_CIRCLE_Y,UI_QUICK_RADIUS-2,0x60,fb);
}
static void draw_badge(uint8_t *fb,int button,bool active) {
    if(button>1||!active)return;
    int cx=center_x(button);
    epd_fill_circle(cx+30,UI_QUICK_CIRCLE_Y-30,5,UI_GRAY_WHITE,fb);
    epd_draw_circle(cx+30,UI_QUICK_CIRCLE_Y-30,5,0x50,fb);
}
bool ui_quick_menu_draw_button(uint8_t *fb,int button,bool active,bool pressed,EpdRect *dirty) {
    if(!fb||!dirty||button<0||button>=4||epd_rotated_display_width()<UI_LOCK_WIDTH||epd_rotated_display_height()<UI_QUICK_HEIGHT)return false;
    EpdRect r=key_rect(button);
    if(s_key_pixels&&s_key_frame==fb&&s_key_rotation==epd_get_rotation()) {
        const uint8_t *pixels=s_key_pixels+button*KEY_BYTES;
        for(int y=0;y<r.height;++y)for(int x=0;x<r.width;++x){
            size_t at=(size_t)y*r.width+x;
            epd_draw_pixel(r.x+x,r.y+y,(uint8_t)(((pixels[at/2]>>((at&1)*4))&15)<<4),fb);
        }
    } else {
        // 内存不足时圆内实心回退，不重算玻璃，也不在圆外刷白方框。
        // Low memory uses a solid circular fallback, never reblurring or whitening a square around it.
        draw_solid_key(fb,button);
        ui_draw_icon(fb,center_x(button),UI_QUICK_CIRCLE_Y,44,s_icons[button],UI_GRAY_BLACK);
    }
    draw_badge(fb,button,active);
    if(pressed){
        epd_draw_circle(center_x(button),UI_QUICK_CIRCLE_Y,UI_QUICK_RADIUS-7,UI_GRAY_BLACK,fb);
        epd_draw_circle(center_x(button),UI_QUICK_CIRCLE_Y,UI_QUICK_RADIUS-8,UI_GRAY_BLACK,fb);
    }
    *dirty=r;return true;
}
void ui_quick_menu_draw_status(uint8_t *fb) {
    ui_fill_rect_fast((EpdRect){0,0,UI_LOCK_WIDTH,76},UI_GRAY_WHITE,fb);
    ui_nav_status(fb);
}

// 只缓存顶部，展开直接取现有画面，收起精确还原；圆键也从清晰底图折射。
// Cache only the top: open from the existing frame, restore it exactly on close, and refract keys from sharp art.
static uint8_t *save_underlay(const uint8_t *fb) {
    uint8_t *saved=heap_caps_malloc(TOP_BYTES,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(!saved)return NULL;
    for(int y=0;y<UI_QUICK_HEIGHT;++y)for(int x=0;x<UI_LOCK_WIDTH;++x){
        size_t at=(size_t)y*UI_LOCK_WIDTH+x;
        uint8_t v=ui_read_pixel(fb,x,y);
        if(at&1)saved[at/2]|=(uint8_t)(v<<4);else saved[at/2]=v;
    }
    return saved;
}
bool ui_quick_menu_restore(uint8_t *fb) {
    if(!s_underlay||s_key_frame!=fb||s_key_rotation!=epd_get_rotation())return false;
    for(int y=0;y<UI_QUICK_HEIGHT;++y)for(int x=0;x<UI_LOCK_WIDTH;++x){
        size_t at=(size_t)y*UI_LOCK_WIDTH+x;
        epd_draw_pixel(x,y,(uint8_t)(((s_underlay[at/2]>>((at&1)*4))&15)<<4),fb);
    }
    return true;
}
static void restore_circle(uint8_t *fb,const uint8_t *saved,int cx) {
    const int cy=UI_QUICK_CIRCLE_Y,r=UI_QUICK_RADIUS;
    for(int y=cy-r;y<cy+r;++y)for(int x=cx-r;x<cx+r;++x){
        // 与玻璃圆角的半像素距离一致，圆外仍保留面板，不留下方形补丁。
        // Match the glass circle's half-pixel boundary, retaining the outer panel without square patches.
        int dx=2*(x-cx)+1,dy=2*(y-cy)+1;
        if(dx*dx+dy*dy>4*r*r)continue;
        size_t at=(size_t)y*UI_LOCK_WIDTH+x;
        epd_draw_pixel(x,y,(uint8_t)(((saved[at/2]>>((at&1)*4))&15)<<4),fb);
    }
}
void ui_quick_menu_draw(uint8_t *fb,bool wifi,bool bluetooth,bool soften_text) {
    ui_quick_menu_end();
    if(!fb||epd_rotated_display_width()<UI_LOCK_WIDTH||epd_rotated_display_height()<UI_QUICK_HEIGHT)return;
    EpdRect panel=glass_rect();
    uint8_t *underlay=s_underlay=save_underlay(fb);
    s_key_frame=fb;s_key_rotation=epd_get_rotation();
    if(!underlay||!ui_glass_panel_frost(fb,panel,56,25,160,soften_text)){
        ui_fill_round_rect(fb,panel,56,0xe0);
        ui_draw_control_frame(fb,panel,56,0x60);
    }
    for(int i=0;i<4;++i){
        int cx=center_x(i);
        if(underlay)restore_circle(fb,underlay,cx);
        EpdRect key=key_rect(i);
        if(!underlay||!ui_glass_panel_frost(fb,key,UI_QUICK_RADIUS,25,160,soften_text)){
            draw_solid_key(fb,i);
        }
        ui_draw_icon(fb,cx,UI_QUICK_CIRCLE_Y,44,s_icons[i],UI_GRAY_BLACK);
    }
    // 玻璃工作区已释放，再保存四圆键，峰值不叠加；不包含可变化的状态点和按压环。
    // Release glass workspaces before saving four keys, excluding mutable badges and press rings from the baseline.
    s_key_pixels=heap_caps_malloc(4*KEY_BYTES,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(s_key_pixels){
        s_key_frame=fb;s_key_rotation=epd_get_rotation();
        for(int i=0;i<4;++i){
            EpdRect r=key_rect(i);uint8_t *pixels=s_key_pixels+i*KEY_BYTES;
            for(int y=0;y<r.height;++y)for(int x=0;x<r.width;++x){
                size_t at=(size_t)y*r.width+x;uint8_t v=ui_read_pixel(fb,r.x+x,r.y+y);
                if(at&1)pixels[at/2]|=(uint8_t)(v<<4);else pixels[at/2]=v;
            }
        }
    }
    draw_badge(fb,0,wifi);draw_badge(fb,1,bluetooth);
    ui_fill_round_rect(fb,(EpdRect){UI_LOCK_WIDTH/2-26,240,52,6},3,0x70);
    // 状态栏保持清晰；其余底图直接透出，不给整个页面加模糊或白色蒙层。
    // Keep status crisp and the rest of the page exposed, without a full-page blur or white veil.
    ui_quick_menu_draw_status(fb);
}
#endif
