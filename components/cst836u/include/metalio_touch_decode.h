/* 中文：FT6336U 固定竖屏坐标；按原厂 b400605 取消 Y 偏移。
 * English: Fixed portrait FT6336U coordinates; no Y offset per vendor b400605. */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef struct { uint16_t x,y; uint8_t id,event; } metalio_touch_point_t;
bool metalio_touch_decode(const uint8_t *raw,size_t size,metalio_touch_point_t points[2],unsigned *count);
