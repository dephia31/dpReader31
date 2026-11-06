/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include "ui_kit.h"
typedef struct {
    bool open;
    int kind,count,current,start;
    const char *title;
    char error[80];
} ui_choice_t;
typedef void (*ui_choice_label_fn)(int kind,int index,char *out,size_t size);
enum {UI_CHOICE_CANCEL=-3,UI_CHOICE_NONE=-2,UI_CHOICE_PAGE=-1};
void ui_choice_begin(ui_choice_t *c,int kind,const char *title,int count,int current);
void ui_choice_draw(const ui_choice_t *c,uint8_t *fb,ui_choice_label_fn label);
int ui_choice_tap(ui_choice_t *c,int x,int y);
bool ui_choice_page(ui_choice_t *c,int direction);
EpdRect ui_choice_box(void);
EpdRect ui_choice_item(int slot);
int ui_choice_slots(void);
