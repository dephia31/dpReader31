#pragma once
#include "epdiy.h"
typedef struct{uint8_t *front_fb,*back_fb; int temperature; void *driver;} EpdiyHighlevelState;
void epd_hl_set_all_white(EpdiyHighlevelState*);
uint8_t *epd_hl_get_framebuffer(EpdiyHighlevelState*);
