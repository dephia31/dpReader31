#include "metalio_touch_decode.h"
bool metalio_touch_decode(const uint8_t *raw,size_t size,metalio_touch_point_t points[2],unsigned *count) {
    if (!raw || !points || !count || size<3) return false;
    *count=0;
    unsigned contacts=raw[2]&15;
    if (contacts>2 || size<3+contacts*6) return false;
    for (unsigned i=0;i<contacts;++i) {
        const uint8_t *p=raw+3+6*i;
        unsigned event=p[0]>>6;
        if (event==1 || event==3) continue;
        unsigned x=((p[0]&15)<<8)|p[1],y=((p[2]&15)<<8)|p[3];
        // 同一设备始终报竖屏坐标，不能按单个点的 X 推断方向，否则右沿越界会跳到左侧。
        // This controller always reports portrait coordinates; inferring orientation from each X makes edge overshoot jump across the screen.
        // 原厂 b400605 已取消 -10px 偏移；盖板键仍保持原始坐标。
        // Vendor b400605 removes the -10px offset; cover keys retain their raw coordinates.
        if (!(y>=1570 && y<=1630)) {
            if (y>1225) continue;
            if (x>683) x=683;
            if (y>1215) y=1215;
        }
        points[(*count)++]=(metalio_touch_point_t){.x=x,.y=y,.id=p[2]>>4,.event=event};
    }
    return true;
}
