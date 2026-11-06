/* SPDX-License-Identifier: LicenseRef-Kiikoread-Personal-Study-1.0
 * 中文：低亮度占半条滑轨；仅转换界面坐标，保留旧PWM/设置值。
 * English: Give low light half the track; map UI coordinates without changing stored PWM values.
 */
#pragma once
static inline unsigned ui_quick_brightness_value(unsigned position) {
    const unsigned range=UI_QUICK_SLIDER_RANGE, split=range/2, low=range/5;
    if(position>range)position=range;
    if(position<=split)return (position*low+split/2)/split;
    return low+((position-split)*(range-low)+(range-split)/2)/(range-split);
}
static inline unsigned ui_quick_brightness_position(unsigned value) {
    const unsigned range=UI_QUICK_SLIDER_RANGE, split=range/2, low=range/5;
    if(value>range)value=range;
    if(value<=low)return (value*split+low/2)/low;
    return split+((value-low)*(range-split)+(range-low)/2)/(range-low);
}
