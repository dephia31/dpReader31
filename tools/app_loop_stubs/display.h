/* 中文：调度测试替身。/ English: Scheduler test shim. */
#pragma once
#include "common.h"

void display_request_full_refresh(void);
bool display_system_fast_set(bool enabled);
enum EpdDrawError update_display_main_turn(EpdiyHighlevelState* hl,EpdRect area,int dir);
static inline bool display_page_prepare_begin(void){return true;}
static inline void display_page_prepare_end(bool held){(void)held;}

enum EpdDrawError update_display_click_feedback(EpdiyHighlevelState* hl,EpdRect area);
