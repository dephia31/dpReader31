#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef void *cst836u_handle_t;
typedef struct{bool active;uint16_t x,y;uint8_t id;} cst836u_point_t;
typedef struct{bool touched;uint8_t count;uint16_t x,y; cst836u_point_t points[2];} cst836u_touch_t;
typedef struct{int dummy;} cst836u_info_t;
