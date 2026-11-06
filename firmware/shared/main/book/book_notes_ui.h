/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include "book_layout.h"
#include "ui_gesture.h"
/* Bind in an event/load callback, never render. Borrowed text dies after reset. */
void book_notes_bind(const char *path,uint32_t chapter,const char *text,size_t length);
void book_notes_catalog(size_t count,bool (*title)(size_t,char*,size_t));
void book_notes_reset(void);
void book_notes_cancel(void);
bool book_notes_active(void);
bool book_notes_open_list(size_t page);
size_t book_notes_page(size_t fallback);
/* Event calls and draw must share the reader font lock. */
bool book_notes_gesture(app_ctx_t *ctx,const ui_gesture_event_t *ev,size_t page,EpdRect body,int px,app_redraw_t *redraw);
app_redraw_t book_notes_tick(void);
void book_notes_draw(uint8_t *fb,size_t page,EpdRect body,int px);
