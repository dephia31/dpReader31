#pragma once
#include "common.h"
unsigned metalio_brightness(void);
unsigned metalio_warmth(void);
bool metalio_frontlight_enabled(void);
unsigned metalio_frontlight_position(bool);
void metalio_frontlight_preview_position(unsigned,unsigned);
void metalio_frontlight_preview(unsigned,unsigned);
void metalio_frontlight_toggle(void);
void metalio_frontlight_commit(void);
esp_err_t metalio_haptic_click(void);
int metalio_take_volume(void);

enum {METALIO_AI_NONE,METALIO_AI_SHORT,METALIO_AI_LONG};
int metalio_take_ai(void);
void metalio_ai_clear(void);
